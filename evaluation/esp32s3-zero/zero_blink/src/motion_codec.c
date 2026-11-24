// SPDX-License-Identifier: MIT

#include <errno.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>

#include <pb_encode.h>
#include "proto/motion.pb.h"

#include "app_config.h"
#include "motion_codec.h"

LOG_MODULE_REGISTER(motion_codec, LOG_LEVEL_INF);

#define PROTOCOL_MAJOR 0
#define PROTOCOL_MINOR 3
#define UUID_SIZE      16
// Every uint64 on the wire is limited to 2^56 - 1 (docs/protocol.md).
#define WIRE_UINT64_MAX ((1ULL << 56) - 1)

static uint8_t session_id[UUID_SIZE];
static uint64_t message_seq;

// The Envelope is a large union (all message types); keep it out of thread stacks.
static motion_v03_Envelope envelope;

void motion_codec_init(void)
{
	// A new session at every boot; IDs and counters are never reused across restarts.
	sys_rand_get(session_id, sizeof(session_id));
	message_seq = 0;
	LOG_HEXDUMP_INF(session_id, sizeof(session_id), "Sender session ID");
}

static uint64_t now_us(void)
{
	return k_ticks_to_us_floor64(k_uptime_ticks());
}

int motion_encode_controller_input(const struct imu_sample *sample, uint8_t *buf,
				   size_t buf_size, size_t *out_len)
{
	if (sample->sample_id > WIRE_UINT64_MAX || message_seq >= WIRE_UINT64_MAX) {
		// The session must be replaced before reaching the wire limit.
		return -EOVERFLOW;
	}

	envelope = (motion_v03_Envelope)motion_v03_Envelope_init_zero;

	envelope.major = PROTOCOL_MAJOR;
	envelope.minor = PROTOCOL_MINOR;
	envelope.sender_id = APP_CONTROLLER_ID;
	envelope.sender_session_id.size = UUID_SIZE;
	memcpy(envelope.sender_session_id.bytes, session_id, UUID_SIZE);
	envelope.message_seq = ++message_seq;
	envelope.clock_id = APP_CLOCK_ID;
	// run_id is assigned by the control plane for a measurement run; not available
	// yet, so a 16-byte all-zero placeholder is sent.
	envelope.run_id.size = UUID_SIZE;

	envelope.which_body = motion_v03_Envelope_controller_input_tag;
	motion_v03_ControllerInput *in = &envelope.body.controller_input;

	in->has_transaction = true;
	in->transaction.controller_id = APP_CONTROLLER_ID;
	in->transaction.session_id.size = UUID_SIZE;
	memcpy(in->transaction.session_id.bytes, session_id, UUID_SIZE);
	in->transaction.request_seq = sample->sample_id;

	in->t0_us = sample->t0_us;
	in->sample_id = sample->sample_id;

	// Polling read: the time the driver read the data, not a hardware capture.
	// Uncertainty is one sampling period of the sensor (200 Hz).
	in->has_capture = true;
	in->capture.value_us = sample->capture_us;
	in->capture.clock_id = APP_CLOCK_ID;
	in->capture.kind = motion_v03_TimeKind_DRIVER_FRAME;
	in->capture.uncertainty_us = 5000;
	in->capture.timing_valid = true;

	// The MPU-6050 has no sensor timestamp; 0 (field absent).
	in->original_sensor_ticks = 0;
	// Association and calibration are not implemented yet (0 = not set).
	in->association_version = 0;
	in->calibration_id = 0;

	in->has_specific_force_m_s2 = true;
	in->specific_force_m_s2.x = sample->accel_m_s2[0];
	in->specific_force_m_s2.y = sample->accel_m_s2[1];
	in->specific_force_m_s2.z = sample->accel_m_s2[2];
	in->has_angular_velocity_rad_s = true;
	in->angular_velocity_rad_s.x = sample->gyro_rad_s[0];
	in->angular_velocity_rad_s.y = sample->gyro_rad_s[1];
	in->angular_velocity_rad_s.z = sample->gyro_rad_s[2];
	in->sensor_saturated = sample->saturated;
	// No battery measurement yet (0 = unknown).
	in->battery_millivolts = 0;

	// Set last so it is as close as possible to the actual send.
	envelope.sent_time_us = now_us();

	pb_ostream_t stream = pb_ostream_from_buffer(buf, MIN(buf_size, MOTION_MAX_UDP_BYTES));

	if (!pb_encode(&stream, motion_v03_Envelope_fields, &envelope)) {
		LOG_ERR("Encoding failed: %s", PB_GET_ERROR(&stream));
		return -EMSGSIZE;
	}
	*out_len = stream.bytes_written;
	return 0;
}
