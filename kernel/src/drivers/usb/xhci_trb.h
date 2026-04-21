/* ============================================================================
 * xhci_trb.h — NexusOS xHCI Transfer Request Block Definitions
 * Purpose: TRB type codes, control flags, completion codes, and struct defs
 * Reference: xHCI Specification 1.2, Section 6.4
 * Author: NexusOS Driver Agent (Phase 4, Task 4.7)
 * ============================================================================ */

#ifndef XHCI_TRB_H
#define XHCI_TRB_H

#include <stdint.h>

/* ── Generic TRB layout (all TRBs are 16 bytes) ──────────────────────────── */

typedef struct {
    uint64_t parameter;   /* TRB-specific data (address, setup packet, etc.) */
    uint32_t status;      /* TRB-specific status (transfer length, CC, etc.)  */
    uint32_t control;     /* Type[15:10] | flags[9:0] | Cycle[0]              */
} __attribute__((packed)) xhci_trb_t;

/* ── TRB Type codes (bits [15:10] of control field) ─────────────────────── */

/* Transfer TRBs */
#define TRB_TYPE_NORMAL             1
#define TRB_TYPE_SETUP_STAGE        2
#define TRB_TYPE_DATA_STAGE         3
#define TRB_TYPE_STATUS_STAGE       4
#define TRB_TYPE_ISOCH              5
#define TRB_TYPE_LINK               6
#define TRB_TYPE_EVENT_DATA         7
#define TRB_TYPE_NOOP               8

/* Command TRBs */
#define TRB_TYPE_ENABLE_SLOT        9
#define TRB_TYPE_DISABLE_SLOT      10
#define TRB_TYPE_ADDRESS_DEVICE    11
#define TRB_TYPE_CONFIG_ENDPOINT   12
#define TRB_TYPE_EVALUATE_CONTEXT  13
#define TRB_TYPE_RESET_ENDPOINT    14
#define TRB_TYPE_STOP_ENDPOINT     15
#define TRB_TYPE_SET_TR_DEQUEUE    16
#define TRB_TYPE_RESET_DEVICE      17
#define TRB_TYPE_NOOP_CMD          23

/* Event TRBs */
#define TRB_TYPE_TRANSFER_EVENT    32
#define TRB_TYPE_CMD_COMPLETION    33
#define TRB_TYPE_PORT_STATUS_CHG   34
#define TRB_TYPE_BANDWIDTH_REQ     35
#define TRB_TYPE_HC_EVENT          37
#define TRB_TYPE_DEVICE_NOTIFY     38
#define TRB_TYPE_MFINDEX_WRAP      39

/* ── TRB Control field helpers ───────────────────────────────────────────── */

/* Encode type into control field bits [15:10] */
#define TRB_TYPE_FIELD(t)           ((uint32_t)(t) << 10)

/* Extract type from control field */
#define TRB_GET_TYPE(ctrl)          (((ctrl) >> 10) & 0x3F)

/* Slot ID in command completion / address device TRB control [31:24] */
#define TRB_SLOT_ID(ctrl)           (((ctrl) >> 24) & 0xFF)
#define TRB_SET_SLOT(s)             ((uint32_t)(s) << 24)

/* Endpoint ID in transfer event TRB control [20:16] */
#define TRB_EP_ID(ctrl)             (((ctrl) >> 16) & 0x1F)

/* ── TRB Control flags ───────────────────────────────────────────────────── */

#define TRB_CYCLE               (1u << 0)    /* Cycle bit — matches ring producer */
#define TRB_TOGGLE_CYCLE        (1u << 1)    /* Toggle Cycle on Link TRB */
#define TRB_ISP                 (1u << 2)    /* Interrupt on Short Packet */
#define TRB_ENT                 (1u << 3)    /* Evaluate Next TRB */
#define TRB_CH                  (1u << 4)    /* Chain Bit */
#define TRB_IOC                 (1u << 5)    /* Interrupt On Completion */
#define TRB_IDT                 (1u << 6)    /* Immediate Data (Setup Stage) */
#define TRB_DIR_IN              (1u << 16)   /* Direction IN (Data/Status Stage) */

/* Address Device: BSR (Block Set Address Request) bit */
#define TRB_BSR                 (1u << 9)

/* Setup Stage TRB: transfer type encoding [17:16] */
#define TRB_TRT_NO_DATA         (0u << 16)
#define TRB_TRT_OUT_DATA        (2u << 16)
#define TRB_TRT_IN_DATA         (3u << 16)

/* ── TRB Status field helpers ────────────────────────────────────────────── */

/* Completion Code: bits [31:24] of event TRB status */
#define TRB_GET_CC(status)          (((status) >> 24) & 0xFF)

/* Transfer Length remaining: bits [23:0] of event TRB status */
#define TRB_GET_LEN(status)         ((status) & 0xFFFFFF)

/* Interrupter Target: bits [31:22] of TRB status for non-event TRBs */
#define TRB_INTR_TARGET(n)          ((uint32_t)(n) << 22)

/* TRB Transfer Length field in status [16:0] for transfer TRBs */
#define TRB_XFER_LEN(l)             ((uint32_t)(l) & 0x1FFFF)

/* ── Completion Codes ────────────────────────────────────────────────────── */

