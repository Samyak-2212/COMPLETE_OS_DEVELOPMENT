/* ============================================================================
 * ntfs.c — NexusOS NTFS Filesystem Driver
 * Purpose: Read-only NTFS partition mounting, directory listing, and file read
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "fs/ntfs.h"
#include "lib/string.h"
#include "lib/printf.h"
#include "mm/heap.h"
#include "fs/partition.h"

/* OEM ID for NTFS: "NTFS    " in little-endian 64-bit */
#define NTFS_OEM_ID  0x202020205346544EULL

/* MFT record magic: "FILE" */
#define NTFS_FILE_MAGIC  0x454C4946U

/* MFT record number of the root directory */
#define NTFS_ROOT_MFT_RECORD  5U

/* System file threshold: MFT records below this are metadata files */
#define NTFS_SYSTEM_FILE_THRESHOLD  16U

/* ---- File-private instance data ------------------------------------------ */

typedef struct {
    disk_partition_t *part;
    uint64_t         mft_lba;           /* LBA of MFT record 0 */
    uint32_t         mft_record_size;   /* Bytes per MFT record */
    uint32_t         cluster_size;      /* Bytes per cluster */
    uint32_t         bytes_per_sector;
} ntfs_fs_t;

typedef struct {
    ntfs_fs_t *fs;
    uint64_t   mft_record_number;
    uint64_t   file_size;
} ntfs_node_t;

/* ---- Forward declarations ------------------------------------------------- */

static dirent_t    *ntfs_readdir(vfs_node_t *node, uint32_t index);
static vfs_node_t  *ntfs_finddir(vfs_node_t *node, const char *name);
static uint64_t     ntfs_read(vfs_node_t *node, uint64_t offset,
                               uint64_t size, uint8_t *buffer);

static vfs_ops_t ntfs_ops;  /* Filled in by ntfs_mount() */

/* ---- Internal helpers ----------------------------------------------------- */

/*
 * ntfs_read_mft_record — read one MFT record into caller-supplied buffer.
 * Applies Update Sequence Array (USA) fixup before returning.
 * buf must be at least fs->mft_record_size bytes.
 * Returns 0 on failure, 1 on success.
 */
static int ntfs_read_mft_record(ntfs_fs_t *fs, uint64_t rec_num, uint8_t *buf)
{
    /* Each MFT record is mft_record_size bytes; compute its starting LBA */
    uint64_t byte_offset  = rec_num * fs->mft_record_size;
    uint64_t lba          = fs->mft_lba + byte_offset / fs->bytes_per_sector;
    uint8_t  sector_count = (uint8_t)(fs->mft_record_size / fs->bytes_per_sector);
    if (sector_count == 0) sector_count = 1;

    if (disk_read_sectors(fs->part->drive, lba, sector_count, buf) == 0)
        return 0;

    /* Validate FILE magic */
    ntfs_mft_record_t *rec = (ntfs_mft_record_t *)buf;
    if (rec->magic != NTFS_FILE_MAGIC)
        return 0;

    /* USA fixup — the update_seq_size includes the sequence number word itself,
     * so there are (update_seq_size - 1) sector end-fixup slots.
     * The USA lives at buf[update_seq_offset]:
     *   word[0] = update sequence number (check value)
     *   word[1..n] = original last-2 bytes of each 512-byte sector  */
    uint16_t *usa      = (uint16_t *)(buf + rec->update_seq_offset);
    uint16_t  seq_num  = usa[0];
    uint16_t  usa_len  = rec->update_seq_size; /* includes the check word */

    for (uint16_t i = 1; i < usa_len; i++) {
        /* Offset of the last 2 bytes of the i-th 512-byte block */
        uint32_t sector_end = i * 512 - 2;
        if (sector_end + 1 >= fs->mft_record_size) break;

        uint16_t *end_word = (uint16_t *)(buf + sector_end);
        /* Verify check value matches (sanity) */
        if (*end_word != seq_num) {
            /* Sequence mismatch — corrupt record; treat as unreadable */
            return 0;
        }
        /* Restore original bytes from USA */
        *end_word = usa[i];
    }

    return 1;
}

