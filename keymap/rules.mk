VIA_ENABLE = yes
DYNAMIC_KEYMAP_ENABLE = yes

TAP_DANCE_ENABLE = yes
REPEAT_KEY_ENABLE = yes
CAPS_WORD_ENABLE = yes
COMBO_ENABLE = yes
SECURE_ENABLE = yes
SWAP_HANDS_ENABLE = yes
RGB_MATRIX_CUSTOM_USER = yes

# LEADER is off by request (it also forced Ctrl to be a tap dance, which made
# every Ctrl chord late). Combos stay ON - they cost latency on their member keys
# but they are wanted; see the note above key_combos[] in keymap.c.
# LTO: this bootloader has failed to boot every image with >~83 KB of real code; LTO keeps us far below that.
LTO_ENABLE = yes
KEY_OVERRIDE_ENABLE = yes
LAYER_LOCK_ENABLE = yes
KEY_LOCK_ENABLE = yes
DYNAMIC_MACRO_ENABLE = yes
AUTO_SHIFT_ENABLE = no
AUTOCORRECT_ENABLE = no
LEADER_ENABLE = no