#define TRB_CC_INVALID              0
#define TRB_CC_SUCCESS              1
#define TRB_CC_DATA_BUFFER_ERROR    2
#define TRB_CC_BABBLE_DETECTED      3
#define TRB_CC_USB_TRANSACTION_ERR  4
#define TRB_CC_TRB_ERROR            5
#define TRB_CC_STALL_ERROR          6
#define TRB_CC_RESOURCE_ERROR       7
#define TRB_CC_BANDWIDTH_ERROR      8
#define TRB_CC_NO_SLOTS_ERROR       9
#define TRB_CC_INVALID_STREAM_TYPE  10
#define TRB_CC_SLOT_NOT_ENABLED     11
#define TRB_CC_EP_NOT_ENABLED       12
#define TRB_CC_SHORT_PACKET         13
#define TRB_CC_RING_UNDERRUN        14
#define TRB_CC_RING_OVERRUN         15
#define TRB_CC_VF_EVENT_RING_FULL   16
#define TRB_CC_PARAMETER_ERROR      17
#define TRB_CC_BANDWIDTH_OVERRUN    18
#define TRB_CC_CONTEXT_STATE_ERROR  19
#define TRB_CC_NO_PING_RESPONSE     20
#define TRB_CC_EVENT_RING_FULL      21
#define TRB_CC_INCOMPATIBLE_DEVICE  22
#define TRB_CC_MISSED_SERVICE_ERR   23
#define TRB_CC_CMD_RING_STOPPED     24
#define TRB_CC_CMD_ABORTED          25
#define TRB_CC_STOPPED              26
#define TRB_CC_STOPPED_LEN_INVALID  27
#define TRB_CC_MAX_EXIT_LATENCY     29
#define TRB_CC_ISOCH_BUFFER_OVERRUN 31
#define TRB_CC_EVENT_LOST           32
#define TRB_CC_UNDEFINED_ERROR      33
#define TRB_CC_INVALID_STREAM_ID    34
#define TRB_CC_SECONDARY_BANDWIDTH  35
#define TRB_CC_SPLIT_TRANSACTION    36

/* ── Event Ring Segment Table Entry ─────────────────────────────────────── */

typedef struct {
    uint64_t ring_segment_base;    /* Physical address of event ring segment */
    uint16_t ring_segment_size;    /* Number of TRBs in segment */
    uint16_t reserved0;
    uint32_t reserved1;
} __attribute__((packed)) xhci_erst_t;

/* ── xHCI Slot Context (32 bytes) ────────────────────────────────────────── */

typedef struct {
    uint32_t dw0;   /* RouteString[19:0] | Speed[23:20] | MTT[25] | Hub[26] | CtxEntries[31:27] */
    uint32_t dw1;   /* MaxExitLatency[15:0] | RootHubPortNum[23:16] | NumPorts[31:24] */
    uint32_t dw2;   /* TT HubSlotID[7:0] | TTPortNum[15:8] | TTT[17:16] | InterrupterTarget[31:22] */
    uint32_t dw3;   /* DevAddr[7:0] | SlotState[12:8] */
    uint32_t reserved[4];
} __attribute__((packed)) xhci_slot_ctx_t;

/* Slot Context DW0 helpers */
#define SLOT_CTX_SPEED(s)           ((uint32_t)(s) << 20)
#define SLOT_CTX_CTX_ENTRIES(n)     ((uint32_t)(n) << 27)
#define SLOT_CTX_ROOT_PORT(p)       ((uint32_t)(p) << 16)   /* in DW1 */

/* ── xHCI Endpoint Context (32 bytes) ────────────────────────────────────── */

typedef struct {
    uint32_t dw0;   /* EPState[2:0] | Mult[9:8] | MaxPStreams[14:10] | LSA[15] | Interval[23:16] */
    uint32_t dw1;   /* CErr[2:1] | EPType[5:3] | HID[7] | MaxBurstSize[15:8] | MaxPacketSize[31:16] */
    uint64_t tr_dequeue_ptr;  /* TR Dequeue Pointer | DCS[0] */
    uint16_t avg_trb_len;
    uint16_t max_esit_payload;
    uint32_t reserved[3];
} __attribute__((packed)) xhci_ep_ctx_t;

/* EP Type values for EPType[5:3] in DW1 */
#define EP_TYPE_ISOCH_OUT   1
#define EP_TYPE_BULK_OUT    2
#define EP_TYPE_INTR_OUT    3
#define EP_TYPE_CTRL        4
#define EP_TYPE_ISOCH_IN    5
#define EP_TYPE_BULK_IN     6
#define EP_TYPE_INTR_IN     7

/* EP Context DW1 helpers */
#define EP_CTX_EP_TYPE(t)       ((uint32_t)(t) << 3)
#define EP_CTX_MPS(m)           ((uint32_t)(m) << 16)
#define EP_CTX_CERR(c)          ((uint32_t)(c) << 1)
#define EP_CTX_MAX_BURST(b)     ((uint32_t)(b) << 8)

/* ── xHCI Input Control Context (32 bytes) ───────────────────────────────── */

typedef struct {
    uint32_t drop_flags;     /* Drop Context flags [31:0] */
    uint32_t add_flags;      /* Add Context flags [31:0]  */
    uint32_t reserved[6];
} __attribute__((packed)) xhci_input_ctrl_ctx_t;

/* ── xHCI Input Context (1 Input Ctrl + 1 Slot + up to 31 EP contexts) ──── */
/* We only need control endpoint (EP0) for enumeration: 3 × 32 = 96 bytes   */
/* Pad to 128 bytes for standard 32-byte context size controllers            */
typedef struct {
    xhci_input_ctrl_ctx_t ctrl;   /* +0   */
    xhci_slot_ctx_t       slot;   /* +32  */
    xhci_ep_ctx_t         ep[2];  /* +64  — ep[0]=EP0 control, ep[1]=interrupt IN */
} __attribute__((packed)) xhci_input_ctx_t;

/* ── xHCI Device Context ─────────────────────────────────────────────────── */

typedef struct {
    xhci_slot_ctx_t slot;
    xhci_ep_ctx_t   ep[31]; /* EP1..EP31 (EP0 = index 0 = control) */
} __attribute__((packed)) xhci_dev_ctx_t;

#endif /* XHCI_TRB_H */