/*
 * ntfs_find_attr — scan attribute list in rec_buf for the first attr
 * whose type matches. Returns pointer inside buf, or NULL if not found.
 * rec_buf must have had USA fixup applied.
 */
static uint8_t *ntfs_find_attr(uint8_t *rec_buf, uint32_t type, uint32_t record_size)
{
    ntfs_mft_record_t *rec = (ntfs_mft_record_t *)rec_buf;
    uint32_t           off = rec->attrs_offset;

    while (off + sizeof(ntfs_attr_header_t) <= record_size) {
        ntfs_attr_header_t *attr = (ntfs_attr_header_t *)(rec_buf + off);

        /* End-of-attribute marker */
        if (attr->type == 0xFFFFFFFFU)
            break;

        /* Sanity: attribute length must be non-zero to avoid infinite loop */
        if (attr->length == 0)
            break;

        if (attr->type == type)
            return (uint8_t *)attr;

        off += attr->length;
    }

    return NULL;
}

/*
 * utf16_to_ascii — downcast a UTF-16LE name of 'len' code units into dest[].
 * dest[] must be at least (len + 1) bytes. Non-ASCII replaced with '?'.
 */
static void utf16_to_ascii(char *dest, const uint16_t *src, uint8_t len)
{
    for (uint8_t i = 0; i < len; i++) {
        uint16_t c = src[i];
        dest[i] = (c < 0x80) ? (char)c : '?';
    }
    dest[len] = '\0';
}

/* ---- Index entry traversal helpers --------------------------------------- */

/*
 * Structure of the INDEX_ROOT attribute value:
 *   [0..3]   attribute_type   (uint32)
 *   [4..7]   collation_rule   (uint32)
 *   [8..11]  index_block_size (uint32)
 *   [12]     clusters_per_index_block (uint8)
 *   [13..15] padding
 *   [16..]   index_header (ntfs_index_header_t)
 *
 * The index_header has:
 *   [0..3]  entries_offset    (uint32) — from start of index_header
 *   [4..7]  index_length      (uint32)
 *   [8..11] alloc_size        (uint32)
 *   [12]    flags             (uint8)  — 0x01 = has INDEX_ALLOCATION
 *   [13..15] padding
 *
 * Then index entries start at (index_header_base + entries_offset).
 *
 * Each index entry:
 *   [0..7]   mft_ref         (uint64) — lower 48 bits = record, upper 16 = seq
 *   [8..9]   entry_length    (uint16)
 *   [10..11] key_length      (uint16)
 *   [12..15] flags           (uint32) — bit1 = last entry
 *   [16..]   key = ntfs_attr_file_name_t
 */

#define NTFS_INDEX_ENTRY_LAST  0x02U  /* Flag: this is the end sentinel */

/* Byte layout offsets for INDEX_ROOT value */
#define INDEX_ROOT_HEADER_OFFSET   16U  /* offset of index_header within Index Root value */
#define INDEX_ENTRY_ENTRIES_OFF     0U  /* offset of entries_offset uint32 within index_header */

/* ---- VFS Operations ------------------------------------------------------- */

