#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>

/* COM1 address */
#define COM1 0x3F8

/* Serial initialization */
int serial_init(void);

/* Basic I/O */
void serial_putc(char c);
void serial_print(const char *s);
char serial_getc(void);

#endif /* SERIAL_H */
