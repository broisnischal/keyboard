#!/usr/bin/env bash
# Recreate the build environment from a fresh clone of this repo.
#   ./setup.sh   then   ./flash-th40.sh
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
QMK="$ROOT/qmk"

if [[ ! -d $QMK ]]; then
  echo "==> cloning the fork (upstream QMK has no ES32/FS026 port)"
  git clone --depth 1 https://github.com/carlosedp/qmk_firmware.git "$QMK"
  echo "==> submodules (only the two ARM ones; a full init pulls ~4 GB)"
  git -C "$QMK" submodule update --init --recursive --depth 1 lib/chibios lib/chibios-contrib
fi

# The keymap is version-controlled at $ROOT/keymap and symlinked into the tree,
# so the giant clone stays out of git.
LINK="$QMK/keyboards/epomaker/th40/keymaps/tapdance"
mkdir -p "$(dirname "$LINK")"
# relative, so the link keeps working wherever the repo is cloned
[[ -L $LINK || ! -e $LINK ]] && ln -sfn ../../../../../keymap "$LINK"
echo "==> keymap linked: $LINK -> $ROOT/keymap"

for t in arm-none-eabi-gcc make python3; do
  command -v "$t" >/dev/null || echo "MISSING: $t"
done
python3 -c "import hid" 2>/dev/null || echo "MISSING: python hidapi (needed by bin/th40)"

echo "==> building"
make -C "$QMK" -j"$(nproc)" epomaker/th40:tapdance
echo "==> done. Flash with ./flash-th40.sh"
