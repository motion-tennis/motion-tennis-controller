// SPDX-License-Identifier: MIT
//
// IMU acquisition thread: reads the MPU-6050 (or generates test data) at a fixed
// rate and queues one sample per sampling boundary for the UDP sender.

#include <math.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

#include "imu.h"

LOG_MODULE_REGISTER(imu, LOG_LEVEL_INF);

// Sampling rate. The specification (spec/reference-profile.json, controller_hz)
// requires 200 Hz; 10 Hz is used for readable console tests.
#define IMU_SAMPLE_RATE_HZ 10

// 1: send generated data (no MPU-6050 needed), 0: read the real sensor.
#define IMU_USE_SIMULATED_DATA 1

// Full-scale ranges, must match accel-fs / gyro-fs in the overlay.
#define IMU_ACCEL_FULL_SCALE_M_S2 (16.0f * 9.80665f)
#define IMU_GYRO_FULL_SCALE_RAD_S (2000.0f * 3.14159265f / 180.0f)
#define IMU_SATURATION_FRACTION   0.98f

// Pending capacity from the specification (16 entries = 80 ms at 200 Hz).
#define IMU_QUEUE_LENGTH 16

#define IMU_THREAD_STACK_SIZE 2048
#define IMU_THREAD_PRIORITY   5

K_MSGQ_DEFINE(imu_msgq, sizeof(struct imu_sample), IMU_QUEUE_LENGTH, 8);
K_TIMER_DEFINE(imu_timer, NULL, NULL);

static atomic_t overloaded;

uint32_t imu_overloaded_count(void)
{
	return (uint32_t)atomic_get(&overloaded);
}

static uint64_t now_us(void)
{
	return k_ticks_to_us_floor64(k_uptime_ticks());
}

static bool is_saturated(const struct imu_sample *s)
{
	for (int i = 0; i < 3; i++) {
		if (fabsf(s->accel_m_s2[i]) >= IMU_SATURATION_FRACTION * IMU_ACCEL_FULL_SCALE_M_S2 ||
		    fabsf(s->gyro_rad_s[i]) >= IMU_SATURATION_FRACTION * IMU_GYRO_FULL_SCALE_RAD_S) {
			return true;
		}
	}
	return false;
}

#if IMU_USE_SIMULATED_DATA

// Generated data: device at rest with gravity on Z, plus a slow sinusoidal
// "swing" on X and a rotation around Z, so changing values are visible.
static int read_sample(struct imu_sample *s)
{
	float t = (float)s->t0_us / 1e6f;
	float swing = sinf(2.0f * 3.14159265f * 0.5f * t); // 0.5 Hz

	s->accel_m_s2[0] = 5.0f * swing;
	s->accel_m_s2[1] = 0.0f;
	s->accel_m_s2[2] = 9.80665f;
	s->gyro_rad_s[0] = 0.0f;
	s->gyro_rad_s[1] = 0.0f;
	s->gyro_rad_s[2] = 2.0f * swing;
	s->capture_us = now_us();
	s->simulated = true;
	return 0;
}

static bool imu_init(void)
{
	LOG_INF("Using simulated IMU data");
	return true;
}

#else // IMU_USE_SIMULATED_DATA

static const struct device *const imu = DEVICE_DT_GET(DT_ALIAS(imu0));

static int read_sample(struct imu_sample *s)
{
	struct sensor_value accel[3];
	struct sensor_value gyro[3];

	s->capture_us = now_us();
	int rc = sensor_sample_fetch(imu);

	if (rc != 0) {
		return rc;
	}
	sensor_channel_get(imu, SENSOR_CHAN_ACCEL_XYZ, accel);
	sensor_channel_get(imu, SENSOR_CHAN_GYRO_XYZ, gyro);
	for (int i = 0; i < 3; i++) {
		s->accel_m_s2[i] = (float)sensor_value_to_double(&accel[i]);
		s->gyro_rad_s[i] = (float)sensor_value_to_double(&gyro[i]);
	}
	s->simulated = false;
	return 0;
}

static bool imu_init(void)
{
	if (!device_is_ready(imu)) {
		LOG_ERR("%s not ready, check wiring and I2C address", imu->name);
		return false;
	}
	LOG_INF("Using %s", imu->name);
	return true;
}

#endif // IMU_USE_SIMULATED_DATA

static void imu_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	uint64_t sample_id = 0;
	uint32_t missed = 0;
	uint32_t errors = 0;

	if (!imu_init()) {
		return;
	}
	LOG_INF("Sampling at %d Hz", IMU_SAMPLE_RATE_HZ);

	k_timer_start(&imu_timer, K_USEC(USEC_PER_SEC / IMU_SAMPLE_RATE_HZ),
		      K_USEC(USEC_PER_SEC / IMU_SAMPLE_RATE_HZ));

	while (true) {
		// Blocks until the next sampling boundary; > 1 means boundaries were missed.
		uint32_t expired = k_timer_status_sync(&imu_timer);
		struct imu_sample s = { 0 };

		s.t0_us = now_us();
		s.sample_id = ++sample_id;
		if (expired > 1) {
			missed += expired - 1;
			LOG_WRN("Missed %u sampling boundaries (total %u)", expired - 1, missed);
		}

		int rc = read_sample(&s);

		if (rc != 0) {
			errors++;
			LOG_WRN("Sample %llu: read failed (%d), errors=%u", s.sample_id, rc, errors);
			continue;
		}
		s.saturated = is_saturated(&s);

		// Never block the sampling thread; a full queue is counted as overload.
		if (k_msgq_put(&imu_msgq, &s, K_NO_WAIT) != 0) {
			atomic_inc(&overloaded);
		}
	}
}

K_THREAD_DEFINE(imu_tid, IMU_THREAD_STACK_SIZE, imu_thread, NULL, NULL, NULL,
		IMU_THREAD_PRIORITY, 0, 0);
