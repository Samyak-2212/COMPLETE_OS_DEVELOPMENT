/* ============================================================================
 * input_event.h — NexusOS Unified Input Event Structure
 * Purpose: Common event type for keyboard and mouse input
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#ifndef NEXUS_DRIVERS_INPUT_EVENT_H
#define NEXUS_DRIVERS_INPUT_EVENT_H

#include <stdint.h>

/* ── Event types ─────────────────────────────────────────────────────────── */

#define INPUT_EVENT_KEY_PRESS     1
#define INPUT_EVENT_KEY_RELEASE   2
#define INPUT_EVENT_MOUSE_MOVE    3
#define INPUT_EVENT_MOUSE_BUTTON  4
#define INPUT_EVENT_MOUSE_SCROLL  5

/* ── Mouse button flags ──────────────────────────────────────────────────── */

#define MOUSE_BTN_LEFT      (1 << 0)
#define MOUSE_BTN_RIGHT     (1 << 1)
#define MOUSE_BTN_MIDDLE    (1 << 2)

/* ── Input event structure ───────────────────────────────────────────────── */

typedef struct {
    uint8_t  type;          /* INPUT_EVENT_* */
    uint8_t  scancode;      /* Raw scancode (for key events) */
    char     ascii;         /* ASCII character (0 if non-printable) */
    uint8_t  modifiers;     /* Shift, Ctrl, Alt flags */
    int16_t  mouse_dx;      /* Mouse X delta (for mouse events) */
    int16_t  mouse_dy;      /* Mouse Y delta */
    int8_t   mouse_dz;      /* Mouse scroll delta */
    uint8_t  mouse_buttons; /* Mouse button state (MOUSE_BTN_*) */
} input_event_t;

/* ── Modifier flags ──────────────────────────────────────────────────────── */

#define MOD_SHIFT   (1 << 0)
#define MOD_CTRL    (1 << 1)
#define MOD_ALT     (1 << 2)
#define MOD_CAPSLOCK (1 << 3)

/* ── Custom Key codes (ASCII field for special keys) ─────────────────────── */
#define KEY_UP      0x80
#define KEY_DOWN    0x81
#define KEY_LEFT    0x82
#define KEY_RIGHT   0x83
#define KEY_DELETE  0x7F  /* Standard DEL */

#endif /* NEXUS_DRIVERS_INPUT_EVENT_H */