static dirent_t *ntfs_readdir(vfs_node_t *node, uint32_t index)
{
    ntfs_node_t *nd = (ntfs_node_t *)node->impl;
    if (!nd) return NULL;
    ntfs_fs_t *fs = nd->fs;

    uint8_t *rec_buf = kmalloc(fs->mft_record_size);
    if (!rec_buf) return NULL;

    if (!ntfs_read_mft_record(fs, nd->mft_record_number, rec_buf)) {
        kfree(rec_buf);
        return NULL;
    }

    /* Find INDEX_ROOT attribute */
    uint8_t *attr_ptr = ntfs_find_attr(rec_buf, NTFS_ATTR_INDEX_ROOT, fs->mft_record_size);
    if (!attr_ptr) {
        kfree(rec_buf);
        return NULL;
    }

    /* Get value pointer — INDEX_ROOT is always resident */
    ntfs_resident_attr_t *res = (ntfs_resident_attr_t *)attr_ptr;
    uint8_t *value = attr_ptr + res->value_offset;

    /* Skip the 16-byte index root header to reach the index_header */
    uint8_t  *index_hdr    = value + INDEX_ROOT_HEADER_OFFSET;
    uint32_t  entries_off  = *(uint32_t *)(index_hdr + 0);  /* from index_hdr base */

    /* Pointer to first index entry */
    uint8_t *entry = index_hdr + entries_off;

    static dirent_t d_out;
    uint32_t valid_count = 0;

    /* Walk index entries */
    while (1) {
        uint64_t mft_ref      = *(uint64_t *)(entry + 0);
        uint16_t entry_length = *(uint16_t *)(entry + 8);
        uint16_t key_length   = *(uint16_t *)(entry + 10);
        uint32_t flags        = *(uint32_t *)(entry + 12);

        /* Last entry sentinel */
        if (flags & NTFS_INDEX_ENTRY_LAST)
            break;

        /* Safety: avoid infinite loop on bad entry_length */
        if (entry_length < 16)
            break;

        /* Skip if no key (dummy entry) */
        if (key_length == 0) {
            entry += entry_length;
            continue;
        }

        /* Lower 48 bits of mft_ref = MFT record number */
        uint64_t rec_num = mft_ref & 0x0000FFFFFFFFFFFFULL;

        /* Skip system/metadata files */
        if (rec_num < NTFS_SYSTEM_FILE_THRESHOLD) {
            entry += entry_length;
            continue;
        }

        /* Key is a FILE_NAME attribute body */
        ntfs_attr_file_name_t *fn = (ntfs_attr_file_name_t *)(entry + 16);

        /* Skip non-POSIX name types (Win32 duplicates, DOS names) — prefer
         * POSIX (0) or Win32&DOS (3). name_type: 0=POSIX 1=Win32 2=DOS 3=Win32+DOS
         * We accept all and take the first match at `index`. */

        if (valid_count == index) {
            utf16_to_ascii(d_out.name, fn->name, fn->name_length);
            d_out.ino  = (uint32_t)(rec_num & 0xFFFFFFFFU);
            d_out.type = (fn->flags & NTFS_RECORD_IS_DIR) ? FS_DIRECTORY : FS_FILE;
            kfree(rec_buf);
            return &d_out;
        }
        valid_count++;
        entry += entry_length;
    }

    kfree(rec_buf);
    return NULL;
}

