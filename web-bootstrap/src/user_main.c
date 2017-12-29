/*
 * user_main.c: Main entry-point for the web bootstrap code.
 *
 * Author: Ian Marshall
 * Date: 12/12/2017
 */
#include "esp8266.h"
#include "httpd.h"
#include "httpdespfs.h"
#include "ets_sys.h"
#include "osapi.h"
#include "mem.h"
#include "gpio.h"
#include "os_type.h"
#include "ip_addr.h"
#include "espconn.h"
#include "user_interface.h"
#include "espmissingincludes.h"

#include "espfs.h"
#include "captdns.h"
#include "webpages-espfs.h"
#include "cgiwifi.h"

#include "tcp_ota.h"
#include "string_builder.h"

/*
 * Returns a HTML page, with an appropriate error code and string.
 */
LOCAL void httpCodeReturn(HttpdConnData *connData, uint16_t code, char *title, char *message) {
	// Write the header.
	httpdStartResponse(connData, code);
	httpdHeader(connData, "Content-Type", "text/html");
	httpdEndHeaders(connData);

	// Write the body message.
	httpdSend(connData, "<html><head><title>", -1);
	httpdSend(connData, title, -1);
	httpdSend(connData, "</title></head><body><p>", -1);
	httpdSend(connData, message, -1);
	httpdSend(connData, "</p></body></html>", -1);
}

/*
 * CGI function to return the current status of the WiFi connection as JSON data.
 */
LOCAL int cgi_wifi_status(HttpdConnData *connData) {
	string_builder *sb = create_string_builder(128);
	if (sb == NULL) {
		httpCodeReturn(connData, 500, "Resource error", "Unable to allocate internal memory for request.");
		return HTTPD_CGI_DONE;
	}

	// Get the operating mode status.
	uint8_t mode = wifi_get_opmode_default();
	append_string_builder(sb, "{\"opmode\": \"");
	switch (mode) {
		case STATION_MODE:
			append_string_builder(sb, "Station");
			break;
		case SOFTAP_MODE:
			append_string_builder(sb, "Access Point");
			break;
		case STATIONAP_MODE:
			append_string_builder(sb, "Station and Access Point");
			break;
		default:
			append_string_builder(sb, "Unknown");
			break;
	}

	// Get the access point information.
    struct ip_info info;
	bool res = wifi_get_ip_info(SOFTAP_IF, &info);
	if (!res) {
		append_string_builder(sb, "\", \"ap\": { \"ip\": \"Unknown\"");
	} else {
		char buf[20];
		os_sprintf(buf, IPSTR, IP2STR(&info.ip));
		append_string_builder(sb, "\", \"ap\": { \"ip\": \"");
		append_string_builder(sb, buf);
		append_string_builder(sb, "\"");
	}
	uint8_t mac[6];
	char mac_str[18];
	wifi_get_macaddr(SOFTAP_IF, mac);
	append_string_builder(sb, ", \"mac\": \"");
	os_sprintf(mac_str, MACSTR, MAC2STR(mac));
	append_string_builder(sb, mac_str);
	int32_t clients = (int32_t)wifi_softap_get_station_num();
	append_string_builder(sb, "\", \"clientCount\": ");
	append_int32_string_builder(sb, clients);
	append_string_builder(sb, "}");

	// Get the station information.
	append_string_builder(sb, ", \"station\": { \"status\": ");
	int stnStatus = wifi_station_get_connect_status();
	switch (stnStatus) {
		case STATION_IDLE:
			append_string_builder(sb, "\"Idle\"");
			break;
		case STATION_CONNECTING:
			append_string_builder(sb, "\"Connecting\"");
			break;
		case STATION_WRONG_PASSWORD:
			append_string_builder(sb, "\"Incorrect password\"");
			break;
		case STATION_NO_AP_FOUND:
			append_string_builder(sb, "\"Access point not found\"");
			break;
		case STATION_CONNECT_FAIL:
			append_string_builder(sb, "\"Connection failed\"");
			break;
		case STATION_GOT_IP:
			append_string_builder(sb, "\"Connected\", \"ip\": \"");
			res = wifi_get_ip_info(STATION_IF, &info);
			if (!res) {
				append_string_builder(sb, "Unknown\"");
			} else {
				char buf[20];
				os_sprintf(buf, IPSTR, IP2STR(&info.ip));
				append_string_builder(sb, buf);
				append_string_builder(sb, "\"");
			}
			break;
	}
	struct station_config config;
	res = wifi_station_get_config(&config);
	if (res) {
		append_string_builder(sb, ", \"ssid\": \"");
		append_string_builder(sb, config.ssid);
		append_string_builder(sb, "\"");
	}
	wifi_get_macaddr(STATION_IF, mac);
	append_string_builder(sb, ", \"mac\": \"");
	os_sprintf(mac_str, MACSTR, MAC2STR(mac));
	append_string_builder(sb, mac_str);
	append_string_builder(sb, "\", \"rssi\": ");
	int8_t rssi = wifi_station_get_rssi();
	if (rssi == 31) {
		append_string_builder(sb, "\"Unknown\" }");
	} else {
		append_int32_string_builder(sb, (int32_t)rssi);
		append_string_builder(sb, " }");
	}

	// Send the JSON response.
	append_string_builder(sb, "}");
	httpdStartResponse(connData, 200);
	httpdHeader(connData, "Content-Type", "text/json");
	httpdEndHeaders(connData);
	httpdSend(connData, sb->buf, sb->len);
	free_string_builder(sb);
	return HTTPD_CGI_DONE;
}

