/*
 * user_main.c: Main entry-point for the doorbell-of-things.
 *
 * Author: Ian Marshall
 * Date: 18/06/2016
 */
#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"    
#include "ip_addr.h"
#include "espconn.h"
#include "user_interface.h"
#include "espmissingincludes.h"
#include "tcp_ota.h"
#include "udp_debug.h"

// Change the below values to suit your own network.
#define SSID "YOUR_NETWORK_SSID"
#define PASSWD "YOUR_NETWORK_PASSWORD"

// The host name of the Pushbullet server - we will use DNS to get the IP address later.
#define PB_HOSTNAME "api.pushbullet.com"

// The HTTP contents and length for the request to Pushbullet - note the token and channel have been masked out.
#define PB_REQUEST "POST https://api.pushbullet.com/v2/pushes HTTP/1.0\r\n" \
    "Access-Token: xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\r\nContent-Type: application/json\r\nContent-Length: 88\r\n\r\n" \
    "{\"channel_tag\":\"xxxx\",\"type\":\"note\",\"title\":\"Doorbell\",\"body\":\"Doorbell has been rung.\"}"
#define PB_REQUEST_LEN 247

// The priority for the task used to disconnect the Pushbullet connection.
#define PB_DISCONNECT_PRI 0

// The queue length for the task used to disconnect the Pushbullet connection.
#define PB_DISCONNECT_QUEUE_LEN 2

// The connection used for calling the Pushbullet API.
LOCAL struct espconn pb_conn;

// The TCP protocol structure used for calling the Pushbullet API.
LOCAL esp_tcp pb_proto;

// The IP address of the Pushbullet API web server.
LOCAL ip_addr_t pb_ip;

// Flag used to determine if a Pushbullet call is already underway, to avoid duplicate/overlapping calls.
LOCAL bool pb_in_progress = false;

// The queue used for posting event to the Pushbullet disconnect queue.
LOCAL os_event_t pb_queue[PB_DISCONNECT_QUEUE_LEN];

/*
 * Call-back for when we get a response from the Pushbullet API web server.
 */
LOCAL void ICACHE_FLASH_ATTR pb_response_cb(void *arg, char *data, uint16_t len) {
    struct espconn *conn = (struct espconn *)arg;

    // Make sure the reply starts with "HTTP/1.? "
    if ((data[0] != 'H') ||(data[1] != 'T') ||(data[2] != 'T') || (data[3] != 'P') || (data[4] != '/') ||
            (data[5] != '1') || (data[6] != '.') || (data[8] != ' ')) {
        
    }

    // Get the status code from the response.
    uint16_t status = 0;
    for (uint16_t ii = 9; ii < len; ii++) {
        if ((data[ii] >= '0') && (data[ii] <= '9')) {
            // This is a digit, update the status code.
            status = (status * 10) + (data[ii] - '0');
        } else {
            // Stop at the first non-digit.
            break;
        }
    }

    if (status != 200) {
        // There was a problem.
        os_printf("Error returned from Pushbullet: \"%s\".\n", data);
    }

    // Close the connection ASAP, now we're done with it.
    system_os_post(PB_DISCONNECT_PRI, 0, 0);
}

LOCAL void ICACHE_FLASH_ATTR pb_disconnect_task(os_event_t *event) {
    int8_t res = espconn_secure_disconnect(&pb_conn);
    pb_in_progress = false;
}

/*
 * Call-back for when we have a secure connection to the Pushbullet API web server. Now we can make our request.
 */
LOCAL void ICACHE_FLASH_ATTR pb_connect_cb(void *arg) {
    struct espconn *conn = (struct espconn *)arg;
    os_printf("Connected to Pushbullet API web server.\n");

    // Register a call-back for when we receive data.
    espconn_regist_recvcb(conn, pb_response_cb);

    // Send through the Pushbullet request.
    int8_t res = espconn_secure_send(conn, PB_REQUEST, PB_REQUEST_LEN);
    os_printf("Sent %s with result %d.\n", PB_REQUEST, res);
}

/*
 * Call-back for when a Pushbullet TCP connection has been disconnected.
 */
LOCAL void ICACHE_FLASH_ATTR pb_disc_cb(void *arg) {
    pb_in_progress = false;
    os_printf("Disconnected from Pushbullet.\n");
}

/*
 * Call-back for when a Pushbullet connection has failed - reconnected is a misleading name, sadly.
 */
LOCAL void ICACHE_FLASH_ATTR pb_recon_cb(void *arg, int8_t err) {
    pb_in_progress = false;
    os_printf("Connection failed to Pushbullet - %d.\n", err);
}

/*
 * Call-back for when we have the IP address of the Pushbullet API web server. From here, we can connect to the server.
 */