static vfs_node_t *ntfs_finddir(vfs_node_t *node, const char *name)
{
    ntfs_node_t *nd = (ntfs_node_t *)node->impl;
    if (!nd) return NULL;
    ntfs_fs_t *fs = nd->fs;

    uint8_t *rec_buf = kmalloc(fs->mft_record_size);
    if (!rec_buf) return NULL;

    if (!ntfs_read_mft_record(fs, nd->mft_record_number, rec_buf)) {
        kfree(rec_buf);
        return NULL;
    }

    uint8_t *attr_ptr = ntfs_find_attr(rec_buf, NTFS_ATTR_INDEX_ROOT, fs->mft_record_size);
    if (!attr_ptr) {
        kfree(rec_buf);
        return NULL;
    }

    ntfs_resident_attr_t *res = (ntfs_resident_attr_t *)attr_ptr;
    uint8_t  *value       = attr_ptr + res->value_offset;
    uint8_t  *index_hdr   = value + INDEX_ROOT_HEADER_OFFSET;
    uint32_t  entries_off = *(uint32_t *)(index_hdr + 0);
    uint8_t  *entry       = index_hdr + entries_off;

    while (1) {
        uint64_t mft_ref      = *(uint64_t *)(entry + 0);
        uint16_t entry_length = *(uint16_t *)(entry + 8);
        uint16_t key_length   = *(uint16_t *)(entry + 10);
        uint32_t flags        = *(uint32_t *)(entry + 12);

        if (flags & NTFS_INDEX_ENTRY_LAST)
            break;
        if (entry_length < 16)
            break;
        if (key_length == 0) {
            entry += entry_length;
            continue;
        }

        uint64_t rec_num = mft_ref & 0x0000FFFFFFFFFFFFULL;
        if (rec_num < NTFS_SYSTEM_FILE_THRESHOLD) {
            entry += entry_length;
            continue;
        }

        ntfs_attr_file_name_t *fn = (ntfs_attr_file_name_t *)(entry + 16);

        char ascii_name[256];
        utf16_to_ascii(ascii_name, fn->name, fn->name_length);

        if (strcmp(ascii_name, name) == 0) {
            /* Build child vfs_node */
            vfs_node_t *child = kmalloc(sizeof(vfs_node_t));
            memset(child, 0, sizeof(vfs_node_t));
            strcpy(child->name, ascii_name);
            child->flags = (fn->flags & NTFS_RECORD_IS_DIR) ? FS_DIRECTORY : FS_FILE;
            child->length = fn->real_size;
            child->ops   = &ntfs_ops;

            ntfs_node_t *child_nd = kmalloc(sizeof(ntfs_node_t));
            child_nd->fs               = fs;
            child_nd->mft_record_number = rec_num;
            child_nd->file_size        = fn->real_size;
            child->impl = child_nd;

            kfree(rec_buf);
            return child;
        }

        entry += entry_length;
    }

    kfree(rec_buf);
    return NULL;
}

/*
 * ntfs_read — read up to `size` bytes from file node at `offset`.
 * Handles both resident and non-resident $DATA attributes.
 */