/*
 * CGI function to configure the network, connecting to a station if required.
 * All required parameters are sourced from the HTML connection.
 */
int ICACHE_FLASH_ATTR cgi_connect_network(HttpdConnData *connData) {
	// Get the settings.
	char essid[33];
	char passwd[65];
	char mode_str[8];

	if (connData->conn==NULL) {
		//Connection aborted. Clean up.
		return HTTPD_CGI_DONE;
	}
	
	// Get the new mode for the network.
	int res = httpdFindArg(connData->post->buff, "mode", mode_str, sizeof(mode_str));
	if (res == -1) {
		// We didn't get a mode, which we require.
		httpCodeReturn(connData, 400, "Missing parameter", "Missing the \"mode\" parameter.");
		return HTTPD_CGI_DONE;
	}
	int mode = atoi(mode_str);
	switch (mode) {
		case STATION_MODE: // Fall through
		case STATIONAP_MODE:
			// We're going to have a station mode, so we'll be wanting the username/password.
			if (httpdFindArg(connData->post->buff, "essid", essid, sizeof(essid)) == -1) {
				// No ESSID has been set.
				httpCodeReturn(connData, 400, "Missing parameter", "Missing the \"essid\" parameter.");
				return HTTPD_CGI_DONE;
			}
			if (httpdFindArg(connData->post->buff, "passwd", passwd, sizeof(passwd)) == -1) {
				// No ESSID has been set.
				httpCodeReturn(connData, 400, "Missing parameter", "Missing the \"passwd\" parameter.");
				return HTTPD_CGI_DONE;
			}

			// Set the mode.
			wifi_set_opmode(mode);
			if (mode == STATION_MODE) {
				// Override the mode to temporarily include soft AP mode for this loader.
				wifi_set_opmode_current(STATIONAP_MODE);
			}

			// Set the configuration.
			struct station_config config;
			config.bssid_set = 0;
			os_memcpy(&config.ssid, essid, 32);
			os_memcpy(&config.password, passwd, 64);
			wifi_station_dhcpc_stop();
			wifi_station_disconnect();
			wifi_station_set_config(&config);
			wifi_station_connect();
			wifi_station_dhcpc_start();
			break;
		case SOFTAP_MODE:
			// Set the mode in flash to soft AP mode, but temporarily allow station mode for this loader.
			// Including station mode allows for scanning of local wifi networks.
			wifi_set_opmode(SOFTAP_MODE);
			wifi_set_opmode_current(STATIONAP_MODE);
			break;
	}

	httpdRedirect(connData, "/net/networks.html");
	return HTTPD_CGI_DONE;
}

