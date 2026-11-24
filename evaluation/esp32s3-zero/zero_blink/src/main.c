// SPDX-License-Identifier: MIT
//
// zero_blink: lets the onboard WS2812 RGB LED of the Waveshare ESP32-S3-Zero
// cycle through red, green and blue (GPIO21, driven via SPI2 / led_strip).
// Starts Wi-Fi; IMU acquisition (imu.c) and UDP sending (udp_sender.c) run in
// their own threads.
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/led_strip.h>

#include "wifi.h"

#define STRIP_NODE      DT_ALIAS(led_strip)
#define STRIP_NUM_PIXELS DT_PROP(STRIP_NODE, chain_length)
#define BLINK_MS        500
#define BRIGHTNESS      0x20 // keep it low, the LED is very bright

static const struct device *const strip = DEVICE_DT_GET(STRIP_NODE);

static const struct led_rgb colors[] = {
	{ .r = BRIGHTNESS, .g = 0x00, .b = 0x00 },
	{ .r = 0x00, .g = BRIGHTNESS, .b = 0x00 },
	{ .r = 0x00, .g = 0x00, .b = BRIGHTNESS },
};

int main(void)
{
	struct led_rgb pixels[STRIP_NUM_PIXELS];
	size_t i = 0;

	if (!device_is_ready(strip)) {
		printk("LED strip %s is not ready\n", strip->name);
		return 0;
	}
	printk("zero_blink running on %s\n", CONFIG_BOARD_TARGET);
	wifi_start();

	while (1) {
		for (size_t p = 0; p < STRIP_NUM_PIXELS; p++) {
			pixels[p] = colors[i];
		}
		int rc = led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS);

		if (rc) {
			printk("led_strip_update_rgb failed: %d\n", rc);
		}
		i = (i + 1) % ARRAY_SIZE(colors);
		k_msleep(BLINK_MS);
	}
	return 0;
}
