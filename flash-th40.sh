#!/usr/bin/env bash
# Flash the Epomaker TH40 (QMK version, 36b0:304e). See keyboard.md for the full runbook.
set -euo pipefail

# Only one flasher at a time. Two waiting in parallel both wake on the same
# bootloader and write FIRMWARE.BIN to the same volume, which corrupts the
# image and leaves the board sitting in the bootloader.
exec 9>/tmp/th40-flash.lock
if ! flock -n 9; then
  echo "another flash-th40.sh is already waiting - refusing to run a second" >&2
  exit 1
fi

QMK=/home/nees/key/qmk
BIN=$QMK/.build/epomaker_th40_tapdance.bin

[[ "${1:-}" == "--no-build" ]] || { echo "==> building"; make -C "$QMK" -j"$(nproc)" epomaker/th40:tapdance; }
[[ -f $BIN ]] || { echo "no firmware at $BIN" >&2; exit 1; }

# Opt-in only: the raw-HID bootloader jump reboots the board but lands back in
# the application, not the bootloader (see keyboard.md §5a). Off by default so a
# flash doesn't pointlessly reconnect the keyboard first.
if [[ ${TH40_TRY_SOFT_JUMP:-0} == 1 ]] && ! lsusb | grep -q 03eb:2045; then
  echo "==> asking the keyboard to reboot into its bootloader (experimental)"
  ~/.local/bin/th40-claude-status bootloader || true
  for _ in $(seq 1 8); do lsusb | grep -q 03eb:2045 && break; sleep 1; done
fi

if ! lsusb | grep -q 03eb:2045; then
  echo "==> waiting up to ${TH40_WAIT:-240}s for bootloader (03eb:2045)"
  echo "    switch to wired mode, unplug, hold the top-left key, plug back in"
  for _ in $(seq 1 "${TH40_WAIT:-240}"); do lsusb | grep -q 03eb:2045 && break; sleep 1; done
fi
lsusb | grep -q 03eb:2045 || { echo "bootloader never appeared" >&2; exit 1; }

# Identify by model string rather than assuming /dev/sda. The kernel creates the
# block device a second or two after the USB device enumerates, so poll for it.
DEV=""
for _ in $(seq 1 20); do
  DEV=$(lsblk -ndo PATH,MODEL | awk '/RDMCTMZT/ {print $1; exit}')
  [[ -n $DEV ]] && break
  sleep 1
done
[[ -n $DEV ]] || { echo "bootloader is up but no block device appeared" >&2; exit 1; }
echo "==> bootloader volume: $DEV"

# The desktop auto-mounts this volume, and it may win the race between our
# findmnt check and our mount attempt - in which case udisksctl fails with
# AlreadyMounted and prints nothing to stdout. So: re-check after every attempt.
MNT=""
for _ in $(seq 1 10); do
  MNT=$(findmnt -no TARGET "$DEV" 2>/dev/null || true)
  [[ -n $MNT ]] && break
  MNT=$(udisksctl mount -b "$DEV" 2>/dev/null | sed -n 's/.* at \(.*\)\.$/\1/p')
  [[ -n $MNT ]] && break
  sleep 1
done
[[ -d $MNT ]] || { echo "could not mount $DEV" >&2; exit 1; }

echo "==> writing $(stat -c%s "$BIN") bytes to $MNT/FIRMWARE.BIN"
# The board flashes on write and reboots itself, so these are expected to be noisy.
cp "$BIN" "$MNT/FIRMWARE.BIN" || true
sync || true
udisksctl unmount -b "$DEV" 2>/dev/null || true

echo "==> waiting for the keyboard to come back"
for _ in $(seq 1 30); do lsusb | grep -q 36b0:304e && break; sleep 1; done
echo "==> running firmware identity (expect EPOMAKER / 0.04, not RDMCTMZT / 0.05):"
lsusb -d 36b0:304e -v 2>/dev/null | grep -E "bcdDevice|iManufacturer" || echo "    keyboard not detected"
