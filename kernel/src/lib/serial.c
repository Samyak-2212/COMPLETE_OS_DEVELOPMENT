#include "serial.h"
#include "../hal/io.h"

/* 16550 UART registers */
#define DATA_REG(port)          (port + 0)
#define INTERRUPT_ENABLE(port)   (port + 1)
#define FIFO_CONTROL(port)       (port + 2)
#define LINE_CONTROL(port)       (port + 3)
#define MODEM_CONTROL(port)      (port + 4)
#define LINE_STATUS(port)        (port + 5)

int serial_init(void) {
    outb(INTERRUPT_ENABLE(COM1), 0x00);    /* Disable interrupts */
    outb(LINE_CONTROL(COM1), 0x80);        /* Set DLAB (Divisor Latch Access Bit) */
    outb(DATA_REG(COM1), 0x03);            /* Set divisor to 3 (38400 baud) lo-byte */
    outb(INTERRUPT_ENABLE(COM1), 0x00);    /* hi-byte */
    outb(LINE_CONTROL(COM1), 0x03);        /* 8 bits, no parity, one stop bit */
    outb(FIFO_CONTROL(COM1), 0xC7);        /* Enable FIFO, clear them, with 14-byte threshold */
    outb(MODEM_CONTROL(COM1), 0x0B);       /* IRQs enabled, RTS/DSR set */
    outb(MODEM_CONTROL(COM1), 0x1E);       /* Set in loopback mode, test the serial chip */
    outb(DATA_REG(COM1), 0xAE);            /* Test serial chip (send byte 0xAE and check if serial returns same byte) */

    /* Check if serial is faulty (i.e: not returning same byte as sent) */
    if(inb(DATA_REG(COM1)) != 0xAE) {
        return 1;
    }

    /* If serial is not faulty set it in normal operation mode
       (not-loopback with IRQs enabled and OUT#1 and OUT#2 bits set) */
    outb(MODEM_CONTROL(COM1), 0x0F);
    return 0;
}

int is_transmit_empty(void) {
    return inb(LINE_STATUS(COM1)) & 0x20;
}

void serial_putc(char c) {
    while (is_transmit_empty() == 0);
    outb(COM1, c);
}

void serial_print(const char *s) {
    while (*s) {
        serial_putc(*s++);
    }
}

int serial_received(void) {
    return inb(LINE_STATUS(COM1)) & 1;
}

char serial_getc(void) {
    while (serial_received() == 0);
    return inb(COM1);
}
