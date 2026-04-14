/* ============================================================================
 * ps2_keyboard.c — NexusOS PS/2 Keyboard Driver
 * Purpose: Native PS/2 Scan Code Set 2 decoder with state machine
 *          No reliance on i8042 hardware translation (bit 6 = 0).
 *
 * Scan Code Set 2 protocol summary:
 *   Make code :  <byte>            (key pressed)
 *   Break code:  0xF0 <byte>       (key released)
 *   Extended  :  0xE0 <make/break> (arrows, numpad-/, Insert, Delete, etc.)
 *   Pause     :  0xE1 0x14 0x77 0xE1 0xF0 0x14 0xF0 0x77 (special case)
 *
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "drivers/input/ps2_keyboard.h"
#include "drivers/input/ps2_controller.h"
#include "drivers/input/input_event.h"
#include "hal/io.h"
#include "hal/isr.h"
#include "hal/pic.h"
#include "lib/printf.h"
#include "lib/debug.h"

/* Forward declaration */
extern void input_push_event(const input_event_t *event);

/* ── Decoder state ───────────────────────────────────────────────────────── */

static uint8_t  modifiers   = 0;
static int      extended    = 0;   /* 1 after 0xE0 prefix */
static int      released    = 0;   /* 1 after 0xF0 prefix */

/* ── Scan Code Set 2 → ASCII tables (256 entries, indexed by SC2 make code) */
/*
 *  Key layout (Set 2 make codes):
 *  0x0E=` 0x0D=Tab 0x76=Esc
 *  0x16=1 0x1E=2 0x26=3 0x25=4 0x2E=5 0x36=6 0x3D=7 0x3E=8 0x46=9 0x45=0
 *  0x4E=-  0x55==  0x66=Bksp
 *  0x15=q 0x1D=w 0x24=e 0x2D=r 0x2C=t 0x35=y 0x3C=u 0x43=i 0x44=o 0x4D=p
 *  0x54=[  0x5B=]  0x5A=Enter
 *  0x14=LCtrl 0x1C=a 0x1B=s 0x23=d 0x2B=f 0x34=g 0x33=h 0x3B=j 0x42=k
 *  0x4B=l  0x4C=;  0x52='  0x5D=backslash
 *  0x12=LShift 0x1A=z 0x22=x 0x21=c 0x2A=v 0x32=b 0x31=n 0x3A=m
 *  0x41=,  0x49=.  0x4A=/  0x59=RShift
 *  0x11=LAlt  0x29=Space  0x58=CapsLock
 */

static const char sc2_ascii[256] = {
/*00*/0,  /*01:F9*/0,   /*02*/0,    /*03:F5*/0,   /*04:F3*/0,   /*05:F1*/0,
/*06:F2*/0, /*07:F12*/0, /*08*/0,   /*09:F10*/0,  /*0A:F8*/0,   /*0B:F6*/0,
/*0C:F4*/0, /*0D:Tab*/'\t', /*0E:`*/'`', /*0F*/0,
/*10*/0,    /*11:LAlt*/0,  /*12:LSh*/0, /*13*/0,  /*14:LCtrl*/0, /*15:q*/'q',
/*16:1*/'1', /*17*/0,
/*18*/0,    /*19*/0,    /*1A:z*/'z', /*1B:s*/'s', /*1C:a*/'a', /*1D:w*/'w',
/*1E:2*/'2', /*1F*/0,
/*20*/0,    /*21:c*/'c', /*22:x*/'x', /*23:d*/'d', /*24:e*/'e', /*25:4*/'4',
/*26:3*/'3', /*27*/0,
/*28*/0,    /*29:Spc*/' ', /*2A:v*/'v', /*2B:f*/'f', /*2C:t*/'t', /*2D:r*/'r',
/*2E:5*/'5', /*2F*/0,
/*30*/0,    /*31:n*/'n', /*32:b*/'b', /*33:h*/'h', /*34:g*/'g', /*35:y*/'y',
/*36:6*/'6', /*37*/0,
/*38*/0,    /*39*/0,    /*3A:m*/'m', /*3B:j*/'j', /*3C:u*/'u', /*3D:7*/'7',
/*3E:8*/'8', /*3F*/0,
/*40*/0,    /*41:,*/','  , /*42:k*/'k', /*43:i*/'i', /*44:o*/'o', /*45:0*/'0',
/*46:9*/'9', /*47*/0,
/*48*/0,    /*49:.*/'.',  /*4A:/ */'/', /*4B:l*/'l', /*4C:;*/';', /*4D:p*/'p',
/*4E:-*/'-', /*4F*/0,
/*50*/0,    /*51*/0,    /*52:' */'\'',/*53*/0,    /*54:[*/'[', /*55:=*/'=',
/*56*/0,    /*57*/0,
/*58:Caps*/0, /*59:RSh*/0, /*5A:Ent*/'\n', /*5B:] */']', /*5C*/0, /*5D:bsl*/'\\',
/*5E*/0,    /*5F*/0,
/*60*/0,    /*61*/0,    /*62*/0,    /*63*/0,    /*64*/0,    /*65*/0,
/*66:Bksp*/'\b', /*67*/0,
/*68*/0,    /*69*/0,    /*6A*/0,    /*6B*/0,    /*6C*/0,    /*6D*/0,
/*6E*/0,    /*6F*/0,
/*70:KP0*/0, /*71:KP.*/0, /*72:KP2*/0, /*73:KP5*/0, /*74:KP6*/0, /*75:KP8*/0,
/*76:Esc*/27, /*77:NmLk*/0,
/*78:F11*/0, /*79:KP+*/0, /*7A:KP3*/0, /*7B:KP-*/0, /*7C:KP**/0, /*7D:KP9*/0,
/*7E:ScrLk*/0, /*7F*/0,
/*80*/0, /*81*/0, /*82*/0, /*83:F7*/0,
/* 0x84–0xFF: all unmapped */
0,0,0,0,0,0,0,0,0,0,0,0,  /* 84–8F */
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 90–9F */
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* A0–AF */
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* B0–BF */
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* C0–CF */
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* D0–DF */
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* E0–EF */
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0  /* F0–FF */
};

