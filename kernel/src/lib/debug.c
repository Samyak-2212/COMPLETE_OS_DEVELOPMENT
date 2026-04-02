#include "debug.h"
#include "serial.h"
#include <stdarg.h>

/* Simple internal vsnprintf-like for serial */
static void serial_vprintf(const char *fmt, va_list args);

static void print_uint_serial(uint64_t val, int base) {
    char buf[32];
    int i = 0;
    const char *digits = "0123456789abcdef";
    if (val == 0) serial_putc('0');
    while (val > 0) {
        buf[i++] = digits[val % base];
        val /= base;
    }
    while (--i >= 0) serial_putc(buf[i]);
}

static int debug_mode = DEBUG_MODE_JSON;

static const char* level_to_str(debug_level_t level) {
    switch (level) {
        case DEBUG_LEVEL_INFO:  return "INFO";
        case DEBUG_LEVEL_WARN:  return "WARN";
        case DEBUG_LEVEL_ERROR: return "ERROR";
        case DEBUG_LEVEL_PANIC: return "PANIC";
        default:                return "UNKNOWN";
    }
}

void debug_set_mode(int mode) {
    debug_mode = mode;
}

void debug_init(void) {
    serial_init();
    debug_log(DEBUG_LEVEL_INFO, "DEBUG", "Debugger Initialized");
}

void debug_log(debug_level_t level, const char *component, const char *fmt, ...) {
    if (debug_mode == DEBUG_MODE_HR) {
        serial_print("[");
        serial_print(level_to_str(level));
        serial_print("] [");
        serial_print(component);
        serial_print("] ");
        
        va_list args;
        va_start(args, fmt);
        serial_vprintf(fmt, args);
        va_end(args);
        serial_print("\n");
    } else {
        serial_print("{\"type\":\"log\",\"level\":\"");
        serial_print(level_to_str(level));
        serial_print("\",\"comp\":\"");
        serial_print(component);
        serial_print("\",\"msg\":\"");
        
        va_list args;
        va_start(args, fmt);
        serial_vprintf(fmt, args);
        va_end(args);
        
        serial_print("\"}\n");
    }
}

void debug_dump_mem(const void *addr, size_t len) {
    if (debug_mode == DEBUG_MODE_HR) {
        const uint8_t *p = (const uint8_t *)addr;
        debug_log(DEBUG_LEVEL_INFO, "MEMDUMP", "Dumping %d bytes at %p", (int)len, addr);
        for (size_t i = 0; i < len; i++) {
            if (i % 16 == 0) {
                if (i > 0) serial_print("\n");
                serial_print("  ");
                print_uint_serial((uintptr_t)(p + i), 16);
                serial_print(": ");
            }
            const char *hex = "0123456789ABCDEF";
            serial_putc(hex[(p[i] >> 4) & 0xF]);
            serial_putc(hex[p[i] & 0xF]);
            serial_putc(' ');
        }
        serial_print("\n");
    } else {
        serial_print("{\"type\":\"memdump\",\"addr\":\"");
        print_uint_serial((uintptr_t)addr, 16);
        serial_print("\",\"len\":");
        print_uint_serial(len, 10);
        serial_print(",\"data\":\"");
        const uint8_t *p = (const uint8_t *)addr;
        for (size_t i = 0; i < len; i++) {
            const char *hex = "0123456789abcdef";
            serial_putc(hex[(p[i] >> 4) & 0xF]);
            serial_putc(hex[p[i] & 0xF]);
        }
        serial_print("\"}\n");
    }
}

/* 
 * Stack trace using RBP frame pointers.
 * Depends on -fno-omit-frame-pointer (default in GCC usually, but we should verify).
 */
void debug_backtrace(void) {
    struct stack_frame {
        struct stack_frame *next;
        uint64_t rip;
    };

    struct stack_frame *frame;
    __asm__ volatile ("mov %%rbp, %0" : "=r"(frame));

    if (debug_mode == DEBUG_MODE_HR) {
        debug_log(DEBUG_LEVEL_INFO, "TRACE", "Kernel Stack Trace:");
        int i = 0;
        while (frame && i < 32) {
            serial_print("  [");
            print_uint_serial(i++, 10);
            serial_print("] ");
            print_uint_serial(frame->rip, 16);
            serial_print("\n");
            frame = frame->next;
        }
    } else {
        serial_print("{\"type\":\"trace\",\"frames\":[");
        int i = 0;
        while (frame && i < 32) {
            if (i > 0) serial_print(",");
            serial_print("\"");
            print_uint_serial(frame->rip, 16);
            serial_print("\"");
            i++;
            frame = frame->next;
        }
        serial_print("]}\n");
    }
}


static void serial_vprintf(const char *fmt, va_list args) {
    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            
            char pad = ' ';
            if (*fmt == '0') {
                pad = '0';
                fmt++;
            }
            
            int width = 0;
            while (*fmt >= '0' && *fmt <= '9') {
                width = width * 10 + (*fmt - '0');
                fmt++;
            }
            
            int length = 0; /* 0=default, 1=long, 2=long long */
            if (*fmt == 'l') {
                length = 1;
                fmt++;
                if (*fmt == 'l') {
                    length = 2;
                    fmt++;
                }
            }
            
            switch (*fmt) {
                case 's': {
                    const char *s = va_arg(args, char *);
                    serial_print(s ? s : "(null)");
                    break;
                }
                case 'd':
                case 'i': {
                    long long val;
                    if (length == 2)      val = va_arg(args, long long);
                    else if (length == 1) val = va_arg(args, long);
                    else                  val = va_arg(args, int);
                    
                    if (val < 0) {
                        serial_putc('-');
                        val = -val;
                    }
                    /* Simple uint serial with padding */
                    char buf[32];
                    int i = 0;
                    if (val == 0) buf[i++] = '0';
                    while (val > 0) {
                        buf[i++] = "0123456789"[val % 10];
                        val /= 10;
                    }
                    while (i < width) buf[i++] = pad;
                    while (--i >= 0) serial_putc(buf[i]);
                    break;
                }
                case 'x': 
                case 'X':
                case 'p': {
                    uint64_t val;
                    if (*fmt == 'p') {
                        serial_print("0x");
                        val = (uintptr_t)va_arg(args, void*);
                        width = 16; pad = '0';
                    } else {
                        if (length == 2)      val = va_arg(args, uint64_t);
                        else if (length == 1) val = va_arg(args, unsigned long);
                        else                  val = va_arg(args, unsigned int);
                    }
                    
                    char buf[32];
                    int i = 0;
                    const char *digits = (*fmt == 'X') ? "0123456789ABCDEF" : "0123456789abcdef";
                    if (val == 0) buf[i++] = '0';
                    while (val > 0) {
                        buf[i++] = digits[val % 16];
                        val /= 16;
                    }
                    while (i < width) buf[i++] = pad;
                    while (--i >= 0) serial_putc(buf[i]);
                    break;
                }
                case 'c': serial_putc((char)va_arg(args, int)); break;
                case '%': serial_putc('%'); break;
            }
        } else {
            serial_putc(*fmt);
        }
        fmt++;
    }
}
