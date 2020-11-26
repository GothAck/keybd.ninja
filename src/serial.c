#include <drivers/uart.h>
#include <logging/log.h>
#include <sys/ring_buffer.h>

#include "serial.h"

LOG_MODULE_REGISTER(serial, LOG_LEVEL_INF);

const struct device *uart_dev;

struct ring_buf rxring;
uint8_t rxring_buf[256];
struct ring_buf txring;
uint8_t txring_buf[256];

static void interrupt_handler(const struct device *dev, void *user_data)
{
    ARG_UNUSED(user_data);

    while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {
        if (uart_irq_rx_ready(dev)) {
            int recv_len, rb_len;
            uint8_t buf[16];
            size_t len = MIN(ring_buf_space_get(&rxring), sizeof(buf));

            recv_len = uart_fifo_read(dev, buf, len);

            rb_len = ring_buf_put(&rxring, buf, recv_len);

            if (rb_len < recv_len) {
                LOG_WRN("Dropped %d rx bytes", recv_len - rb_len);
            }
        }
        if (uart_irq_tx_ready(dev)) {
            uint8_t buf[1];
            int rb_len, send_len;

            rb_len = ring_buf_get(&txring, buf, sizeof(buf));
            if (!rb_len) {
                uart_irq_tx_disable(dev);
                continue;
            }

            send_len = uart_fifo_fill(dev, buf, rb_len);

            if (send_len < rb_len) {
                LOG_WRN("Dropped %d tx bytes", rb_len - send_len);
            }
        }

    }
}


void uart_init() {
    ring_buf_init(&rxring, sizeof(rxring_buf), rxring_buf);
    ring_buf_init(&txring, sizeof(txring_buf), txring_buf);

	uart_dev = device_get_binding("UART_0");
	if (!uart_dev) {
		LOG_INF("No UART device");
		return;
	}

	uart_irq_callback_set(uart_dev, interrupt_handler);

	/* Enable rx interrupts */
	uart_irq_rx_enable(uart_dev);
}

int uart_send(uint8_t *data, uint16_t len) {
    int ret = ring_buf_put(&txring, data, len);
    uart_irq_tx_enable(uart_dev);
    return ret;
}