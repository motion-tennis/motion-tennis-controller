// SPDX-License-Identifier: MIT
//
// Wi-Fi station: connects with WPA2-PSK, obtains an address via DHCPv4,
// disables power save for low latency and reconnects after a disconnect.

#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/wifi_mgmt.h>

#include "wifi.h"
#include "wifi_credentials.h"

LOG_MODULE_REGISTER(wifi, LOG_LEVEL_INF);

#define WIFI_RECONNECT_DELAY K_SECONDS(5)

#define WIFI_EVENTS (NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT)
#define IPV4_EVENTS (NET_EVENT_IPV4_ADDR_ADD | NET_EVENT_IPV4_ADDR_DEL)

#define EVT_READY BIT(0)

static struct net_mgmt_event_callback wifi_cb;
static struct net_mgmt_event_callback ipv4_cb;
static K_EVENT_DEFINE(wifi_events);

static void connect_work_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(connect_work, connect_work_handler);

static void disable_power_save(struct net_if *iface)
{
	// Modem sleep adds latency to every packet (docs/hardware.md).
	struct wifi_ps_params ps = {
		.enabled = WIFI_PS_DISABLED,
		.type = WIFI_PS_PARAM_STATE,
	};

	if (net_mgmt(NET_REQUEST_WIFI_PS, iface, &ps, sizeof(ps)) != 0) {
		LOG_WRN("Could not disable power save");
	}
}

static void connect_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	struct net_if *iface = net_if_get_wifi_sta();

	if (iface == NULL) {
		iface = net_if_get_default();
	}

	struct wifi_connect_req_params params = {
		.ssid = (const uint8_t *)WIFI_SSID,
		.ssid_length = strlen(WIFI_SSID),
		.psk = (const uint8_t *)WIFI_PASSWORD,
		.psk_length = strlen(WIFI_PASSWORD),
		.security = WIFI_SECURITY_TYPE_PSK,
		.channel = WIFI_CHANNEL_ANY,
		.band = WIFI_FREQ_BAND_2_4_GHZ,
		.mfp = WIFI_MFP_OPTIONAL,
		.timeout = SYS_FOREVER_MS,
	};

	LOG_INF("Connecting to \"%s\"...", WIFI_SSID);
	int rc = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &params, sizeof(params));

	if (rc != 0) {
		LOG_ERR("Connect request failed (%d), retrying", rc);
		k_work_reschedule(&connect_work, WIFI_RECONNECT_DELAY);
	}
}

static void wifi_event_handler(struct net_mgmt_event_callback *cb, uint64_t event,
			       struct net_if *iface)
{
	const struct wifi_status *status = (const struct wifi_status *)cb->info;

	switch (event) {
	case NET_EVENT_WIFI_CONNECT_RESULT:
		if (status->status != 0) {
			LOG_ERR("Connection failed (%d), retrying", status->status);
			k_work_reschedule(&connect_work, WIFI_RECONNECT_DELAY);
		} else {
			LOG_INF("Connected, waiting for DHCP");
			disable_power_save(iface);
		}
		break;
	case NET_EVENT_WIFI_DISCONNECT_RESULT:
		LOG_WRN("Disconnected (%d), reconnecting", status->status);
		k_event_clear(&wifi_events, EVT_READY);
		k_work_reschedule(&connect_work, WIFI_RECONNECT_DELAY);
		break;
	default:
		break;
	}
}

static void ipv4_event_handler(struct net_mgmt_event_callback *cb, uint64_t event,
			       struct net_if *iface)
{
	ARG_UNUSED(cb);

	if (event == NET_EVENT_IPV4_ADDR_ADD) {
		char addr[NET_IPV4_ADDR_LEN];
		struct net_if_ipv4 *ipv4 = iface->config.ip.ipv4;

		if (ipv4 != NULL) {
			net_addr_ntop(AF_INET, &ipv4->unicast[0].ipv4.address.in_addr, addr,
				      sizeof(addr));
			LOG_INF("IPv4 address: %s", addr);
		}
		k_event_post(&wifi_events, EVT_READY);
	} else if (event == NET_EVENT_IPV4_ADDR_DEL) {
		k_event_clear(&wifi_events, EVT_READY);
	}
}

void wifi_start(void)
{
	net_mgmt_init_event_callback(&wifi_cb, wifi_event_handler, WIFI_EVENTS);
	net_mgmt_add_event_callback(&wifi_cb);
	net_mgmt_init_event_callback(&ipv4_cb, ipv4_event_handler, IPV4_EVENTS);
	net_mgmt_add_event_callback(&ipv4_cb);

	k_work_reschedule(&connect_work, K_NO_WAIT);
}

bool wifi_wait_ready(k_timeout_t timeout)
{
	return k_event_wait(&wifi_events, EVT_READY, false, timeout) != 0;
}

bool wifi_is_ready(void)
{
	return (k_event_test(&wifi_events, EVT_READY) & EVT_READY) != 0;
}
