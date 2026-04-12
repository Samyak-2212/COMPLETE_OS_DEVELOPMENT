/* ============================================================================
 * terminal.h — NexusOS VT100 Terminal Emulator
 * Purpose: ANSI escape sequence definitions and terminal state
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#ifndef NEXUS_DISPLAY_TERMINAL_H
#define NEXUS_DISPLAY_TERMINAL_H

#include <stdint.h>
#include <stdbool.h>

/* ANSI Standard Colors (4-bit palette) */
#define TERM_COLOR_BLACK          0x00000000
#define TERM_COLOR_RED            0x00AA0000
#define TERM_COLOR_GREEN          0x0000AA00
#define TERM_COLOR_BROWN          0x00AA5500
#define TERM_COLOR_BLUE           0x000000AA
#define TERM_COLOR_MAGENTA        0x00AA00AA
#define TERM_COLOR_CYAN           0x0000AAAA
#define TERM_COLOR_LIGHT_GRAY     0x00AAAAAA
#define TERM_COLOR_DARK_GRAY      0x00555555
#define TERM_COLOR_LIGHT_RED      0x00FF5555
#define TERM_COLOR_LIGHT_GREEN    0x0055FF55
#define TERM_COLOR_YELLOW         0x00FFFF55
#define TERM_COLOR_LIGHT_BLUE     0x005555FF
#define TERM_COLOR_LIGHT_MAGENTA  0x00FF55FF
#define TERM_COLOR_LIGHT_CYAN     0x0055FFFF
#define TERM_COLOR_WHITE          0x00FFFFFF
#define TERM_COLOR_NEON_GREEN     0x0039FF14

/* ANSI Default Colors */
#define TERM_DEFAULT_FG           TERM_COLOR_LIGHT_GRAY
#define TERM_DEFAULT_BG           0x001A1A2E

/* Parser states */
typedef enum {
    VT_STATE_NORMAL,
    VT_STATE_ESC,
    VT_STATE_CSI,
    VT_STATE_PARAM,
    VT_STATE_DEC_SPACE  /* After ESC [ ... but before q, looking for ' ' */
} terminal_vt_state_t;

/* Max parameters for CSI sequences (e.g. \033[1;2;3m) */
#define VT_MAX_PARAMS 16

typedef struct {
    uint32_t cursor_x;
    uint32_t cursor_y;
    uint32_t width;
    uint32_t height;
    
    uint32_t fg_color;
    uint32_t bg_color;
    
    /* VT100 Parser State */
    terminal_vt_state_t state;
    uint32_t params[VT_MAX_PARAMS];
    int      param_count;
    char     private_mode;

    /* Backbuffer */
    char     *buffer;
    uint32_t *attr_buffer;

    /* Cursor State */
    bool     cursor_enabled;
    bool     cursor_blink_visible; /* Internal: Toggled by timer */
    uint32_t cursor_style;         /* 0=block, 1=bar, etc */
    int      cursor_suspended;     /* Re-entrancy counter for batch writes */
    uint32_t last_rendered_x;      /* Coordinates where cursor was last DRAWN */
    uint32_t last_rendered_y;      /* (Used to ensure stable erasure) */
    uint64_t last_blink_ms;
} terminal_t;

/* Global terminal instance */
extern terminal_t main_terminal;

/* API */
void terminal_init(terminal_t *term);
void terminal_write(terminal_t *term, const char *data, uint64_t size);
void terminal_putchar(terminal_t *term, char c);

/* Allocate backbuffers (must be called after heap init) */
void terminal_allocate_backbuffer(terminal_t *term);

/* Clear terminal screen */
void terminal_clear(terminal_t *term);
void terminal_scroll(terminal_t *term);
void terminal_set_colors(terminal_t *term, uint32_t fg, uint32_t bg);

/* Cursor API */
void terminal_render_cursor(terminal_t *term, bool visible);
void terminal_blink_tick(terminal_t *term);

#endif /* NEXUS_DISPLAY_TERMINAL_H */
