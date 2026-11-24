// SPDX-License-Identifier: MIT
//
// UDP sender thread: takes queued IMU samples, encodes each one as
// motion.v03.Envelope { controller_input } and sends it once to the receiver.
// Input UDP is not retransmitted (docs/controller.md).

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>

#include "app_config.h"
#include "imu.h"
#include "motion_codec.h"
#include "wifi.h"

LOG_MODULE_REGISTER(udp_sender, LOG_LEVEL_INF);

// Print one status line per N samples (1 at 10 Hz; e.g. 20 at 200 Hz).
#define SENDER_PRINT_DIVIDER 1

#define SENDER_THREAD_STACK_SIZE 4096
#define SENDER_THREAD_PRIORITY   6

static uint8_t tx_buf[MOTION_MAX_UDP_BYTES];

static int open_socket(struct sockaddr_in *dst)
{
	dst->sin_family = AF_INET;
	dst->sin_port = htons(APP_RECEIVER_PORT);
	if (zsock_inet_pton(AF_INET, APP_RECEIVER_IPV4, &dst->sin_addr) != 1) {
		LOG_ERR("Invalid receiver address %s", APP_RECEIVER_IPV4);
		return -EINVAL;
	}

	int sock = zsock_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	if (sock < 0) {
		LOG_ERR("socket() failed (%d)", errno);
		return -errno;
	}
	return sock;
}

static void sender_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	struct sockaddr_in dst;
	struct imu_sample s;
	uint32_t sent = 0;
	uint32_t not_sent = 0;
	int sock;

	motion_codec_init();

	sock = open_socket(&dst);
	if (sock < 0) {
		return;
	}
	LOG_INF("Sending controller_input to %s:%d", APP_RECEIVER_IPV4, APP_RECEIVER_PORT);

	while (true) {
		k_msgq_get(&imu_msgq, &s, K_FOREVER);

		size_t len = 0;
		int rc = motion_encode_controller_input(&s, tx_buf, sizeof(tx_buf), &len);

		if (rc != 0) {
			not_sent++;
			LOG_ERR("Sample %llu: encode failed (%d)", s.sample_id, rc);
			continue;
		}

		if (!wifi_is_ready()) {
			// Encoded but no network: counted, not buffered for later.
			not_sent++;
		} else if (zsock_sendto(sock, tx_buf, len, ZSOCK_MSG_DONTWAIT,
					(struct sockaddr *)&dst, sizeof(dst)) < 0) {
			not_sent++;
			LOG_WRN("Sample %llu: sendto failed (%d)", s.sample_id, errno);
		} else {
			sent++;
		}

		if ((s.sample_id % SENDER_PRINT_DIVIDER) == 0) {
			printk("#%llu %s %u B | a %6.2f %6.2f %6.2f | g %5.2f %5.2f %5.2f%s | "
			       "sent=%u not_sent=%u overload=%u\n",
			       s.sample_id, s.simulated ? "SIM" : "IMU", (unsigned int)len,
			       (double)s.accel_m_s2[0], (double)s.accel_m_s2[1],
			       (double)s.accel_m_s2[2], (double)s.gyro_rad_s[0],
			       (double)s.gyro_rad_s[1], (double)s.gyro_rad_s[2],
			       s.saturated ? " SAT" : "", sent, not_sent, imu_overloaded_count());
		}
	}
}

K_THREAD_DEFINE(sender_tid, SENDER_THREAD_STACK_SIZE, sender_thread, NULL, NULL, NULL,
		SENDER_THREAD_PRIORITY, 0, 0);
