/* ============================================================================
 * terminal.c — NexusOS VT100 Terminal Emulator
 * Purpose: ANSI escape sequence parser and character rendering
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "display/terminal.h"
#include "drivers/video/framebuffer.h"
#include "lib/string.h"
#include "mm/heap.h"
#include "drivers/timer/pit.h"
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
    term->cursor_style = 5; /* Bar cursor */
    term->last_rendered_x = 0;
    term->last_rendered_y = 0;
    
    /* Buffers will be allocated later when heap is initialized */
    term->buffer = NULL;
    term->attr_buffer = NULL;
    
    term->cursor_enabled = true;
    term->cursor_blink_visible = true;
    term->cursor_style = 5; /* Bar */
    term->last_blink_ms = 0;
    
    terminal_clear(term);
}

void terminal_allocate_backbuffer(terminal_t *term) {
    if (!term->buffer) {
        term->buffer = kmalloc(term->width * term->height);
        if (term->buffer) memset(term->buffer, ' ', term->width * term->height);
    }
    if (!term->attr_buffer) {
        term->attr_buffer = kmalloc(term->width * term->height * sizeof(uint32_t));
        if (term->attr_buffer) {
            for (uint32_t i = 0; i < term->width * term->height; i++) term->attr_buffer[i] = term->fg_color;
        }
    }
}

void terminal_clear(terminal_t *term) {
    terminal_render_cursor(term, false);
    framebuffer_clear(term->bg_color);
    term->cursor_x = 0;
    term->cursor_y = 0;
    if (term->buffer) memset(term->buffer, ' ', term->width * term->height);
    term->last_rendered_x = 0;
    term->last_rendered_y = 0;
    terminal_render_cursor(term, term->cursor_blink_visible);
}

void terminal_scroll(terminal_t *term) {
    framebuffer_scroll(FONT_HEIGHT);
    if (term->cursor_y > 0) {
        term->cursor_y--;
    }
    if (term->last_rendered_y > 0) {
        term->last_rendered_y--;
    }
    
    /* Scroll backbuffer */
    if (term->buffer) {
        memmove(term->buffer, term->buffer + term->width, term->width * (term->height - 1));
        memset(term->buffer + term->width * (term->height - 1), ' ', term->width);
    }
    if (term->attr_buffer) {
        memmove(term->attr_buffer, term->attr_buffer + term->width, term->width * (term->height - 1) * sizeof(uint32_t));
        for (uint32_t i = 0; i < term->width; i++) {
            term->attr_buffer[term->width * (term->height - 1) + i] = term->fg_color;
        }
    }
}

void terminal_set_colors(terminal_t *term, uint32_t fg, uint32_t bg) {
    term->fg_color = fg;
    term->bg_color = bg;
}

