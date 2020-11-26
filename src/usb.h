#include "hid.h"

#pragma once

void usb_init();

void usb_loop();

void usb_input_keys(struct hid_report report);

