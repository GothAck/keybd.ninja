#include <usb/usb_device.h>
#include <usb/class/usb_hid.h>
#include <usb/class/usb_cdc.h>

#include <logging/log.h>

#include "usb.h"
#include "hid.h"

LOG_MODULE_REGISTER(usb, LOG_LEVEL_INF);

static const uint8_t hid_kbd_report_desc[] = HID_KEYBOARD_REPORT_DESC();

const struct device *hid0_dev, *cdc0_dev;

struct ring_buf rxring;
uint8_t rxring_buf[sizeof(struct hid_report) * 32];
static K_SEM_DEFINE(usb_sem, 1, 1);	/* starts off "available" */

static void in_ready_cb(const struct device *dev)
{
	ARG_UNUSED(dev);

	k_sem_give(&usb_sem);
}

static int in_set_report_cb(const struct device *dev, struct usb_setup_packet *setup, int32_t *len, uint8_t **data) {
	LOG_INF("Set report %d %d %d %d %d", setup->bmRequestType, setup->bRequest, setup->wValue, setup->wIndex, setup->wLength);
	LOG_INF("Set report %d", *len);
	for (int i = 0; i < *len; ++i) {
		LOG_INF("%02x", *data[i]);
	}
	return 0;
}

static const struct hid_ops ops = {
	.set_report = in_set_report_cb,
	.int_in_ready = in_ready_cb,
};

static void status_cb(enum usb_dc_status_code status, const uint8_t *param)
{
	LOG_INF("Status %d", status);
}

void usb_init() {
    int ret;

    hid0_dev = device_get_binding("HID_0");
	if (hid0_dev == NULL) {
		LOG_ERR("Cannot get USB HID 0 Device");
		return;
	}

    cdc0_dev = device_get_binding("CDC_ACM_0");
	if (cdc0_dev == NULL) {
		LOG_ERR("Cannot get USB CDC 0 Device");
		return;
	}

	usb_hid_register_device(hid0_dev, hid_kbd_report_desc,
				sizeof(hid_kbd_report_desc), &ops);

	ret = usb_hid_init(hid0_dev);
	if (ret != 0) {
		LOG_ERR("Failed to enable USB HID %d", ret);
		return;
	}

	ret = usb_enable(status_cb);
	if (ret != 0) {
		LOG_ERR("Failed to enable USB %d", ret);
		return;
	}

    LOG_INF("USB Initialized");
}

void usb_loop() {

}

void usb_input_keys(struct hid_report report) {
    hid_int_ep_write(hid0_dev, (const uint8_t *)&report, sizeof(report), NULL);
}