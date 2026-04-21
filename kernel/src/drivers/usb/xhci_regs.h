/* ============================================================================
 * xhci_regs.h — NexusOS xHCI MMIO Register Offsets
 * Purpose: All xHCI register offsets relative to MMIO base (constants only)
 * Reference: xHCI Specification 1.2, Section 5
 * Author: NexusOS Driver Agent (Phase 4, Task 4.7)
 * ============================================================================ */

#ifndef XHCI_REGS_H
#define XHCI_REGS_H

/* ── Capability Registers (at MMIO base) ────────────────────────────────── */
#define XHCI_CAP_CAPLENGTH      0x00   /* 1 byte: capability register length */
#define XHCI_CAP_HCIVERSION     0x02   /* 2 bytes: interface version number */
#define XHCI_CAP_HCSPARAMS1     0x04   /* 4 bytes: structural parameters 1 */
#define XHCI_CAP_HCSPARAMS2     0x08   /* 4 bytes: structural parameters 2 */
#define XHCI_CAP_HCSPARAMS3     0x0C   /* 4 bytes: structural parameters 3 */
#define XHCI_CAP_HCCPARAMS1     0x10   /* 4 bytes: capability parameters 1 */
#define XHCI_CAP_DBOFF          0x14   /* 4 bytes: doorbell array offset */
#define XHCI_CAP_RTSOFF         0x18   /* 4 bytes: runtime register space offset */
#define XHCI_CAP_HCCPARAMS2     0x1C   /* 4 bytes: capability parameters 2 */

/* HCSPARAMS1 field extraction */
#define XHCI_HCSP1_MAXSLOTS(x)  ((x) & 0xFF)
#define XHCI_HCSP1_MAXINTRS(x)  (((x) >> 8) & 0x7FF)
#define XHCI_HCSP1_MAXPORTS(x)  (((x) >> 24) & 0xFF)

/* HCCPARAMS1: xECP field (extended capabilities pointer) */
#define XHCI_HCCP1_XECP(x)     (((x) >> 16) & 0xFFFF)

/* ── Operational Registers (at MMIO base + CAPLENGTH) ───────────────────── */
#define XHCI_OP_USBCMD          0x00   /* USB Command */
#define XHCI_OP_USBSTS          0x04   /* USB Status */
#define XHCI_OP_PAGESIZE        0x08   /* Page Size */
#define XHCI_OP_DNCTRL          0x14   /* Device Notification Control */
#define XHCI_OP_CRCR_LO        0x18   /* Command Ring Control (low 32) */
#define XHCI_OP_CRCR_HI        0x1C   /* Command Ring Control (high 32) */
#define XHCI_OP_DCBAAP_LO      0x30   /* Device Context Base Address Array Pointer (low) */
#define XHCI_OP_DCBAAP_HI      0x34   /* Device Context Base Address Array Pointer (high) */
#define XHCI_OP_CONFIG          0x38   /* Configure: MaxSlotsEn[7:0] */

/* Port Register Sets: op_base + 0x400 + port_index * 0x10 */
#define XHCI_PORT_BASE          0x400
#define XHCI_PORT_STRIDE        0x10
#define XHCI_PORTSC(n)          (XHCI_PORT_BASE + (n) * XHCI_PORT_STRIDE + 0x00)
#define XHCI_PORTPMSC(n)        (XHCI_PORT_BASE + (n) * XHCI_PORT_STRIDE + 0x04)
#define XHCI_PORTLI(n)          (XHCI_PORT_BASE + (n) * XHCI_PORT_STRIDE + 0x08)
#define XHCI_PORTHLPMC(n)       (XHCI_PORT_BASE + (n) * XHCI_PORT_STRIDE + 0x0C)

/* USBCMD bits */
#define XHCI_USBCMD_RUN         (1u << 0)    /* Run/Stop */
#define XHCI_USBCMD_HCRST       (1u << 1)    /* Host Controller Reset */
#define XHCI_USBCMD_INTE        (1u << 2)    /* Interrupter Enable */
#define XHCI_USBCMD_HSEE        (1u << 3)    /* Host System Error Enable */
#define XHCI_USBCMD_EWE        (1u << 10)    /* Enable Wrap Event */

/* USBSTS bits */
#define XHCI_USBSTS_HCH         (1u << 0)    /* Host Controller Halted */
#define XHCI_USBSTS_HSE         (1u << 2)    /* Host System Error */
#define XHCI_USBSTS_EINT        (1u << 3)    /* Event Interrupt */
#define XHCI_USBSTS_PCD         (1u << 4)    /* Port Change Detect */
#define XHCI_USBSTS_CNR         (1u << 11)   /* Controller Not Ready */