static const char sc2_ascii_shift[256] = {
/*00*/0,  /*01*/0, /*02*/0, /*03*/0, /*04*/0, /*05*/0, /*06*/0, /*07*/0,
/*08*/0,  /*09*/0, /*0A*/0, /*0B*/0, /*0C*/0,
/*0D:Tab*/'\t', /*0E:`*/'~', /*0F*/0,
/*10*/0,    /*11*/0,   /*12*/0,   /*13*/0,  /*14*/0,  /*15:Q*/'Q',
/*16:!*/'!', /*17*/0,
/*18*/0,    /*19*/0,   /*1A:Z*/'Z', /*1B:S*/'S', /*1C:A*/'A', /*1D:W*/'W',
/*1E:@*/'@', /*1F*/0,
/*20*/0,    /*21:C*/'C', /*22:X*/'X', /*23:D*/'D', /*24:E*/'E', /*25:$*/'$',
/*26:#*/'#', /*27*/0,
/*28*/0,    /*29:Spc*/' ', /*2A:V*/'V', /*2B:F*/'F', /*2C:T*/'T', /*2D:R*/'R',
/*2E:%*/'%', /*2F*/0,
/*30*/0,    /*31:N*/'N', /*32:B*/'B', /*33:H*/'H', /*34:G*/'G', /*35:Y*/'Y',
/*36:^*/'^', /*37*/0,
/*38*/0,    /*39*/0,   /*3A:M*/'M', /*3B:J*/'J', /*3C:U*/'U', /*3D:&*/'&',
/*3E:**/'*', /*3F*/0,
/*40*/0,    /*41:<*/'<', /*42:K*/'K', /*43:I*/'I', /*44:O*/'O', /*45:)*/')',
/*46:(*/'(', /*47*/0,
/*48*/0,    /*49:>*/'>', /*4A:?*/'?', /*4B:L*/'L', /*4C::*/':', /*4D:P*/'P',
/*4E:_*/'_', /*4F*/0,
/*50*/0,    /*51*/0,   /*52:"*/'"',  /*53*/0,  /*54:{*/'{', /*55:+*/'+',
/*56*/0,    /*57*/0,
/*58*/0,    /*59*/0,   /*5A:Ent*/'\n', /*5B:}*/'}', /*5C*/0, /*5D:|*/'|',
/*5E*/0,    /*5F*/0,
/*60*/0,    /*61*/0,   /*62*/0,   /*63*/0,   /*64*/0,  /*65*/0,
/*66:Bksp*/'\b', /*67*/0,
/*68*/0,    /*69*/0,   /*6A*/0,   /*6B*/0,   /*6C*/0,  /*6D*/0,
/*6E*/0,    /*6F*/0,
/*70*/0,    /*71*/0,   /*72*/0,   /*73*/0,   /*74*/0,  /*75*/0,
/*76:Esc*/27, /*77*/0,
/*78*/0,    /*79*/0,   /*7A*/0,   /*7B*/0,   /*7C*/0,  /*7D*/0,
/*7E*/0,    /*7F*/0,
/*80*/0, /*81*/0, /*82*/0, /*83*/0,
/* 0x84–0xFF */
0,0,0,0,0,0,0,0,0,0,0,0,  /* 84–8F */
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 90–9F */
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* A0–AF */
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* B0–BF */
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* C0–CF */
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* D0–DF */
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* E0–EF */
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0  /* F0–FF */
};

/* ── SC2 make code → modifier flag mapping ───────────────────────────────── */

