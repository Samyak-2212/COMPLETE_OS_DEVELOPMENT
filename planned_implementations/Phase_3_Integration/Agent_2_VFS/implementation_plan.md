# Implementation Plan — Agent 2: VFS & Filesystem Architect

## Role & Expertise
You are a **Senior File Systems Architect**. Your expertise lies in VFS node routing, path resolution algorithms, and binary parsing of on-disk structures (MBR, FAT, Ext). You write clean, modular code that handles nested directory structures with ease.

## Objective
Finalize the Virtual Filesystem (VFS) and enable automatic discovery and mounting of physical partitions.

## Proposed Changes

### 1. VFS Core
#### [MODIFY] [vfs.c](file:///c:/Users/naska/OneDrive/Documents/GitHub/COMPLETE_OS_DEVELOPMENT/kernel/src/fs/vfs.c)
- Implement `vfs_resolve_path(const char *path)`:
    - Tokenize paths into segments.
    - Traverse nodes via `finddir` until the target is reached.
- Implement `vfs_mount(const char *path, vfs_node_t *root)`:
    - Add logic to overlay a filesystem at a specific VFS path.

### 2. Partition Management
#### [MODIFY] [partition.c](file:///c:/Users/naska/OneDrive/Documents/GitHub/COMPLETE_OS_DEVELOPMENT/kernel/src/fs/partition.c)
- Expand `partition_parse_mbr` to correctly identify Extended Partitions.
- Implement `partition_parse_gpt` for UEFI/Modern disk support.
- Automatically trigger a VFS mount probe for every valid partition found.

### 3. File Systems (ReadOnly)
#### [MODIFY] [ramfs.c](file:///c:/Users/naska/OneDrive/Documents/GitHub/COMPLETE_OS_DEVELOPMENT/kernel/src/fs/ramfs.c)
- Implement `vfs_mkdir` and `vfs_touch` for the root volatile filesystem.

#### [MODIFY] [fat32.c](file:///c:/Users/naska/OneDrive/Documents/GitHub/COMPLETE_OS_DEVELOPMENT/kernel/src/fs/fat32.c)
- Implement cluster-chain walking in `fat32_read`.
- Ensure directory traversal (`readdir/finddir`) handles the full volume.

#### [MODIFY] [ext4.c](file:///c:/Users/naska/OneDrive/Documents/GitHub/COMPLETE_OS_DEVELOPMENT/kernel/src/fs/ext4.c)
- Verify `ext4_get_physical_block` handles large file extents.

## Verification Plan
1. **Root Hierarchy**: Verify `/dev`, `/tmp`, and `/mnt` exist on boot (via `vfs_readdir("/")`).
2. **Mount Logic**: Connect a FAT32 image in QEMU and verify it appears under `/mnt/hda1`.
3. **Traversal**: Read a nested file (e.g., `cat /mnt/hda1/docs/readme.txt`).
