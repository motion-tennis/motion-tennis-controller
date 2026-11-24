// SPDX-License-Identifier: MIT

#ifndef WIFI_H_
#define WIFI_H_

#include <stdbool.h>
#include <zephyr/kernel.h>

// Starts the station connection with the credentials from wifi_credentials.h.
// Reconnects automatically after a disconnect.
void wifi_start(void);

// Waits until the station is connected and has an IPv4 address.
// Returns true if the network is ready within the timeout.
bool wifi_wait_ready(k_timeout_t timeout);

// True while connected with an IPv4 address.
bool wifi_is_ready(void);

#endif // WIFI_H_
