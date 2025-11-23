/*
 * AArch64 Bare Metal OS - UART Driver (PL011)
 *
 * PL011 UART driver for console output
 */

#ifndef _UART_H
#define _UART_H

#include "kernel.h"

/* PL011 UART Registers (offsets from base) */
#define UART_DR         0x00    /* Data Register */
#define UART_FR         0x18    /* Flag Register */
#define UART_IBRD       0x24    /* Integer Baud Rate Divisor */
#define UART_FBRD       0x28    /* Fractional Baud Rate Divisor */
#define UART_LCRH       0x2C    /* Line Control Register */
#define UART_CR         0x30    /* Control Register */
#define UART_IMSC       0x38    /* Interrupt Mask Set/Clear */
#define UART_ICR        0x44    /* Interrupt Clear Register */

/* Flag Register bits */
#define UART_FR_TXFF    (1 << 5)    /* Transmit FIFO Full */
#define UART_FR_RXFE    (1 << 4)    /* Receive FIFO Empty */

/* Line Control Register bits */
#define UART_LCRH_WLEN_8    (3 << 5)    /* 8-bit word length */
#define UART_LCRH_FEN       (1 << 4)    /* Enable FIFOs */

/* Control Register bits */
#define UART_CR_UARTEN  (1 << 0)    /* UART Enable */
#define UART_CR_TXE     (1 << 8)    /* Transmit Enable */
#define UART_CR_RXE     (1 << 9)    /* Receive Enable */

/* Function prototypes */
void uart_init(void);
void uart_putc(char c);
void uart_puts(const char *s);
void uart_puthex(uint64_t value);
char uart_getc(void);

#endif /* _UART_H */
