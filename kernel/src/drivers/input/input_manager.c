/* ============================================================================
 * input_manager.c — NexusOS Input Event Manager
 * Purpose: Ring buffer for unified input events, polling API
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "drivers/input/input_event.h"
#include "lib/spinlock.h"
#include "lib/string.h"
#include "lib/printf.h"

/* ── Ring buffer ────────────────────────────────────────────────────────── */

#define INPUT_QUEUE_SIZE 256

static input_event_t event_queue[INPUT_QUEUE_SIZE];
static volatile uint32_t queue_head = 0;
static volatile uint32_t queue_tail = 0;
static spinlock_t input_lock = SPINLOCK_INIT;

/* ── Push an event into the queue (called from IRQ handlers) ────────────── */

void input_push_event(const input_event_t *event) {
    uint32_t next = (queue_head + 1) % INPUT_QUEUE_SIZE;
    if (next == queue_tail) {
        /* Queue full — drop oldest event */
        queue_tail = (queue_tail + 1) % INPUT_QUEUE_SIZE;
    }
    event_queue[queue_head] = *event;
    queue_head = next;
}

/* ── Poll an event from the queue ───────────────────────────────────────── */

int input_poll_event(input_event_t *out) {
    if (queue_head == queue_tail) {
        return 0;  /* Queue empty */
    }

    spinlock_acquire(&input_lock);

    if (queue_head == queue_tail) {
        spinlock_release(&input_lock);
        return 0;
    }

    *out = event_queue[queue_tail];
    queue_tail = (queue_tail + 1) % INPUT_QUEUE_SIZE;

    spinlock_release(&input_lock);
    return 1;
}

/* ── Check if events are available ──────────────────────────────────────── */

int input_has_event(void) {
    return queue_head != queue_tail;
}

/* ── Initialize the input manager ───────────────────────────────────────── */

void input_manager_init(void) {
    queue_head = 0;
    queue_tail = 0;
    memset(event_queue, 0, sizeof(event_queue));

    kprintf_set_color(0x0088FF88, FB_DEFAULT_BG);
    kprintf("[OK] ");
    kprintf_set_color(0x00CCCCCC, FB_DEFAULT_BG);
    kprintf("Input manager: %u-event ring buffer\n",
            (unsigned int)INPUT_QUEUE_SIZE);
}
