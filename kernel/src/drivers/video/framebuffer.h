/* ============================================================================
 * framebuffer.h — NexusOS Framebuffer Driver
 * Purpose: Low-level pixel rendering to the Limine-provided linear framebuffer
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#ifndef NEXUS_DRIVERS_VIDEO_FRAMEBUFFER_H
#define NEXUS_DRIVERS_VIDEO_FRAMEBUFFER_H

#include <stdint.h>
#include <stddef.h>

/* Framebuffer information structure */
typedef struct {
    volatile uint32_t *address;    /* Pointer to pixel data */
    uint64_t width;                /* Width in pixels */
    uint64_t height;               /* Height in pixels */
    uint64_t pitch;                /* Bytes per scanline */
    uint16_t bpp;                  /* Bits per pixel */
} framebuffer_info_t;

/* Initialize the framebuffer from Limine response. Returns 0 on success. */
int framebuffer_init(void);

/* Get framebuffer information */
const framebuffer_info_t *framebuffer_get_info(void);

/* Put a single pixel at (x, y) with the given 32-bit ARGB color */
void framebuffer_put_pixel(uint32_t x, uint32_t y, uint32_t color);

/* Fill the entire screen with a solid color */
void framebuffer_clear(uint32_t color);

/* Scroll the framebuffer up by one text line (font_height pixels) */
void framebuffer_scroll(uint32_t font_height);

/* Draw a single character glyph at pixel position (px, py) */
void framebuffer_draw_char(uint32_t px, uint32_t py, char c,
                           uint32_t fg, uint32_t bg);

/* Get terminal grid dimensions based on the built-in font */
void framebuffer_get_text_dimensions(uint32_t *cols, uint32_t *rows);

/* Built-in font dimensions */
#define FONT_WIDTH   8
#define FONT_HEIGHT  16

#endif /* NEXUS_DRIVERS_VIDEO_FRAMEBUFFER_H */
