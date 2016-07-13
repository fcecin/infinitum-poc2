// Infinitum:: infinitum module

// The block version field is now laid out in this fashion:
//
// Bits 16-31 (high 16 bits): bits 32-47 of the block timestamp (extra 16 bits)
// Bits 8-15 (middle 8 bits): "dust" threshold vote value "n" [0..255] (dust is: 2^n)
// Bits 0-7 (low 8 bits): block version [0..255]

static const int32_t INFINITUM_BLOCK_VERSION = 1;

