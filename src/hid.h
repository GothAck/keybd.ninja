#include <zephyr.h>
#include <usb/class/usb_hid.h>

#pragma once

struct hid_report {
    uint8_t modifier;
    uint8_t unused;
    uint8_t codes[6];
} __packed;