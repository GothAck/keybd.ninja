/** @file
 *  @brief HoG Service sample
 */

/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <sys/byteorder.h>
#include <zephyr.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>

#include <logging/log.h>

#include "hog.h"

LOG_MODULE_REGISTER(hog, LOG_LEVEL_INF);

enum {
	HIDS_REMOTE_WAKE = BIT(0),
	HIDS_NORMALLY_CONNECTABLE = BIT(1),
};

struct hids_info {
	uint16_t version; /* version number of base USB HID Specification */
	uint8_t code; /* country HID Device hardware is localized for. */
	uint8_t flags;
} __packed;

struct hids_report {
	uint8_t id; /* report id */
	uint8_t type; /* report type */
} __packed;

static struct hids_info info = {
	.version = 0x0101,
	.code = 0x00,
	.flags = HIDS_NORMALLY_CONNECTABLE,
};

enum {
	HIDS_INPUT = 0x01,
	HIDS_OUTPUT = 0x02,
	HIDS_FEATURE = 0x03,
};

static const char *manufacturer = "GothAck";
static const uint8_t pnp_id[] = {
    0x02,
    0x6d,
    0x04,
    0x5b,
    0xb3,
    0x11,
    0x00,
};

static uint8_t input_report[8] = {0};
static uint8_t output_report[20] = {0};
static uint8_t feature_report[2] = {0};

static struct hids_report input = {
	.id = 0x01,
	.type = HIDS_INPUT,
};

static struct hids_report output = {
    .id = 0x02,
    .type = HIDS_OUTPUT,
};

static struct hids_report feature = {
    .id = 0x03,
    .type = HIDS_FEATURE,
};

uint8_t simulate_input;
static uint8_t ctrl_point;
static uint8_t report_map[] = {
    0x05, 0x01,                 // Usage Page (Generic Desktop)
    0x09, 0x06,                 // Usage (Keyboard)
    0xA1, 0x01,                 // Collection (Application)
    0x05, 0x07,                 //     Usage Page (Key Codes)
    0x19, 0xe0,                 //     Usage Minimum (224)
    0x29, 0xe7,                 //     Usage Maximum (231)
    0x15, 0x00,                 //     Logical Minimum (0)
    0x25, 0x01,                 //     Logical Maximum (1)
    0x95, 0x08,                 //     Report Count (8)
    0x75, 0x01,                 //     Report Size (1)
    0x81, 0x02,                 //     Input (Data, Variable, Absolute)

    0x95, 0x01,                 //     Report Count (1)
    0x75, 0x08,                 //     Report Size (8)
    0x81, 0x01,                 //     Input (Constant) reserved byte(1)

    0x95, 0x05,                 //     Report Count (5)
    0x75, 0x01,                 //     Report Size (1)
    0x05, 0x08,                 //     Usage Page (Page# for LEDs)
    0x19, 0x01,                 //     Usage Minimum (1)
    0x29, 0x05,                 //     Usage Maximum (5)
    0x91, 0x02,                 //     Output (Data, Variable, Absolute), Led report
    0x95, 0x01,                 //     Report Count (1)
    0x75, 0x03,                 //     Report Size (3)
    0x91, 0x01,                 //     Output (Data, Variable, Absolute), Led report padding

    0x95, 0x06,                 //     Report Count (6)
    0x75, 0x08,                 //     Report Size (8)
    0x15, 0x00,                 //     Logical Minimum (0)
    0x25, 0x65,                 //     Logical Maximum (101)
    0x05, 0x07,                 //     Usage Page (Key codes)
    0x19, 0x00,                 //     Usage Minimum (0)
    0x29, 0x65,                 //     Usage Maximum (101)
    0x81, 0x00,                 //     Input (Data, Array) Key array(6 bytes)

    0x09, 0x05,                 //     Usage (Vendor Defined)
    0x15, 0x00,                 //     Logical Minimum (0)
    0x26, 0xFF, 0x00,           //     Logical Maximum (255)
    0x95, 0x02,                 //     Report Count (2)
    0x75, 0x08,                 //     Report Size (8 bit)
    0xB1, 0x02,                 //     Feature (Data, Variable, Absolute)

    0xC0,                       // End Collection (Application)
};

