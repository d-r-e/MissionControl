/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
uint32_t mc_stadia_max_map_bytes(void);
// Returns1 only if the production parser validates the known Stadia input size.
int mc_stadia_validate_map(const uint8_t *data, size_t size);
// Packed ID | (type << 8); -1 for invalid descriptor. No synthetic reference.
int mc_hogp_parse_reference(const uint8_t *data, size_t size);
// -1 for an invalid reference or one absent from the discovered map.
int mc_hogp_reference_size(const uint8_t *map, size_t map_size,
                           const uint8_t *reference, size_t reference_size);
#ifdef __cplusplus
}
#endif
