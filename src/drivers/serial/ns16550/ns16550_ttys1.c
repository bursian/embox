/**
 * @file
 *
 * @date May 19, 2023
 * @author Anton Bondarev
 */

#include <kernel/irq.h>

#include <util/macro.h>

#include <drivers/serial/uart_dev.h>
#include <drivers/ttys.h>

#include <drivers/common/memory.h>

#include <framework/mod/options.h>

#define UART_BASE      OPTION_GET(NUMBER, base_addr)
#define IRQ_NUM        OPTION_GET(NUMBER, irq_num)
#define BAUD_RATE      OPTION_GET(NUMBER,baud_rate)

#define TTY_NAME    ttyS1

extern irq_return_t uart_irq_handler(unsigned int irq_nr, void *data);

extern const struct uart_ops ns16550_uart_ops;

static struct uart ns16550_ttyS1 = {
		.uart_ops = &ns16550_uart_ops,
		.irq_num = IRQ_NUM,
		.base_addr = UART_BASE,
		.params =  {
				.baud_rate = BAUD_RATE,
				.uart_param_flags = UART_PARAM_FLAGS_8BIT_WORD | UART_PARAM_FLAGS_USE_IRQ,
		},
};

PERIPH_MEMORY_DEFINE(ns16550, UART_BASE, 0x1000);

STATIC_IRQ_ATTACH(IRQ_NUM, uart_irq_handler, &ns16550_ttyS1);

TTYS_DEF(MACRO_STRING(TTY_NAME), &ns16550_ttyS1);
