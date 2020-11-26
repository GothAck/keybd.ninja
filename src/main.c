#include <zephyr.h>

#include <settings/settings.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>

#include <drivers/uart.h>
#include <drivers/led.h>

#include <drivers/gpio.h>
#include <logging/log.h>

#include <sys/ring_buffer.h>

#include "hog.h"
#include "usb.h"
#include "serial.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL,
		      0x12, 0x18, /* HID Service */
		      0x0f, 0x18), /* Battery Service */
};

const struct device *gpio_dev;

static void connected(struct bt_conn *conn, uint8_t err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (err) {
		LOG_ERR("Failed to connect to %s (%u)", log_strdup(addr), err);
		return;
	}

	LOG_INF("Connected %s", log_strdup(addr));

	if (bt_conn_set_security(conn, BT_SECURITY_L2)) {
		LOG_ERR("Failed to set security");
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Disconnected from %s (reason 0x%02x)", log_strdup(addr), reason);
}

static void security_changed(struct bt_conn *conn, bt_security_t level,
			     enum bt_security_err err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (!err) {
		LOG_INF("Security changed: %s level %u", log_strdup(addr), level);
	} else {
		LOG_ERR("Security failed: %s level %u err %d", log_strdup(addr), level,
		       err);
	}
}

static struct bt_conn_cb conn_callbacks = {
	.connected = connected,
	.disconnected = disconnected,
	.security_changed = security_changed,
};

void bt_ready(int err) {
    if (err) {
        LOG_ERR("Bluetooth init failed %d", err);
        return;
    }
    
    hog_init();

    // Load settings - including pairing info
    settings_load();

    LOG_INF("Bluetooth initialized");


    err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		LOG_ERR("Advertising failed to start (err %d)", err);
		return;
	}

    LOG_INF("Advertising successfully started");
}

static void auth_passkey_display(struct bt_conn *conn, unsigned int passkey)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Passkey for %s: %06u", log_strdup(addr), passkey);
}

static void auth_cancel(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Pairing cancelled: %s", log_strdup(addr));
}

static struct bt_conn_auth_cb auth_cb_display = {
	.passkey_display = auth_passkey_display,
	.passkey_entry = NULL,
	.cancel = auth_cancel,
};

struct k_timer my_timer;

uint8_t toggle = 0;

void my_timer_expiry(struct k_timer *timer_id) {
	const char *data = "Hello\r\n";
	int ret;

	// ret = uart_send((uint8_t *)data, strlen(data));

	// if (ret <= 0) {
	// 	LOG_ERR("Failed to uart_send %d", ret);
	// }
	// if (toggle) {
	// 	hog_input_keys(0x04);
	// 	usb_input_keys(0x04);
	// } else {
	// 	hog_input_keys(0x00);
	// 	usb_input_keys(0x00);
	// }
	// toggle = ! toggle;
}

void scan_thread() {
	while (true) {
		// hog_input_keys(0x53);
		// usb_input_keys(0x53);
		// k_sleep(K_MSEC(10));
		// hog_input_keys(0x00);
		// usb_input_keys(0x00);
		k_sleep(K_MSEC(5000));
	}
}

void main(void) {
    int err;

	uart_init();

    if ((err = bt_enable(bt_ready))) {
        LOG_INF("Bluetooth init failed %d", err);
        return;
    }

	bt_conn_cb_register(&conn_callbacks);
	bt_conn_auth_cb_register(&auth_cb_display);

	usb_init();

	gpio_dev = device_get_binding("GPIO_0");
	if (!gpio_dev) {
		LOG_INF("No GPIO device");
		return;
	}
	gpio_pin_configure(gpio_dev, 22, GPIO_OUTPUT); //p0.22 == Green
	gpio_pin_configure(gpio_dev, 23, GPIO_OUTPUT); //p0.23 == Red
	gpio_pin_configure(gpio_dev, 24, GPIO_OUTPUT); //p0.24 == Blue

	gpio_pin_set(gpio_dev, 22, 1);
	gpio_pin_set(gpio_dev, 23, 1);
	// gpio_pin_set(gpio_dev, 24, 1);

    k_timer_init(&my_timer, my_timer_expiry, NULL);
    k_timer_start(&my_timer, K_MSEC(1000), K_MSEC(1000));

	int timer = 0;
	while(true) {
		k_sleep(K_MSEC(1));
		if (!ring_buf_is_empty(&rxring)) {
			uint8_t buf[16];
			int len;

			len = ring_buf_get(&rxring, buf, sizeof(buf));
			if (len <= 0) continue;
			uart_send(buf, len);
		}
	}
}

K_THREAD_DEFINE(scan_thread_id, 1024, scan_thread, NULL, NULL, NULL, 7, 0, 0);