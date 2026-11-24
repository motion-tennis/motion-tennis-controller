// SPDX-License-Identifier: MIT
//
// Application settings for the controller test firmware.
// Wi-Fi credentials are kept separately in wifi_credentials.h (not committed).

#ifndef APP_CONFIG_H_
#define APP_CONFIG_H_

// Provisioned numeric node ID of this controller (Envelope.sender_id and
// Transaction.controller_id). Must be nonzero and unique per controller.
#define APP_CONTROLLER_ID 1

// Identifier of this controller's local monotonic clock (Envelope.clock_id and
// ControllerInput.capture.clock_id). Placeholder until the clock-mapping contract
// assigns IDs.
#define APP_CLOCK_ID 1

// Receiver of controller_input messages (the Raspberry Pi), see docs/protocol.md.
// For tests this can be any PC on the same network running a UDP listener.
#define APP_RECEIVER_IPV4 "192.168.1.100"
#define APP_RECEIVER_PORT 41000

#endif // APP_CONFIG_H_
