#!/usr/bin/env bash
set -e
cd /mnt/c/Users/naska/OneDrive/Documents/GitHub/COMPLETE_OS_DEVELOPMENT

echo "[1] Creating 64 MiB image..."
dd if=/dev/zero of=ntfs_test.img bs=1M count=64 2>&1

echo "[2] Writing MBR partition table (type 0x07)..."
echo '2048,,7' | sfdisk ntfs_test.img

echo "[3] Attaching loop device..."
LOOP=$(echo '112233' | sudo -S losetup -fP --show ntfs_test.img)
echo "    Loop: $LOOP"

echo "[4] Formatting NTFS on ${LOOP}p1..."
echo '112233' | sudo -S mkfs.ntfs -F -Q ${LOOP}p1

echo "[5] Mounting and writing test files..."
mkdir -p /tmp/mnt_ntfs
echo '112233' | sudo -S mount ${LOOP}p1 /tmp/mnt_ntfs
echo 'hello' | sudo -S tee /tmp/mnt_ntfs/test.txt
echo '112233' | sudo -S mkdir -p /tmp/mnt_ntfs/subdir
echo '112233' | sudo -S umount /tmp/mnt_ntfs

echo "[6] Detaching loop..."
echo '112233' | sudo -S losetup -d $LOOP

echo "DONE -- verifying with ntfsls:"
LOOP2=$(echo '112233' | sudo -S losetup -fP --show ntfs_test.img)
ntfsls --offset $((2048*512)) ntfs_test.img || true
echo '112233' | sudo -S losetup -d $LOOP2 2>/dev/null || true
echo "Image ready."
