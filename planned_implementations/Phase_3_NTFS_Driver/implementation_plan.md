# Implementation Plan — NTFS Read-Only Driver (`ntfs.c`)

## Goal

Implement `ntfs.c` to give `ntfs_mount()` a working body. On success, the
returned `vfs_node_t *` exposes the NTFS volume as a read-only VFS tree.
Also wire MBR type `0x07` and the GPT "Basic Data" GUID into `partition.c`
so NTFS volumes auto-mount under `/mnt/` exactly like FAT32 and ext4 do.

---

## Background — NTFS On-Disk Layout

```
LBA 0        Boot sector  (ntfs_bpb_t)
              oem_id = 0x202020205346544E  ("NTFS    " little-endian)
              bytes_per_sector, sectors_per_cluster
              mft_cluster  → LBA of $MFT first record

MFT (Master File Table)
  Record 0  → $MFT (the MFT file itself)
  Record 1  → $MFTMirr
  Record 2  → $LogFile
  Record 3  → $Volume
  Record 4  → $AttrDef
  Record 5  → . (root directory)   ← start here for directory listing

Each MFT record = mft_record_size bytes (usually 1024 B, but use BPB field)
  ntfs_mft_record_t header
  Followed by a chain of attributes (ntfs_attr_header_t + body)
  Terminated by 0xFFFFFFFF type marker

Key attributes for Phase 3:
  0x10  STANDARD_INFORMATION  (timestamps, flags)
  0x30  FILE_NAME             (UTF-16 name, parent ref)
  0x80  DATA                  (file data — resident or non-resident)
  0x90  INDEX_ROOT            (B-tree root for directory)
  0xA0  INDEX_ALLOCATION      (B-tree non-resident for large dirs)
```

---

## Scope

**In scope (Phase 3):**
- Parse BPB, locate $MFT
- Navigate MFT record 5 (root directory `$INDEX_ROOT`)
- Implement `ntfs_readdir` (flat listing of root using index B-tree leaf entries)
- Implement `ntfs_finddir` (walk entries to find by name; UTF-16→ASCII downcast)
- Implement `ntfs_read` for resident `$DATA` attributes
- Implement `ntfs_read` for non-resident `$DATA` via data-run decoding
- Wire MBR type `0x07` into `partition.c:vfs_probe_and_mount()`

**Out of scope (deferred):**
- Write support
- Sparse / compressed files (detect and skip gracefully)
- Hard links, symlinks
- Full recursive subdirectory listing (requires INDEX_ALLOCATION walk)
- Journalling ($LogFile replay)

---

## Files

### [NEW] `kernel/src/fs/ntfs.c`

Single implementation file. Mirrors fat32.c structure: private `ntfs_fs_t`
instance data, static `vfs_ops_t ntfs_ops`, and public `ntfs_mount()`.

#### Internal structs (file-private)

```c
typedef struct {
    disk_partition_t *part;
    uint64_t         mft_lba;         /* LBA of first MFT record */
    uint32_t         mft_record_size; /* Bytes per MFT record (from BPB) */
    uint32_t         cluster_size;    /* Bytes per cluster */
    uint32_t         bytes_per_sector;
} ntfs_fs_t;

typedef struct {
    ntfs_fs_t *fs;
    uint64_t   mft_record_number;   /* Which MFT record represents this node */
    uint64_t   file_size;           /* 0 for directories */
    /* For non-resident DATA: decoded run list */
    uint64_t   data_run_lcn;        /* Starting LCN of first run (simplified) */
    uint64_t   data_run_len_clusters;
} ntfs_node_t;
```

#### Key implementation steps inside `ntfs.c`

**1. `ntfs_mount(disk_partition_t *part)` — entry point**
```
1. Read LBA = part->lba_start (boot sector)
2. Cast to ntfs_bpb_t; verify oem_id == 0x202020205346544E ("NTFS    ")
3. Compute:
     cluster_size    = bpb->bytes_per_sector * bpb->sectors_per_cluster
     mft_lba         = part->lba_start + bpb->mft_cluster * bpb->sectors_per_cluster
     mft_record_size = (bpb->clusters_per_mft_record >= 0)
                         ? (bpb->clusters_per_mft_record * cluster_size)
                         : (1u << (uint8_t)(-bpb->clusters_per_mft_record))
4. Allocate ntfs_fs_t, store above values
5. Build a vfs_node_t for root dir (MFT record 5)
6. Return root vfs_node_t (or NULL on any failure)
```

> [!IMPORTANT]
> `clusters_per_mft_record` in the BPB is a signed byte. When **negative**,
> the actual record size is `1 << (-value)` bytes (e.g., -10 → 1024 bytes).
> When positive, it is a literal cluster count. Both cases must be handled.

**2. `ntfs_read_mft_record(ntfs_fs_t *fs, uint64_t rec_num, uint8_t *buf)`**
```
lba = fs->mft_lba + (rec_num * fs->mft_record_size) / fs->bytes_per_sector
sector_count = fs->mft_record_size / fs->bytes_per_sector (minimum 1)
disk_read_sectors(fs->part->drive, lba, sector_count, buf)
Apply update sequence array fixup (copy last 2 bytes of each 512-byte
sector from the update sequence array in the record header).
```

> [!CAUTION]
> The Update Sequence Array (USA) fixup is **mandatory**. Each 512-byte
> boundary in the raw MFT record has the last 2 bytes replaced with a
> sequence number by NTFS. The correct bytes are stored in the USA.
> Skipping this yields corrupted attribute data.

**3. `ntfs_find_attr(uint8_t *rec_buf, uint32_t type)` → returns pointer inside buf**
```
Walk from record->attrs_offset:
  Read ntfs_attr_header_t
  If header->type == 0xFFFFFFFF: stop, return NULL
  If header->type == type: return pointer to this attribute
  Advance by header->length
```

