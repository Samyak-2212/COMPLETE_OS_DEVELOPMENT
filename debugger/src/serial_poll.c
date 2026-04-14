#include "debugger.h"
#include <hal/io.h>

#define COM1 0x3F8

static bool serial_initialized = false;

void debugger_serial_init(void) {
    if (serial_initialized) return;

    outb(COM1 + 1, 0x00);    // Disable all interrupts
    outb(COM1 + 3, 0x80);    // Enable DLAB (set baud rate divisor)
    outb(COM1 + 0, 0x01);    // Set divisor to 1 (lo byte) 115200 baud
    outb(COM1 + 1, 0x00);    //                  (hi byte)
    outb(COM1 + 3, 0x03);    // 8 bits, no parity, one stop bit
    outb(COM1 + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
    outb(COM1 + 4, 0x0B);    // IRQs enabled, RTS/DSR set
    
    serial_initialized = true;
}

static int is_transmit_empty(void) {
    return inb(COM1 + 5) & 0x20;
}

void debugger_serial_putc(char c) {
    while (is_transmit_empty() == 0);
    outb(COM1, c);
}

static int serial_received(void) {
    return inb(COM1 + 5) & 1;
}

char debugger_serial_getc(void) {
    while (serial_received() == 0);
    return inb(COM1);
}

bool debugger_serial_has_data(void) {
    return serial_received() != 0;
}

void debugger_serial_print(const char *s) {
    while (*s) {
        debugger_serial_putc(*s++);
    }
}