/*
 * Call-back for when we have an event from the wireless internet connection.
 */
LOCAL void ICACHE_FLASH_ATTR wifi_event_cb(System_Event_t *event) {
    struct ip_info info;

    // To determine what actually happened, we need to look at the event.
    switch (event->event) {
        case EVENT_STAMODE_CONNECTED: {
            // We are connected as a station, but we don't have an IP address yet.
            char ssid[33];
            uint8_t len = event->event_info.connected.ssid_len;
            if (len > 32) {
                len = 32;
            }
            strncpy(ssid, event->event_info.connected.ssid, len + 1);
            os_printf("Received EVENT_STAMODE_CONNECTED. "
                      "SSID = %s, BSSID = "MACSTR", channel = %d.\n",
                      ssid, MAC2STR(event->event_info.connected.bssid), event->event_info.connected.channel);
            break;
        }
        case EVENT_STAMODE_DISCONNECTED: {
            // We have been disconnected as a station.
            char ssid[33];
            uint8_t len = event->event_info.connected.ssid_len;
            if (len > 32) {
                len = 32;
            }
            strncpy(ssid, event->event_info.connected.ssid, len + 1);
            os_printf("Received EVENT_STAMODE_DISCONNECTED. "
                      "SSID = %s, BSSID = "MACSTR", channel = %d.\n",
                      ssid, MAC2STR(event->event_info.disconnected.bssid), event->event_info.disconnected.reason);
            break;
        }
        case EVENT_STAMODE_GOT_IP:
            // We have an IP address, ready to run. Return the IP address, too.
            os_printf("Received EVENT_STAMODE_GOT_IP. IP = "IPSTR", mask = "IPSTR", gateway = "IPSTR"\n", 
                      IP2STR(&event->event_info.got_ip.ip.addr), 
                      IP2STR(&event->event_info.got_ip.mask.addr),
                      IP2STR(&event->event_info.got_ip.gw));
            break;
        case EVENT_STAMODE_DHCP_TIMEOUT:
            // We couldn't get an IP address via DHCP, so we'll have to try re-connecting.
            os_printf("Received EVENT_STAMODE_DHCP_TIMEOUT.\n");
            wifi_station_disconnect();
            wifi_station_connect();
            break;
    }
}

/*
 * Sets up the WiFi interface on the ESP8266.
 */
LOCAL void ICACHE_FLASH_ATTR wifi_init() {
	// Enter station + soft AP mode.
	wifi_set_opmode_current(STATIONAP_MODE);

	// Try and get the DHCP information from the access point.
	wifi_station_dhcpc_start();

    // Set up the call back for the status of the WiFi.
    wifi_set_event_handler_cb(wifi_event_cb);
}

// The URLs that the HTTP server can handle.
HttpdBuiltInUrl builtInUrls[]={
	{"/", cgiRedirect, "/net/networks.html"},
	{"/net", cgiRedirect, "/net/networks.html"},
	{"/net/", cgiRedirect, "/net/networks.html"},
	{"/net/scan.cgi", cgiWiFiScan, NULL},
	{"/net/status.cgi", cgi_wifi_status, NULL},
	{"/net/connect.cgi", cgi_connect_network, NULL},
	{"*", cgiEspFsHook, NULL}, //Catch-all cgi function for the filesystem
	{NULL, NULL, NULL}
};

/*
 * Entry point for the program. Sets up the microcontroller for use.
 */
void user_init(void) {
	// Initialise the UART.
	uart_div_modify(0, UART_CLK_FREQ / 19200);
	os_printf("Starting up web bootstrap.\n");

	// Initialise the wifi.
	os_printf("Initialising the WiFi.\n");
	wifi_init();

	// Initialise the HTTP server.
	os_printf("Initialising HTTP server.\n");
	captdnsInit();
	espFsInit((void*)(webpages_espfs_start));
	httpdInit(builtInUrls, 80);

    // Initialise the OTA flash system.
	os_printf("Initialising OTA.\n");
    ota_init();
	
	os_printf("Web Bootstrap initialisation complete..\n");
}