/* Returns the modifier flag for SC2 make codes that are modifier keys, 0 otherwise */
static uint8_t sc2_get_modifier_flag(uint8_t sc, int is_extended) {
    if (!is_extended) {
        switch (sc) {
            case 0x12: return MOD_SHIFT;   /* L Shift */
            case 0x59: return MOD_SHIFT;   /* R Shift */
            case 0x14: return MOD_CTRL;    /* L Ctrl */
            case 0x11: return MOD_ALT;     /* L Alt */
            case 0x58: return MOD_CAPSLOCK; /* Caps Lock (toggle handled separately) */
            default:   return 0;
        }
    } else {
        switch (sc) {
            case 0x14: return MOD_CTRL;    /* R Ctrl (E0 14) */
            case 0x11: return MOD_ALT;     /* R Alt  (E0 11) */
            default:   return 0;
        }
    }
}

/* ── IRQ1 handler ────────────────────────────────────────────────────────── */

static void keyboard_irq_handler(registers_t *regs) {
    (void)regs;

    uint8_t sc = inb(PS2_DATA_PORT);

    /* ── State machine ── */
    if (sc == 0xE0) {
        extended = 1;
        pic_send_eoi(1);
        return;
    }
    if (sc == 0xE1) {
        /* Pause key — 8-byte sequence; just ignore by flushing state */
        extended = 0;
        released = 0;
        pic_send_eoi(1);
        return;
    }
    if (sc == 0xF0) {
        released = 1;
        pic_send_eoi(1);
        return;
    }

    /* sc is now a valid make code */
    int is_ext = extended;
    int is_rel = released;
    extended   = 0;
    released   = 0;

    debug_log(DEBUG_LEVEL_INFO, "IRQ1",
              "SC2: sc=0x%02x ext=%d rel=%d", (int)sc, is_ext, is_rel);

    /* ── Handle modifier keys ── */
    uint8_t mod_flag = sc2_get_modifier_flag(sc, is_ext);

    if (mod_flag) {
        if (mod_flag == MOD_CAPSLOCK) {
            /* Caps Lock toggles only on make */
            if (!is_rel) {
                modifiers ^= MOD_CAPSLOCK;
            }
        } else {
            if (is_rel) {
                modifiers &= ~mod_flag;
            } else {
                modifiers |= mod_flag;
            }
        }
        pic_send_eoi(1);
        return;
    }

    /* ── Determine ASCII character ── */
    char ch = 0;
    if (!is_ext) {
        int use_shift = (modifiers & MOD_SHIFT) != 0;
        int caps      = (modifiers & MOD_CAPSLOCK) != 0;

        ch = use_shift ? sc2_ascii_shift[sc] : sc2_ascii[sc];

        /* CapsLock flips case on letters */
        if (caps && ch >= 'a' && ch <= 'z')      ch -= 32;
        else if (caps && ch >= 'A' && ch <= 'Z') ch += 32;
    } else {
        /* Extended scancodes */
        switch (sc) {
            case 0x6B: ch = KEY_LEFT;  break;
            case 0x74: ch = KEY_RIGHT; break;
            case 0x75: ch = KEY_UP;    break;
            case 0x72: ch = KEY_DOWN;  break;
            case 0x71: ch = KEY_DELETE;break;
        }
    }

    /* ── Build and push input event ── */
    input_event_t event = {0};
    event.scancode  = sc;
    event.modifiers = modifiers;

    if (is_rel) {
        event.type  = INPUT_EVENT_KEY_RELEASE;
        event.ascii = 0;
    } else {
        event.type  = INPUT_EVENT_KEY_PRESS;
        event.ascii = ch;
    }

    input_push_event(&event);
    pic_send_eoi(1);
}

/* ── Public API ──────────────────────────────────────────────────────────── */

void ps2_keyboard_init(void) {
    /* Step 1: Reset the keyboard */
    ps2_send_data(0xFF);
    uint8_t ack = ps2_read_data();
    if (ack != 0xFA) {
        debug_log(DEBUG_LEVEL_WARN, "KBD", "Reset ACK unexpected: 0x%02x", (int)ack);
    }
    /* Drain self-test result (0xAA = pass) */
    if (inb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT) {
        inb(PS2_DATA_PORT);
    }

    /* Step 2: Select Scan Code Set 2 explicitly */
    ps2_send_data(0xF0);           /* Set scan code set command */
    ps2_read_data();               /* ACK */
    ps2_send_data(0x02);           /* Set 2 */
    ps2_read_data();               /* ACK */

    /* Step 3: Enable scanning */
    ps2_send_data(0xF4);
    ps2_read_data();               /* ACK */

    /* Step 4: Register IRQ1 handler and unmask */
    isr_register_handler(IRQ1, keyboard_irq_handler);
    pic_clear_mask(1);

    kprintf_set_color(0x0088FF88, FB_DEFAULT_BG);
    kprintf("[OK] ");
    kprintf_set_color(0x00CCCCCC, FB_DEFAULT_BG);
    kprintf("PS/2 keyboard initialized (IRQ1, Scan Code Set 2)\n");
}

uint8_t ps2_keyboard_get_modifiers(void) {
    return modifiers;
}
