/* ============================================================================
 * terminal.c — NexusOS VT100 Terminal Emulator
 * Purpose: ANSI escape sequence parser and character rendering
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "display/terminal.h"
#include "drivers/video/framebuffer.h"
#include "lib/string.h"
#include <stddef.h>

terminal_t main_terminal;

/* Global ANSI to ARGB palette mapping */
static const uint32_t terminal_palette[16] = {
    TERM_COLOR_BLACK,   TERM_COLOR_RED,     TERM_COLOR_GREEN,   TERM_COLOR_BROWN,
    TERM_COLOR_BLUE,    TERM_COLOR_MAGENTA, TERM_COLOR_CYAN,    TERM_COLOR_LIGHT_GRAY,
    TERM_COLOR_DARK_GRAY, TERM_COLOR_LIGHT_RED, TERM_COLOR_LIGHT_GREEN, TERM_COLOR_YELLOW,
    TERM_COLOR_LIGHT_BLUE, TERM_COLOR_LIGHT_MAGENTA, TERM_COLOR_LIGHT_CYAN, TERM_COLOR_WHITE
};

void terminal_init(terminal_t *term) {
    uint32_t cols, rows;
    framebuffer_get_text_dimensions(&cols, &rows);
    
    term->width = cols;
    term->height = rows;
    term->cursor_x = 0;
    term->cursor_y = 0;
    term->fg_color = TERM_DEFAULT_FG;
    term->bg_color = TERM_DEFAULT_BG;
    term->state = VT_STATE_NORMAL;
    term->param_count = 0;
    term->private_mode = 0;
    
    terminal_clear(term);
}

void terminal_clear(terminal_t *term) {
    framebuffer_clear(term->bg_color);
    term->cursor_x = 0;
    term->cursor_y = 0;
}

void terminal_scroll(terminal_t *term) {
    framebuffer_scroll(FONT_HEIGHT);
    if (term->cursor_y > 0) {
        term->cursor_y--;
    }
}

void terminal_set_colors(terminal_t *term, uint32_t fg, uint32_t bg) {
    term->fg_color = fg;
    term->bg_color = bg;
}

/* Character routing with line wrapping and scrolling */
void terminal_putchar(terminal_t *term, char c) {
    if (c == '\n') {
        term->cursor_x = 0;
        term->cursor_y++;
    } else if (c == '\r') {
        term->cursor_x = 0;
    } else if (c == '\b') {
        if (term->cursor_x > 0) {
            term->cursor_x--;
            framebuffer_draw_char(term->cursor_x * FONT_WIDTH, term->cursor_y * FONT_HEIGHT, ' ', term->fg_color, term->bg_color);
        }
    } else if (c == '\t') {
        term->cursor_x = (term->cursor_x + 8) & ~7;
    } else {
        if (term->cursor_x >= term->width) {
            term->cursor_x = 0;
            term->cursor_y++;
        }
        
        if (term->cursor_y >= term->height) {
            terminal_scroll(term);
            term->cursor_y = term->height - 1;
        }
        
        framebuffer_draw_char(term->cursor_x * FONT_WIDTH, term->cursor_y * FONT_HEIGHT, c, term->fg_color, term->bg_color);
        term->cursor_x++;
    }
    
    /* Handle final wrap/scroll if we hit boundaries after update */
    if (term->cursor_x >= term->width) {
        term->cursor_x = 0;
        term->cursor_y++;
    }
    if (term->cursor_y >= term->height) {
        terminal_scroll(term);
        term->cursor_y = term->height - 1;
    }
}

/* Handle SGR (Select Graphic Rendition) - ESC [ nm */
static void terminal_handle_sgr(terminal_t *term) {
    if (term->param_count == 0) {
        /* Default: Reset */
        term->fg_color = TERM_DEFAULT_FG;
        term->bg_color = TERM_DEFAULT_BG;
        return;
    }
    
    for (int i = 0; i < term->param_count; i++) {
        uint32_t p = term->params[i];
        
        if (p == 0) {
            /* Reset */
            term->fg_color = TERM_DEFAULT_FG;
            term->bg_color = TERM_DEFAULT_BG;
        } else if (p >= 30 && p <= 37) {
            /* Foreground color (0-7) */
            term->fg_color = terminal_palette[p - 30];
        } else if (p >= 40 && p <= 47) {
            /* Background color (0-7) */
            term->bg_color = terminal_palette[p - 40];
        } else if (p >= 90 && p <= 97) {
            /* Bright foreground (8-15) */
            term->fg_color = terminal_palette[p - 90 + 8];
        } else if (p >= 100 && p <= 107) {
            /* Bright background (8-15) */
            term->bg_color = terminal_palette[p - 100 + 8];
        } else if (p == 39) {
            /* Reset foreground */
            term->fg_color = TERM_DEFAULT_FG;
        } else if (p == 49) {
            /* Reset background */
            term->bg_color = TERM_DEFAULT_BG;
        }
    }
}

/* VT100 Emulator - Main Parser Entry */
void terminal_write(terminal_t *term, const char *data, uint64_t size) {
    for (uint64_t i = 0; i < size; i++) {
        char c = data[i];
        
        switch (term->state) {
            case VT_STATE_NORMAL:
                if (c == '\033') {
                    term->state = VT_STATE_ESC;
                } else {
                    terminal_putchar(term, c);
                }
                break;
                
            case VT_STATE_ESC:
                if (c == '[') {
                    term->state = VT_STATE_CSI;
                    term->param_count = 0;
                    memset(term->params, 0, sizeof(term->params));
                    term->private_mode = 0;
                } else {
                    /* Unhandled sequence, return to normal */
                    term->state = VT_STATE_NORMAL;
                }
                break;
                
            case VT_STATE_CSI:
                if (c == '?') {
                    term->private_mode = 1;
                } else if (c >= '0' && c <= '9') {
                    term->params[term->param_count] = c - '0';
                    term->state = VT_STATE_PARAM;
                } else if (c == ';') {
                    term->param_count++;
                } else {
                    /* Final character of a CSI sequence */
                    if (c == 'm') {
                        if (term->state != VT_STATE_PARAM && term->param_count == 0) {
                             /* Special case for \033[m */
                             term->param_count = 0;
                        } else {
                             term->param_count++;
                        }
                        terminal_handle_sgr(term);
                    } else if (c == 'H' || c == 'f') {
                        /* Cursor position */
                        uint32_t y = term->params[0];
                        uint32_t x = term->params[1];
                        if (y > 0) y--; if (x > 0) x--;
                        if (y >= term->height) y = term->height - 1;
                        if (x >= term->width) x = term->width - 1;
                        term->cursor_x = x;
                        term->cursor_y = y;
                    } else if (c == 'J') {
                        /* Clear Screen */
                        terminal_clear(term);
                    }
                    
                    term->state = VT_STATE_NORMAL;
                }
                break;
                
            case VT_STATE_PARAM:
                if (c >= '0' && c <= '9') {
                    term->params[term->param_count] = term->params[term->param_count] * 10 + (c - '0');
                } else if (c == ';') {
                    term->state = VT_STATE_CSI;
                    term->param_count++;
                } else {
                    /* Exit param mode and process character as end of CSI */
                    term->param_count++;
                    i--; /* Reprocess this char as end of CSI */
                    term->state = VT_STATE_CSI;
                }
                break;
        }
    }
}
