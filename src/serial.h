#pragma once

extern struct ring_buf rxring;
extern struct ring_buf txring;

void uart_init();

int uart_send(uint8_t *data, uint16_t len);