/* Character routing with line wrapping and scrolling */
void terminal_putchar(terminal_t *term, char c) {
    if (term->cursor_suspended == 0) terminal_render_cursor(term, false);

    if (c == '\n') {
        term->cursor_x = 0;
        term->cursor_y++;
    } else if (c == '\r') {
        term->cursor_x = 0;
    } else if (c == '\b') {
        if (term->cursor_x > 0) {
            term->cursor_x--;
            framebuffer_draw_char(term->cursor_x * FONT_WIDTH, term->cursor_y * FONT_HEIGHT, ' ', term->fg_color, term->bg_color);
            /* Update backbuffer */
            if (term->buffer) term->buffer[term->cursor_y * term->width + term->cursor_x] = ' ';
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
        
        /* Update backbuffer */
        if (term->buffer) term->buffer[term->cursor_y * term->width + term->cursor_x] = c;
        if (term->attr_buffer) term->attr_buffer[term->cursor_y * term->width + term->cursor_x] = term->fg_color;
        
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

    if (term->cursor_suspended == 0) terminal_render_cursor(term, term->cursor_blink_visible);
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
    /* Always ensure cursor is visible when typing/writing to sync last_rendered */
    term->cursor_blink_visible = true;
    term->last_blink_ms = pit_get_ticks();

    /* Hide cursor and suspend automatic updates while we process this batch */
    terminal_render_cursor(term, false);
    term->cursor_suspended++;

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
                } else if (c == ' ') {
                    term->state = VT_STATE_DEC_SPACE;
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
                    } else if (c == 'K') {
                        /* Erase in Line (0=to end) */
                        uint32_t start_x = term->cursor_x;
                        for (uint32_t x = start_x; x < term->width; x++) {
                            if (term->buffer) term->buffer[term->cursor_y * term->width + x] = ' ';
                            framebuffer_draw_char(x * FONT_WIDTH, term->cursor_y * FONT_HEIGHT, ' ', term->fg_color, term->bg_color);
                        }
                    } else if (c == 'C') {
                        /* Cursor Forward */
                        uint32_t n = term->params[0];
                        if (n == 0) n = 1;
                        term->cursor_x += n;
                        if (term->cursor_x >= term->width) term->cursor_x = term->width - 1;
                    } else if (c == 'D') {
                        /* Cursor Backward */
                        uint32_t n = term->params[0];
                        if (n == 0) n = 1;
                        if (term->cursor_x >= n) term->cursor_x -= n;
                        else term->cursor_x = 0;
                    } else if (c == 'q' && term->state == VT_STATE_DEC_SPACE) {
                        /* DECSCUSR - Set Cursor Style */
                        term->cursor_style = term->params[0];
                        term->state = VT_STATE_NORMAL;
                    } else if (c == 'h' && term->private_mode && term->params[0] == 25) {
                        /* Show Cursor */
                        term->cursor_enabled = true;
                    } else if (c == 'l' && term->private_mode && term->params[0] == 25) {
                        /* Hide Cursor */
                        term->cursor_enabled = false;
                    }
                    
                    if (term->state != VT_STATE_DEC_SPACE) {
                        term->state = VT_STATE_NORMAL;
                    }
                }
                break;
                
            case VT_STATE_DEC_SPACE:
                if (c == 'q') {
                    term->cursor_style = term->params[0];
                    term->state = VT_STATE_NORMAL;
                } else if (c != ' ') {
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

    term->cursor_suspended--;
    /* Restore cursor at the final resolved position */
    terminal_render_cursor(term, term->cursor_blink_visible);
}

void terminal_render_cursor(terminal_t *term, bool visible) {
    if (!term || !term->cursor_enabled) return;
    if (term->cursor_suspended > 0 && visible) return; /* Don't draw bar if suspended */
    
    /* Safety bounds check */
    if (term->cursor_x >= term->width || term->cursor_y >= term->height) return;

    uint32_t px, py;
    
    if (!visible) {
        /* Erase: Draw character from backbuffer at LAST DRAWN location */
        px = term->last_rendered_x * FONT_WIDTH;
        py = term->last_rendered_y * FONT_HEIGHT;
        
        char c = ' ';
        uint32_t fg = term->fg_color;
        if (term->buffer) c = term->buffer[term->last_rendered_y * term->width + term->last_rendered_x];
        if (term->attr_buffer) fg = term->attr_buffer[term->last_rendered_y * term->width + term->last_rendered_x];
        framebuffer_draw_char(px, py, c, fg, term->bg_color);
    } else {
        /* Update tracking coordinates for the draw position */
        term->last_rendered_x = term->cursor_x;
        term->last_rendered_y = term->cursor_y;
        px = term->cursor_x * FONT_WIDTH;
        py = term->cursor_y * FONT_HEIGHT;
        
        /* Draw cursor based on style */
        uint32_t color = TERM_COLOR_NEON_GREEN;
        
        if (term->cursor_style == 1 || term->cursor_style == 0) {
            /* Block */
            for (int y = 0; y < FONT_HEIGHT; y++) {
                for (int x = 0; x < FONT_WIDTH; x++) {
                    framebuffer_put_pixel(px + x, py + y, color);
                }
            }
        } else if (term->cursor_style == 2) {
            /* Steady Block (handled by visible=true) */
        } else if (term->cursor_style == 3 || term->cursor_style == 4) {
            /* Underline */
            for (int x = 0; x < FONT_WIDTH; x++) {
                framebuffer_put_pixel(px + x, py + FONT_HEIGHT - 1, color);
                framebuffer_put_pixel(px + x, py + FONT_HEIGHT - 2, color);
            }
        } else if (term->cursor_style == 5 || term->cursor_style == 6) {
            /* Bar (vertical) */
            for (int y = 0; y < FONT_HEIGHT; y++) {
                framebuffer_put_pixel(px, py + y, color);
                framebuffer_put_pixel(px + 1, py + y, color);
            }
        }
    }
}

void terminal_blink_tick(terminal_t *term) {
    if (term->cursor_suspended > 0) return; /* No blinking during batch operations */
    
    uint64_t now = pit_get_ticks();
    if (now - term->last_blink_ms >= 500) {
        term->cursor_blink_visible = !term->cursor_blink_visible;
        terminal_render_cursor(term, term->cursor_blink_visible);
        term->last_blink_ms = now;
    }
}
