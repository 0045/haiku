/*
 * Copyright (c) 2008 Travis Geiselbrecht
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */


#include <debug.h>
#include <arch/arm/reg.h>
#include <arch/arm/uart.h>
#include <board_config.h>
//#include <target/debugconfig.h>


#define DEBUG_UART BOARD_DEBUG_UART


struct uart_stat {
	addr_t base;
	uint shift;
};


static struct uart_stat uart[3] = {
	{ BOARD_UART1_BASE, 2 },
	{ BOARD_UART2_BASE, 2 },
	{ BOARD_UART3_BASE, 2 },
};


static inline void write_uart_reg(int port, uint reg, unsigned char data)
{
	*(volatile unsigned char *)(uart[port].base + (reg << uart[port].shift))
		= data;
}


static inline unsigned char read_uart_reg(int port, uint reg)
{
	return *(volatile unsigned char *)(uart[port].base
		+ (reg << uart[port].shift));
}


#define LCR_8N1		0x03

#define FCR_FIFO_EN	0x01	/* Fifo enable */
#define FCR_RXSR	0x02	/* Receiver soft reset */
#define FCR_TXSR	0x04	/* Transmitter soft reset */

#define MCR_DTR		0x01
#define MCR_RTS		0x02
#define MCR_DMA_EN	0x04
#define MCR_TX_DFR	0x08

#define LCR_WLS_MSK	0x03	/* character length select mask */
#define LCR_WLS_5	0x00	/* 5 bit character length */
#define LCR_WLS_6	0x01	/* 6 bit character length */
#define LCR_WLS_7	0x02	/* 7 bit character length */
#define LCR_WLS_8	0x03	/* 8 bit character length */
#define LCR_STB		0x04	/* Number of stop Bits, off = 1, on = 1.5 or 2) */
#define LCR_PEN		0x08	/* Parity eneble */
#define LCR_EPS		0x10	/* Even Parity Select */
#define LCR_STKP	0x20	/* Stick Parity */
#define LCR_SBRK	0x40	/* Set Break */
#define LCR_BKSE	0x80	/* Bank select enable */

#define LSR_DR		0x01	/* Data ready */
#define LSR_OE		0x02	/* Overrun */
#define LSR_PE		0x04	/* Parity error */
#define LSR_FE		0x08	/* Framing error */
#define LSR_BI		0x10	/* Break */
#define LSR_THRE	0x20	/* Xmit holding register empty */
#define LSR_TEMT	0x40	/* Xmitter empty */
#define LSR_ERR		0x80	/* Error */


int uart_debug_port(void)
{
	return DEBUG_UART;
}


void uart_init_port(int port, uint baud)
{
	uint16 baudDivisor = BOARD_UART_CLOCK / (16 * baud);

	// Write standard uart settings
	write_uart_reg(port, UART_LCR, LCR_8N1);
		// 8N1
	write_uart_reg(port, UART_IER, 0);
		// Disable interrupt
	write_uart_reg(port, UART_FCR, 0);
		// Disable FIFO
	write_uart_reg(port, UART_MCR, MCR_DTR | MCR_RTS);
		// DTR / RTS

	// Gain access to, and program baud divisor
	unsigned char buffer = read_uart_reg(port, UART_LCR);
	write_uart_reg(port, UART_LCR, buffer | LCR_BKSE);
	write_uart_reg(port, UART_DLL, baudDivisor & 0xff);
	write_uart_reg(port, UART_DLH, (baudDivisor >> 8) & 0xff);
	write_uart_reg(port, UART_LCR, buffer & ~LCR_BKSE);

//	write_uart_reg(port, UART_MDR1, 0); // UART 16x mode
//	write_uart_reg(port, UART_LCR, 0xBF); // config mode B
//	write_uart_reg(port, UART_EFR, (1<<7)|(1<<6)); // hw flow control
//	write_uart_reg(port, UART_LCR, LCR_8N1); // operational mode
}


void uart_init_early(void)
{
	// Perform special hardware UART configuration

	#if BOARD_CPU_OMAP3
	/* UART1 */
	RMWREG32(CM_FCLKEN1_CORE, 13, 1, 1);
	RMWREG32(CM_ICLKEN1_CORE, 13, 1, 1);

	/* UART2 */
	RMWREG32(CM_FCLKEN1_CORE, 14, 1, 1);
	RMWREG32(CM_ICLKEN1_CORE, 14, 1, 1);

	/* UART3 */
	RMWREG32(CM_FCLKEN_PER, 11, 1, 1);
	RMWREG32(CM_ICLKEN_PER, 11, 1, 1);
	#else
	#warning INTITIALIZE UART!!!!!
	#endif
}


void uart_init(void)
{
	uart_init_port(uart_debug_port(), 115200);
}


int uart_putc(int port, char c )
{
	while (!(read_uart_reg(port, UART_LSR) & (1<<6)));
		// wait for the last char to get out
	write_uart_reg(port, UART_THR, c);
	return 0;
}


int uart_getc(int port, bool wait)  /* returns -1 if no data available */
{
	if (wait) {
		while (!(read_uart_reg(port, UART_LSR) & (1<<0)));
			// wait for data to show up in the rx fifo
	} else {
		if (!(read_uart_reg(port, UART_LSR) & (1<<0)))
			return -1;
	}
	return read_uart_reg(port, UART_RHR);
}


void uart_flush_tx(int port)
{
	while (!(read_uart_reg(port, UART_LSR) & (1<<6)));
		// wait for the last char to get out
}


void uart_flush_rx(int port)
{
	// empty the rx fifo
	while (read_uart_reg(port, UART_LSR) & (1<<0)) {
		volatile char c = read_uart_reg(port, UART_RHR);
		(void)c;
	}
}