/* PORTSC bits */
#define XHCI_PORTSC_CCS         (1u << 0)    /* Current Connect Status */
#define XHCI_PORTSC_PED         (1u << 1)    /* Port Enabled/Disabled */
#define XHCI_PORTSC_OCA         (1u << 3)    /* Over-current Active */
#define XHCI_PORTSC_PR          (1u << 4)    /* Port Reset */
#define XHCI_PORTSC_PLS_MASK    (0xFu << 5)  /* Port Link State */
#define XHCI_PORTSC_PP          (1u << 9)    /* Port Power */
#define XHCI_PORTSC_SPEED_MASK  (0xFu << 10) /* Port Speed */
#define XHCI_PORTSC_SPEED_SHIFT 10
#define XHCI_PORTSC_CSC         (1u << 17)   /* Connect Status Change */
#define XHCI_PORTSC_PEC         (1u << 18)   /* Port Enable/Disable Change */
#define XHCI_PORTSC_WRC         (1u << 19)   /* Warm Port Reset Change */
#define XHCI_PORTSC_OCC         (1u << 20)   /* Over-current Change */
#define XHCI_PORTSC_PRC         (1u << 21)   /* Port Reset Change */
#define XHCI_PORTSC_PLC         (1u << 22)   /* Port Link State Change */
#define XHCI_PORTSC_CEC         (1u << 23)   /* Config Error Change */
#define XHCI_PORTSC_WPR         (1u << 31)   /* Warm Port Reset */
/* Mask of all RW1C change bits — write 1 to clear */
#define XHCI_PORTSC_CHANGE_BITS (XHCI_PORTSC_CSC | XHCI_PORTSC_PEC | \
                                  XHCI_PORTSC_WRC | XHCI_PORTSC_OCC | \
                                  XHCI_PORTSC_PRC | XHCI_PORTSC_PLC | \
                                  XHCI_PORTSC_CEC)

/* PORTSC speed values */
#define XHCI_SPEED_FS   1   /* Full-Speed  (12 Mb/s)  */
#define XHCI_SPEED_LS   2   /* Low-Speed   (1.5 Mb/s) */
#define XHCI_SPEED_HS   3   /* High-Speed  (480 Mb/s) */
#define XHCI_SPEED_SS   4   /* SuperSpeed  (5 Gb/s)   */

/* ── Runtime Registers (at MMIO base + RTSOFF) ──────────────────────────── */
#define XHCI_RT_MFINDEX         0x000   /* Microframe Index */
/* Interrupter n register set: RTSOFF + 0x020 + n*0x20 */
#define XHCI_IMAN(n)            (0x020 + (n) * 0x20 + 0x00) /* Interrupt Management */
#define XHCI_IMOD(n)            (0x020 + (n) * 0x20 + 0x04) /* Interrupt Moderation */
#define XHCI_ERSTSZ(n)          (0x020 + (n) * 0x20 + 0x08) /* Event Ring Segment Table Size */
/* 0x0C: reserved */
#define XHCI_ERSTBA_LO(n)       (0x020 + (n) * 0x20 + 0x10) /* ERST Base Address (low) */
#define XHCI_ERSTBA_HI(n)       (0x020 + (n) * 0x20 + 0x14) /* ERST Base Address (high) */
#define XHCI_ERDP_LO(n)         (0x020 + (n) * 0x20 + 0x18) /* Event Ring Dequeue Pointer (low) */
#define XHCI_ERDP_HI(n)         (0x020 + (n) * 0x20 + 0x1C) /* high */

/* IMAN bits */
#define XHCI_IMAN_IP            (1u << 0)   /* Interrupt Pending (RW1C) */
#define XHCI_IMAN_IE            (1u << 1)   /* Interrupt Enable */

/* ERDP EHB bit: Event Handler Busy */
#define XHCI_ERDP_EHB           (1u << 3)

/* ── Doorbell Registers (at MMIO base + DBOFF) ──────────────────────────── */
/* DB[0] = host command doorbell. DB[slot] = transfer doorbell. */
#define XHCI_DB(n)              ((n) * 4u)

/* ── CRCR bits ───────────────────────────────────────────────────────────── */
#define XHCI_CRCR_RCS           (1ULL << 0)  /* Ring Cycle State */
#define XHCI_CRCR_CS            (1ULL << 1)  /* Command Stop */
#define XHCI_CRCR_CA            (1ULL << 2)  /* Command Abort */
#define XHCI_CRCR_CRR           (1ULL << 3)  /* Command Ring Running */

/* ── Extended Capability IDs ─────────────────────────────────────────────── */
#define XHCI_XCAP_ID_LEGACY     0x01   /* USB Legacy Support */
#define XHCI_XCAP_ID_PROTO      0x02   /* Supported Protocol */

/* USB Legacy Support register offsets relative to xECP */
#define XHCI_USBLEGSUP_OS_OWN   (1u << 24)  /* OS Owned semaphore */
#define XHCI_USBLEGSUP_BIOS_OWN (1u << 16)  /* BIOS Owned semaphore */

#endif /* XHCI_REGS_H */
