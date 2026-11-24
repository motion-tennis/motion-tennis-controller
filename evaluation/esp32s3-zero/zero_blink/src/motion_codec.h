// SPDX-License-Identifier: MIT
//
// Encodes IMU samples as motion.v03.Envelope { controller_input } (spec/motion.proto).

#ifndef MOTION_CODEC_H_
#define MOTION_CODEC_H_

#include <stddef.h>
#include <stdint.h>

#include "imu.h"

// Maximum UDP payload for controller messages (docs/protocol.md).
#define MOTION_MAX_UDP_BYTES 1200

// Creates a fresh 16-byte sender session ID. Call once at boot.
void motion_codec_init(void);

// Encodes one sample. Returns 0 and sets *out_len, or a negative errno.
int motion_encode_controller_input(const struct imu_sample *sample, uint8_t *buf,
				   size_t buf_size, size_t *out_len);

#endif // MOTION_CODEC_H_
