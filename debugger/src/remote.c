#include "debugger.h"
#include <lib/string.h>

/* Low-level serial access from serial_poll.c */
void debugger_serial_init(void);
void debugger_serial_putc(char c);
char debugger_serial_getc(void);
bool debugger_serial_has_data(void);
void debugger_serial_print(const char *s);

static uint8_t calculate_checksum(const char *data) {
    uint8_t sum = 0;
    while (*data) {
        sum += (uint8_t)(*data++);
    }
    return sum;
}

static void send_packet(const char *payload) {
    uint8_t cksum = calculate_checksum(payload);
    
    debugger_serial_putc('$');
    debugger_serial_print(payload);
    debugger_serial_putc('#');
    
    const char *hex = "0123456789abcdef";
    debugger_serial_putc(hex[(cksum >> 4) & 0xF]);
    debugger_serial_putc(hex[cksum & 0xF]);
}

void debugger_send_response(const char *json) {
    send_packet(json);
}

void debugger_send_event(const char *type, const char *msg) {
    /* Rudimentary JSON builder to avoid complex internal state */
    char buf[512];
    /* Note: In a real kernel, we'd use a safer snprintf, but for simplicity: */
    // TODO: Implement a safe snprintf-like for debugger or use current kprintf-logic
    // For now, let's just use manual string concatenation
    
    /* Simplistic: {"event":"TYPE","msg":"MSG"} */
    strcpy(buf, "{\"event\":\"");
    strcat(buf, type);
    strcat(buf, "\",\"msg\":\"");
    strcat(buf, msg);
    strcat(buf, "\"}");
    
    send_packet(buf);
}

/* 
 * Receive a packet from serial.
 * Returns true if a valid packet was received in out_buf.
 */
bool debugger_receive_packet(char *out_buf, size_t max_len) {
    char c = debugger_serial_getc();
    if (c != '$') return false;

    size_t i = 0;
    while (i < max_len - 1) {
        c = debugger_serial_getc();
        if (c == '#') break;
        out_buf[i++] = c;
    }
    out_buf[i] = '\0';

    /* Read checksum */
    char ck_hi = debugger_serial_getc();
    char ck_lo = debugger_serial_getc();
    
    /* TODO: Validate checksum. For now, we trust the agent or protocol layer. */
    
    return true;
}
