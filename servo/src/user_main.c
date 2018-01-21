/*
 * user_main.c: Main entry-point for the web bootstrap code.
 *
 * Author: Ian Marshall
 * Date: 5/01/2018
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
#include "pwm.h"

#include "espfs.h"
#include "webpages-espfs.h"
#include "cgiwebsocket.h"

#include "string_builder.h"
#include "tcp_ota.h"
#include "udp_debug.h"

#define PWM_PERIOD 20000 // 20ms
#define PWM_MIN 22222    // 1ms
#define PWM_MAX 44444    // 2ms

// Globals.
LOCAL int8_t servo_angle = 0;
LOCAL uint32_t pwm_duty = 0;

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
    // Set up the call back for the status of the WiFi.
    wifi_set_event_handler_cb(wifi_event_cb);
}

/*
 * Sets the servo's position, in the range of [-90 90]. Angle is in degrees.
 */
LOCAL void ICACHE_FLASH_ATTR set_servo(int8_t position) {
	// Ensure the position is in the range of [-90 90] degrees.
	if (position < -90) {
		position = -90;
	} else if (position > 90) {
		position = 90;
	}
	servo_angle = position;

	// Calculate the duty cycle to keep it between 1ms (-90 degs) and 2ms (+90 degs).
	pwm_duty = ((uint32_t)(position + 90) * (PWM_MAX - PWM_MIN) / 180) + PWM_MIN;

	// Set the new PWM duty cycle.
	pwm_set_duty(pwm_duty, 0);
	pwm_start();

	// Send the information to all web socket listeners.
	string_builder *sb = create_string_builder(128);
	if (sb == NULL) {
		os_printf("Unable to create string builder for web socket reply.");
	} else {
		append_string_builder(sb, "{\"angle\": ");
		append_int32_string_builder(sb, servo_angle);
		append_string_builder(sb, ", \"duty\": ");
		append_int32_string_builder(sb, pwm_duty);
		append_string_builder(sb, "}");
		cgiWebsockBroadcast("/ws.cgi", sb->buf, sb->len, WEBSOCK_FLAG_NONE);
		free_string_builder(sb);
	}
}

/*
 * Sets up the pulse width modulation (PWM) for servo control.
 */
LOCAL void ICACHE_FLASH_ATTR init_pwm() {
	uint32_t pwm_info[][3] = {{PERIPHS_IO_MUX_MTDO_U, FUNC_GPIO15, 15}};
	uint32_t servo_duty[1] = {0};
	pwm_init(PWM_PERIOD, servo_duty, 1, pwm_info);
	set_servo(0);
}

/*
 * Processes the reception of a message from a web socket.
 */
void ws_recv(Websock *ws, char *data, int len, int flags) {
	if ((data != NULL) && (len > 0)) {
		// Get the desired angle from the web socket's data.
		int32_t angle = 0;
		int32_t multiplier = 1;
		for (int ii = 0; ii < len; ii++) {
			if ((ii == 0) && (data[ii] == '-')) {
				// We have a negative number.
				multiplier = -1;
			} else if ((data[ii] >= '0') && (data[ii] <= '9')) {
				// We have a numeric digit.
				angle *= 10;
				angle += data[ii] - '0';
			} else {
				// We no longer have an angle.
				break;
			}
		}
		angle *= multiplier;

		// Set the angle.
		set_servo((int8_t)angle);
	}
}

/*
 * Processes the connection for a web socket.
 */
void ws_connected(Websock *ws) {
	ws->recvCb=ws_recv;
}

// The URLs that the HTTP server can handle.
HttpdBuiltInUrl builtInUrls[]={
	{"/", cgiRedirect, "/servo.html"},
	{"/ws.cgi", cgiWebsocket, ws_connected},
	{"*", cgiEspFsHook, NULL}, //Catch-all cgi function for the filesystem
	{NULL, NULL, NULL}
};

/*
 * Entry point for the program. Sets up the microcontroller for use.
 */
void user_init(void) {
	// Initialise the wifi.
	wifi_init();

    // Initialise the OTA flash system.
    ota_init();

    // Initialise the network debugging.
    dbg_init();

	// Initialise the PWM.
	init_pwm();

	// Initialise the HTTP server.
	espFsInit((void*)(webpages_espfs_start));
	httpdInit(builtInUrls, 80);
}