static ssize_t read_info(struct bt_conn *conn,
			  const struct bt_gatt_attr *attr, void *buf,
			  uint16_t len, uint16_t offset)
{
    LOG_INF("read_info");
	return bt_gatt_attr_read(conn, attr, buf, len, offset, attr->user_data,
				 sizeof(struct hids_info));
}

static ssize_t read_dis_pnp_id(struct bt_conn *conn,
			  const struct bt_gatt_attr *attr, void *buf,
			  uint16_t len, uint16_t offset)
{
    LOG_INF("read_dis_pnp_id");
	return bt_gatt_attr_read(conn, attr, buf, len, offset, attr->user_data,
				 sizeof(pnp_id));
}

static ssize_t read_dis_manu_name(struct bt_conn *conn,
			  const struct bt_gatt_attr *attr, void *buf,
			  uint16_t len, uint16_t offset)
{
    LOG_INF("read_dis_manu_name");
	return bt_gatt_attr_read(conn, attr, buf, len, offset, manufacturer,
				 strlen(manufacturer));
}

static uint8_t bas_level = 0x32;

static ssize_t read_bas_level(struct bt_conn *conn,
			  const struct bt_gatt_attr *attr, void *buf,
			  uint16_t len, uint16_t offset)
{
    LOG_INF("read_dis_manu_name");
	return bt_gatt_attr_read(conn, attr, buf, len, offset, attr->user_data,
				 sizeof(bas_level));
}

static ssize_t read_report_map(struct bt_conn *conn,
			       const struct bt_gatt_attr *attr, void *buf,
			       uint16_t len, uint16_t offset)
{
    LOG_INF("read_report_map");
	return bt_gatt_attr_read(conn, attr, buf, len, offset, report_map,
				 sizeof(report_map));
}

static ssize_t read_report(struct bt_conn *conn,
			   const struct bt_gatt_attr *attr, void *buf,
			   uint16_t len, uint16_t offset)
{
    struct hids_report *report = (struct hids_report *)attr->user_data;
    LOG_INF("read_report %d %d", report->id, report->type);
	return bt_gatt_attr_read(conn, attr, buf, len, offset, attr->user_data,
				 sizeof(struct hids_report));
}

static void input_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    LOG_INF("input_ccc_changed %x", value);
	simulate_input = (value == BT_GATT_CCC_NOTIFY) ? 1 : 0;
}

static void output_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    LOG_INF("output_ccc_changed %x", value);
}

static ssize_t read_input_report(struct bt_conn *conn,
				 const struct bt_gatt_attr *attr, void *buf,
				 uint16_t len, uint16_t offset)
{
    LOG_INF("read_input_report");
	return bt_gatt_attr_read(conn, attr, buf, len, offset, attr->user_data, sizeof(input_report));
}

static ssize_t read_output_report(struct bt_conn *conn,
				 const struct bt_gatt_attr *attr, void *buf,
				 uint16_t len, uint16_t offset)
{
    LOG_INF("read_output_report");
	return bt_gatt_attr_read(conn, attr, buf, len, offset, attr->user_data, sizeof(output_report));
}