LOCAL void ICACHE_FLASH_ATTR have_pb_ip(const char *name, ip_addr_t *addr, void *arg) {
    struct espconn *conn = (struct espconn *)arg;

    if (addr == NULL) {
        // We couldn't get the IP after all.
        os_printf("Unable to get IP address for Pushbullet.\n");
        pb_in_progress = false;
        return;
    }

    // Set up the connection structure for the Pushbullet API web server.
    conn->type = ESPCONN_TCP;
    conn->state = ESPCONN_NONE;
    conn->proto.tcp = &pb_proto;
    conn->proto.tcp->remote_port = 443;
    os_memcpy(conn->proto.tcp->remote_ip, &addr->addr, 4);

    // Connect to the Pushbullet API web server.
    espconn_regist_connectcb(conn, pb_connect_cb);
    espconn_regist_disconcb(conn, pb_disc_cb);
    espconn_regist_reconcb(conn, pb_recon_cb);

    os_printf("Connecting to %d.%d.%d.%d:%d.\n", IP2STR(&addr->addr), conn->proto.tcp->remote_port);
    espconn_secure_set_size(0x01, 6144);
    int8_t res = espconn_secure_connect(conn);
    if (res) {
        pb_in_progress = false;
        switch (res) {
            case ESPCONN_MEM:
                os_printf("Unable to connect to Pushbullet server - out of memory.\n");
                break;
            case ESPCONN_ISCONN:
                os_printf("Unable to connect to Pushbullet server - already connected.\n");
                break;
            case ESPCONN_ARG:
                os_printf("Unable to connect to Pushbullet server - illegal argument.\n");
                break;
            default:
                os_printf("Unable to connect to Pushbullet server - unknown error.\n");
                break;
        }
    }
}

/*
 * Handles the interrupt of a GPIO pin. As we only have GPIO 5 interrupted, we know what it was - the doorbell has 
 * been pressed. Get Pushbullet to notify all listeners of this event.
 *
 */
LOCAL void gpio_interrupt(uint32_t intr_mask, void *arg) {
    // Get the interrupt information.
    uint32_t gpio_status = GPIO_REG_READ(GPIO_STATUS_ADDRESS);
    gpio_intr_ack(intr_mask);
    os_printf("GPIO interrupt - %04x, %04x.\n", intr_mask, gpio_status);

    // Notify Pushbullet, if we're not already.
    if (!pb_in_progress) {
        pb_in_progress = true;
        espconn_gethostbyname(&pb_conn, PB_HOSTNAME, &pb_ip, have_pb_ip);
    } else {
        os_printf("Not sending to pushbullet, as call is currently in progress.\n");
    }

    // Re-assert the interrupt for this pin.
    gpio_pin_intr_state_set(GPIO_ID_PIN(5), GPIO_PIN_INTR_NEGEDGE);
}

/*
 * Call-back for changes in the WIFi connection's state.
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
 * Sets up the WiFi interface on the ESP-8266.
 */
LOCAL void ICACHE_FLASH_ATTR wifi_init() {
    // Set station mode - we will talk to a WiFi router.
    wifi_set_opmode_current(STATION_MODE);

    // Set up the network name and password.
    struct station_config sc;
    strncpy(sc.ssid, SSID, 32);
    strncpy(sc.password, PASSWD, 64);
    wifi_station_set_config(&sc);
    wifi_station_dhcpc_start();

    // Set up the call back for the status of the WiFi.
    wifi_set_event_handler_cb(wifi_event_cb);
}

/*
 * Entry point for the program. Sets up the microcontroller for use.
 */
void user_init(void) {
    // Initialise the serial port, output only.
    uart_div_modify(0, UART_CLK_FREQ / 115200);

    // Start the network.
    wifi_init();

    // Initialise the OTA flash system.
    ota_init();

    // Initialise the network debugging.
    dbg_init();

    // Set up a task to disconnect the Pushbullet connection on demand.
    system_os_task(pb_disconnect_task, PB_DISCONNECT_PRI, pb_queue, PB_DISCONNECT_QUEUE_LEN);

    // Initialise all GPIOs.
    gpio_init();

    // Set up GPIO 5 is an input.
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO5_U, FUNC_GPIO5);
    gpio_output_set(0, 0, 0, BIT5);
    PIN_PULLUP_EN(PERIPHS_IO_MUX_GPIO5_U);

    // Make GPIO 5 trigger an interrupt on a high->low transition.
    ETS_GPIO_INTR_DISABLE();
    gpio_intr_handler_register(gpio_interrupt, NULL);
    gpio_pin_intr_state_set(GPIO_ID_PIN(5), GPIO_PIN_INTR_NEGEDGE);
    ETS_GPIO_INTR_ENABLE();
}