static uint64_t ntfs_read(vfs_node_t *node, uint64_t offset,
                            uint64_t size, uint8_t *buffer)
{
    if ((node->flags & FS_FILE) != FS_FILE) return 0;

    ntfs_node_t *nd = (ntfs_node_t *)node->impl;
    if (!nd) return 0;
    ntfs_fs_t *fs = nd->fs;

    if (offset >= nd->file_size) return 0;
    if (offset + size > nd->file_size)
        size = nd->file_size - offset;
    if (size == 0) return 0;

    uint8_t *rec_buf = kmalloc(fs->mft_record_size);
    if (!rec_buf) return 0;

    if (!ntfs_read_mft_record(fs, nd->mft_record_number, rec_buf)) {
        kfree(rec_buf);
        return 0;
    }

    uint8_t *attr_ptr = ntfs_find_attr(rec_buf, NTFS_ATTR_DATA, fs->mft_record_size);
    if (!attr_ptr) {
        kfree(rec_buf);
        return 0;
    }

    ntfs_attr_header_t *hdr = (ntfs_attr_header_t *)attr_ptr;
    uint64_t bytes_read = 0;

    if (hdr->non_resident == 0) {
        /* ---- Resident DATA ---- */
        ntfs_resident_attr_t *res  = (ntfs_resident_attr_t *)attr_ptr;
        uint8_t              *data = attr_ptr + res->value_offset;
        uint32_t              data_len = res->value_length;

        if (offset >= data_len) {
            kfree(rec_buf);
            return 0;
        }
        uint64_t to_copy = data_len - offset;
        if (to_copy > size) to_copy = size;

        memcpy(buffer, data + offset, to_copy);
        bytes_read = to_copy;

    } else {
        /* ---- Non-Resident DATA — decode data runs ---- */
        ntfs_non_resident_attr_t *nres = (ntfs_non_resident_attr_t *)attr_ptr;
        uint8_t *run = attr_ptr + nres->data_runs_offset;

        /* Allocate a cluster-sized I/O buffer */
        uint8_t *cluster_buf = kmalloc(fs->cluster_size);
        if (!cluster_buf) {
            kfree(rec_buf);
            return 0;
        }

        int64_t  current_lcn     = 0;   /* absolute LCN accumulator */
        uint64_t current_vcn     = 0;   /* VCN (byte offset / cluster_size) in file */

        while (*run != 0x00 && bytes_read < size) {
            uint8_t header     = *run++;
            uint8_t len_bytes  = header & 0x0F;   /* nibble: length field width */
            uint8_t off_bytes  = (header >> 4) & 0x0F; /* nibble: offset field width */

            if (len_bytes == 0) break;  /* invalid run */

            /* Decode run length (cluster count) — unsigned */
            uint64_t run_clusters = 0;
            for (uint8_t i = 0; i < len_bytes; i++)
                run_clusters |= ((uint64_t)(*run++)) << (i * 8);

            /* Decode LCN delta — signed */
            int64_t lcn_delta = 0;
            if (off_bytes > 0) {
                for (uint8_t i = 0; i < off_bytes; i++)
                    lcn_delta |= ((int64_t)(*run++)) << (i * 8);
                /* Sign-extend if the high bit of the last byte is set */
                uint8_t high_byte = (uint8_t)(lcn_delta >> ((off_bytes - 1) * 8));
                if (high_byte & 0x80) {
                    /* Fill upper bytes with 0xFF */
                    for (uint8_t i = off_bytes; i < 8; i++)
                        lcn_delta |= (int64_t)0xFF << (i * 8);
                }
            }
            /* off_bytes == 0 → sparse run (LCN delta = 0, data = zeros) */

            /* Absolute LCN of this run */
            current_lcn += lcn_delta;

            /* Run covers VCN range [current_vcn, current_vcn + run_clusters) */
            uint64_t run_start_byte = current_vcn * fs->cluster_size;
            uint64_t run_end_byte   = run_start_byte + run_clusters * fs->cluster_size;

            /* Check if this run overlaps with [offset, offset+size) */
            if (run_end_byte > offset && run_start_byte < offset + size) {
                /* Byte within this run where we start reading */
                uint64_t run_read_start = (offset > run_start_byte)
                                          ? (offset - run_start_byte)
                                          : 0;
                uint64_t run_read_end   = (offset + size < run_end_byte)
                                          ? (offset + size - run_start_byte)
                                          : (run_clusters * fs->cluster_size);

                /* Walk cluster by cluster within this run */
                uint64_t cluster_idx = run_read_start / fs->cluster_size;
                uint64_t intra_off   = run_read_start % fs->cluster_size;

                while (cluster_idx < run_clusters &&
                       cluster_idx * fs->cluster_size < run_read_end &&
                       bytes_read < size)
                {
                    if (off_bytes == 0) {
                        /* Sparse — return zeros */
                        uint64_t to_zero = fs->cluster_size - intra_off;
                        uint64_t remain  = size - bytes_read;
                        if (to_zero > remain) to_zero = remain;
                        memset(buffer + bytes_read, 0, to_zero);
                        bytes_read += to_zero;
                    } else {
                        /* Real cluster */
                        uint64_t lcn_abs = (uint64_t)(current_lcn + (int64_t)cluster_idx);
                        uint64_t sector_lba = fs->part->lba_start
                                            + lcn_abs * (fs->cluster_size / fs->bytes_per_sector);
                        uint8_t secs = (uint8_t)(fs->cluster_size / fs->bytes_per_sector);
                        if (secs == 0) secs = 1;

                        if (disk_read_sectors(fs->part->drive, sector_lba, secs, cluster_buf) == 0)
                            goto done_nonres;

                        uint64_t to_copy = fs->cluster_size - intra_off;
                        uint64_t remain  = size - bytes_read;
                        if (to_copy > remain) to_copy = remain;

                        memcpy(buffer + bytes_read, cluster_buf + intra_off, to_copy);
                        bytes_read += to_copy;
                    }
                    intra_off = 0;
                    cluster_idx++;
                }
            }

            current_vcn += run_clusters;
        }

done_nonres:
        kfree(cluster_buf);
    }

    kfree(rec_buf);
    return bytes_read;
}