static ssize_t write_output_report(struct bt_conn *conn,
				const struct bt_gatt_attr *attr,
				const void *buf, uint16_t len, uint16_t offset,
				uint8_t flags)
{
    LOG_INF("write_output_report %d %d", len, offset);
	uint8_t *value = attr->user_data;

	if (offset + len > sizeof(output_report)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	memcpy(value + offset, buf, len);

    if (output_report[0] & 0x01) {
        LOG_INF("Num lock ON");
    } else {
        LOG_INF("Num lock OFF");
    }
    if (output_report[0] & 0x02) {
        LOG_INF("Caps lock ON");
    } else {
        LOG_INF("Caps lock OFF");
    }

	return len;
}

static ssize_t read_feature_report(struct bt_conn *conn,
				 const struct bt_gatt_attr *attr, void *buf,
				 uint16_t len, uint16_t offset)
{
    LOG_INF("read_feature_report");
	return bt_gatt_attr_read(conn, attr, buf, len, offset, attr->user_data, sizeof(feature_report));
}

static ssize_t write_feature_report(struct bt_conn *conn,
				const struct bt_gatt_attr *attr,
				const void *buf, uint16_t len, uint16_t offset,
				uint8_t flags)
{
    LOG_INF("write_feature_report %d %d", len, offset);
	uint8_t *value = attr->user_data;

	if (offset + len > sizeof(feature_report)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	memcpy(value + offset, buf, len);

	return len;
}

static ssize_t write_ctrl_point(struct bt_conn *conn,
				const struct bt_gatt_attr *attr,
				const void *buf, uint16_t len, uint16_t offset,
				uint8_t flags)
{
    LOG_INF("write_ctrl_point");
	uint8_t *value = attr->user_data;

	if (offset + len > sizeof(ctrl_point)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	memcpy(value + offset, buf, len);

	return len;
}

/* Device Info Service Declaration */
BT_GATT_SERVICE_DEFINE(dis_svc,
	BT_GATT_PRIMARY_SERVICE(BT_UUID_DIS),
	BT_GATT_CHARACTERISTIC(BT_UUID_DIS_PNP_ID, BT_GATT_CHRC_READ,
			       BT_GATT_PERM_READ, read_dis_pnp_id, NULL, &pnp_id),
	BT_GATT_CHARACTERISTIC(BT_UUID_DIS_MANUFACTURER_NAME, BT_GATT_CHRC_READ,
			       BT_GATT_PERM_READ, read_dis_manu_name, NULL, NULL),
);

/* Battery Service Declaration */
BT_GATT_SERVICE_DEFINE(bas_svc,
	BT_GATT_PRIMARY_SERVICE(BT_UUID_BAS),
	BT_GATT_CHARACTERISTIC(BT_UUID_BAS_BATTERY_LEVEL, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
			       BT_GATT_PERM_READ, read_bas_level, NULL, &bas_level),
	BT_GATT_CCC(NULL,
		    BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

/* HID Service Declaration */
BT_GATT_SERVICE_DEFINE(hog_svc,
	BT_GATT_PRIMARY_SERVICE(BT_UUID_HIDS),
	BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_INFO, BT_GATT_CHRC_READ,
			       BT_GATT_PERM_READ, read_info, NULL, &info),
	BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT_MAP, BT_GATT_CHRC_READ,
			       BT_GATT_PERM_READ, read_report_map, NULL, NULL),
	BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
			       BT_GATT_PERM_READ_ENCRYPT,
			       read_input_report, NULL, &input_report),
	BT_GATT_CCC(input_ccc_changed,
		    BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT),
	BT_GATT_DESCRIPTOR(BT_UUID_HIDS_REPORT_REF, BT_GATT_PERM_READ,
			   read_report, NULL, &input),
	BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
			       BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT,
			       read_output_report, write_output_report, &output_report),
	BT_GATT_DESCRIPTOR(BT_UUID_HIDS_REPORT_REF, BT_GATT_PERM_READ,
			   read_report, NULL, &output),
	BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
			       BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT,
			       read_feature_report, write_feature_report, &feature_report),
	BT_GATT_DESCRIPTOR(BT_UUID_HIDS_REPORT_REF, BT_GATT_PERM_READ,
			   read_report, NULL, &feature),
	BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_CTRL_POINT,
			       BT_GATT_CHRC_WRITE_WITHOUT_RESP,
			       BT_GATT_PERM_WRITE,
			       NULL, write_ctrl_point, &ctrl_point),
);

void hog_input_keys(uint8_t k0) {
    input_report[2] = k0;
    int err = bt_gatt_notify(NULL, &hog_svc.attrs[5], &input_report, sizeof(input_report));
    if (err)
        LOG_ERR("Gatt notify error %d", err);
}

void hog_init(void)
{
}

