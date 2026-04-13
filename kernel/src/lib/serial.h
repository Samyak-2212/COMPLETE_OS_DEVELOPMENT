#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>

/* COM1 address */
#define COM1 0x3F8

#ifdef DEBUGGER_ENABLED
/* Serial initialization */
int serial_init(void);

/* Basic I/O */
void serial_putc(char c);
void serial_print(const char *s);
char serial_getc(void);
#else
static inline int  serial_init(void)     { return 0; }
static inline void serial_putc(char c)    { (void)c;  }
static inline void serial_print(const char *s) { (void)s; }
static inline char serial_getc(void)     { return 0; }
#endif

#endif /* SERIAL_H */