/* ---- Mount ---------------------------------------------------------------- */

/*
 * ntfs_mount — validate the boot sector, parse BPB, build root vfs_node_t.
 * Returns NULL on any error (no panic).
 */
vfs_node_t *ntfs_mount(disk_partition_t *part)
{
    /* Read boot sector (512 bytes minimum) */
    uint8_t *sector = kmalloc(512);
    if (!sector) return NULL;

    if (disk_read_sectors(part->drive, part->lba_start, 1, sector) == 0) {
        kfree(sector);
        return NULL;
    }

    ntfs_bpb_t *bpb = (ntfs_bpb_t *)sector;

    /* Verify OEM ID: "NTFS    " = 0x202020205346544E LE */
    if (bpb->oem_id != NTFS_OEM_ID) {
        kfree(sector);
        return NULL;
    }

    /* Sanity: bytes_per_sector must be 512, 1024, 2048, or 4096 */
    uint32_t bps = bpb->bytes_per_sector;
    if (bps < 512 || bps > 4096 || (bps & (bps - 1)) != 0) {
        kfree(sector);
        return NULL;
    }

    uint32_t cluster_size = bps * bpb->sectors_per_cluster;

    /* MFT record size: negative value means 1 << (-value), positive = cluster count */
    uint32_t mft_record_size;
    int8_t c_mft = bpb->clusters_per_mft_record;
    if (c_mft < 0) {
        mft_record_size = 1u << (uint8_t)(-c_mft);
    } else {
        mft_record_size = (uint32_t)c_mft * cluster_size;
    }

    /* MFT starts at bpb->mft_cluster, measured from partition start */
    uint64_t mft_lba = part->lba_start
                     + bpb->mft_cluster * bpb->sectors_per_cluster;

    /* Populate filesystem instance */
    ntfs_fs_t *fs = kmalloc(sizeof(ntfs_fs_t));
    if (!fs) {
        kfree(sector);
        return NULL;
    }
    fs->part            = part;
    fs->mft_lba         = mft_lba;
    fs->mft_record_size = mft_record_size;
    fs->cluster_size    = cluster_size;
    fs->bytes_per_sector = bps;

    kfree(sector);

    /* Wire ops table */
    ntfs_ops.read    = ntfs_read;
    ntfs_ops.write   = NULL;        /* Read-only */
    ntfs_ops.readdir = ntfs_readdir;
    ntfs_ops.finddir = ntfs_finddir;
    ntfs_ops.open    = NULL;
    ntfs_ops.close   = NULL;
    ntfs_ops.mkdir   = NULL;

    /* Build root vfs_node (MFT record 5 = root directory) */
    vfs_node_t *root = kmalloc(sizeof(vfs_node_t));
    if (!root) {
        kfree(fs);
        return NULL;
    }
    memset(root, 0, sizeof(vfs_node_t));
    strcpy(root->name, "NTFS");
    root->flags = FS_DIRECTORY;
    root->ops   = &ntfs_ops;

    ntfs_node_t *nd = kmalloc(sizeof(ntfs_node_t));
    if (!nd) {
        kfree(root);
        kfree(fs);
        return NULL;
    }
    nd->fs               = fs;
    nd->mft_record_number = NTFS_ROOT_MFT_RECORD;
    nd->file_size        = 0;
    root->impl = nd;

    kprintf_set_color(0x0088FF88, 0x001A1A2E);
    kprintf("[NTFS] ");
    kprintf_set_color(0x00CCCCCC, 0x001A1A2E);
    kprintf("Mounted NTFS volume (Cluster: %u B, MFT record: %u B)\n",
            cluster_size, mft_record_size);

    return root;
}