**4. `ntfs_readdir(vfs_node_t *node, uint32_t index)`**
```
Read MFT record for node->mft_record_number into a buffer
Find INDEX_ROOT attribute (0x90)
The INDEX_ROOT value starts with a 16-byte index root header, then
a 16-byte index header, then index entries.
Each index entry:
  uint64_t  mft_ref         (lower 48 bits = record number; upper 16 = seq)
  uint16_t  entry_length
  uint16_t  key_length
  uint32_t  flags           (bit 1 = last entry)
  [key_length bytes]        = ntfs_attr_file_name_t
Walk entries, skip MFT ref < 16 (system files), skip last-entry flag
Return entry at `index` as dirent_t:
  Convert UTF-16 name to ASCII (simple: take low byte of each char)
  type = (flags & NTFS_RECORD_IS_DIR) ? FS_DIRECTORY : FS_FILE
```

**5. `ntfs_finddir(vfs_node_t *node, const char *name)`**
```
Same as readdir but compare ASCII-downcast name to target
On match: allocate vfs_node_t + ntfs_node_t for child
  child->mft_record_number = mft_ref & 0x0000FFFFFFFFFFFF
  Decode file size from FILE_NAME attr
Return child vfs_node_t
```

**6. `ntfs_read(vfs_node_t *node, uint64_t offset, uint64_t size, uint8_t *buf)`**
```
Read node's MFT record
Find DATA attribute (0x80)
If attr->non_resident == 0 (resident):
  Data sits inline at (attr_ptr + resident->value_offset)
  memcpy directly
If attr->non_resident == 1:
  Decode data runs starting at (attr_ptr + non_res->data_runs_offset)
  Data run encoding (per run):
    byte header: low nibble = length-field bytes, high nibble = offset-field bytes
    Read length_bytes → cluster count for this run
    Read offset_bytes → signed LCN delta (0 = sparse/hole, skip)
    Convert: LBA = (current_LCN + delta) * cluster_size / bytes_per_sector
    Read sectors, copy to output buffer respecting offset/size window
  Advance through runs until size satisfied or all runs consumed
```

---

### [MODIFY] `kernel/src/fs/partition.c`

In `vfs_probe_and_mount()`, add NTFS to the dispatch chain:

```diff
+#include "fs/ntfs.h"

 void vfs_probe_and_mount(disk_partition_t *part) {
     ...
     /* Try FAT32 */
     if (part->os_type == 0x0B || part->os_type == 0x0C ||
         part->os_type == 0x0E || part->os_type == 0xEF) {
         fs_root = fat32_mount(part);
     }
+    /* Try NTFS (MBR type 0x07) — also handles exFAT but we verify magic in ntfs_mount */
+    else if (part->os_type == 0x07) {
+        fs_root = ntfs_mount(part);
+    }
     /* Try Ext4 */
     else if (part->os_type == 0x83) {
         fs_root = ext4_mount(part);
     }
     ...
 }
```

Also add `#include "fs/ntfs.h"` to the includes block.

For GPT partitions (currently all assigned `os_type = 0xEE`), the GPT path
in `partition_parse_gpt()` sets every entry to `0xEE` and calls
`vfs_probe_and_mount()`. NTFS can still be detected there because
`ntfs_mount()` verifies the OEM ID magic — if the magic matches, it mounts;
otherwise returns NULL. This means NTFS GPT partitions auto-detect without
any GUID matching needed at the partition layer. No change needed in
`partition_parse_gpt()`.

---

## x64 Correctness Notes

| # | Issue | Guard |
|---|---|---|
| 1 | MFT record size from `clusters_per_mft_record` negative value | Handle both sign cases in `ntfs_mount()` |
| 2 | USA fixup must be applied before reading any attribute | Done in `ntfs_read_mft_record()` before returning |
| 3 | MFT record buffer must be `mft_record_size` bytes — allocate via `kmalloc`, not stack | Stack may be too small (1024 B) for deeply nested ISR paths |
| 4 | Data run LCN delta is **signed** | Cast offset bytes from run to `int64_t`, not `uint64_t` |
| 5 | UTF-16LE name conversion: only downcast ASCII range (0x00–0x7F) safely | Non-ASCII chars: replace with `'?'` |
| 6 | MFT reference = lower 48 bits of `uint64_t` (upper 16 are sequence number) | Mask: `ref & 0x0000FFFFFFFFFFFF` |

---

## Verification Plan

### Automated Test
```bash
# Boot with an NTFS disk image
make run QEMU_FLAGS="-hdb ntfs_test.img"
```

Create `ntfs_test.img` on the dev machine:
```bash
dd if=/dev/zero of=ntfs_test.img bs=1M count=64
mkfs.ntfs -F ntfs_test.img
mkdir /tmp/mnt_ntfs && mount -o loop ntfs_test.img /tmp/mnt_ntfs
echo "hello from ntfs" > /tmp/mnt_ntfs/hello.txt
mkdir /tmp/mnt_ntfs/subdir
umount /tmp/mnt_ntfs
```

### Manual Verification in Shell
```
ls /mnt/hdb1          → should list hello.txt, subdir
cat /mnt/hdb1/hello.txt  → should print "hello from ntfs"
ls /mnt/hdb1/subdir   → should print empty or its contents
```

### Failure Modes to Check
- Partition with missing/wrong OEM magic → `ntfs_mount` returns NULL, no crash
- Sparse data run (LCN delta = 0) → return zeros for that range, no hang
- Compressed file → log warning, return 0 bytes read (do not crash)
