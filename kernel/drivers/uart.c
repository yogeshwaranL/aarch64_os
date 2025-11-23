/*
 * AArch64 Bare Metal OS - UART Driver Implementation
 */

#include "kernel.h"
#include "uart.h"

/* Read from UART register */
static inline uint32_t uart_read(uint32_t offset)
{
    return *(volatile uint32_t *)(UART0_BASE + offset);
}

/* Write to UART register */
static inline void uart_write(uint32_t offset, uint32_t value)
{
    *(volatile uint32_t *)(UART0_BASE + offset) = value;
}

/*
 * Initialize UART
 *
 * Note: In QEMU, UART is already initialized by UEFI
 * This function ensures it's in a known state
 */
void uart_init(void)
{
    /* Disable UART */
    uart_write(UART_CR, 0);

    /* Clear all interrupts */
    uart_write(UART_ICR, 0x7FF);

    /*
     * Set baud rate (115200)
     * UARTCLK = 24MHz (QEMU default)
     * Baud divisor = UARTCLK / (16 * baud_rate)
     *              = 24000000 / (16 * 115200)
     *              = 13.02 (13 + 0.02)
     * IBRD = 13, FBRD = int(0.02 * 64 + 0.5) = 1
     */
    uart_write(UART_IBRD, 13);
    uart_write(UART_FBRD, 1);

    /* Set line control: 8 bits, FIFOs enabled, no parity */
    uart_write(UART_LCRH, UART_LCRH_WLEN_8 | UART_LCRH_FEN);

    /* Mask all interrupts */
    uart_write(UART_IMSC, 0);

    /* Enable UART, TX, and RX */
    uart_write(UART_CR, UART_CR_UARTEN | UART_CR_TXE | UART_CR_RXE);
}

/*
 * Write a single character to UART
 */
void uart_putc(char c)
{
    /* Wait until TX FIFO is not full */
    while (uart_read(UART_FR) & UART_FR_TXFF)
        ;

    /* Write character */
    uart_write(UART_DR, (uint32_t)c);

    /* Handle newline: also send carriage return */
    if (c == '\n') {
        while (uart_read(UART_FR) & UART_FR_TXFF)
            ;
        uart_write(UART_DR, '\r');
    }
}

/*
 * Write a string to UART
 */
void uart_puts(const char *s)
{
    while (*s) {
        uart_putc(*s++);
    }
}

/*
 * Write a hexadecimal number to UART
 */
void uart_puthex(uint64_t value)
{
    const char hex_chars[] = "0123456789ABCDEF";
    char buffer[19]; /* "0x" + 16 hex digits + null */
    int i;

    buffer[0] = '0';
    buffer[1] = 'x';

    /* Convert to hex (big-endian) */
    for (i = 0; i < 16; i++) {
        buffer[2 + i] = hex_chars[(value >> (60 - i * 4)) & 0xF];
    }
    buffer[18] = '\0';

    uart_puts(buffer);
}

/*
 * Read a single character from UART
 */
char uart_getc(void)
{
    /* Wait until RX FIFO is not empty */
    while (uart_read(UART_FR) & UART_FR_RXFE)
        ;

    /* Read and return character */
    return (char)uart_read(UART_DR);
}
