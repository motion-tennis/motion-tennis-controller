// SPDX-License-Identifier: MIT

#ifndef IMU_H_
#define IMU_H_

#include <stdbool.h>
#include <stdint.h>
#include <zephyr/kernel.h>

// One regular IMU sampling boundary (one controller_input transaction).
struct imu_sample {
	uint64_t sample_id;     // Starts at 1, increments per sampling boundary
	uint64_t t0_us;         // Sampling boundary on the local monotonic clock
	uint64_t capture_us;    // Time the sensor data was read
	float accel_m_s2[3];    // Specific force including gravity
	float gyro_rad_s[3];    // Angular velocity
	bool saturated;         // Any axis at or near the configured full scale
	bool simulated;         // Generated data instead of the MPU-6050
};

// Filled by the IMU thread, consumed by the UDP sender thread.
extern struct k_msgq imu_msgq;

// Number of samples that could not be queued because the queue was full.
uint32_t imu_overloaded_count(void);

#endif // IMU_H_
