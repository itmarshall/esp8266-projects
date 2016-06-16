/*
 * user_main.c: Main entry-point for demonstrating OTA firmware upgrades via TCP/IP (not HTTP).
 *
 * Author: Ian Marshall
 * Date: 26/05/2016
 */
#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"    
#include "ip_addr.h"
#include "espconn.h"
#include "user_interface.h"
#include "driver/uart.h"
#include "espmissingincludes.h"
#include "tcp_ota.h"
#include "udp_debug.h"

// Change the below values to suit your own network.
#define SSID "YOUR_NETWORK_SSID"
#define PASSWD "YOUR_NETWORK_PASSWORD"

// Timer used to determine when the LED is to be turned on/off.
LOCAL os_timer_t blink_timer;

// The current state of the LED's output.
LOCAL uint8_t led_state = 0;

// Structure holding the TCP connection information.
LOCAL struct espconn tcp_conn;

// TCP specific protocol structure.
LOCAL esp_tcp tcp_proto;

// Structure holding the UDP connection information.
LOCAL struct espconn udp_conn;

// UDP specific protocol structure.
LOCAL esp_udp udp_proto;

// Connection used to transmit UDP packets.
LOCAL struct espconn udp_tx;

// UDP specific procotol structure used for transmitting UDP packets.
LOCAL esp_udp udp_proto_tx;

// The IP address of the last system to send us any data.
LOCAL uint32_t last_addr = 0;

/*
 * Sends a UDP packet to the given IP address.
 */
LOCAL void ICACHE_FLASH_ATTR udp_tx_data(uint8_t *data, uint16_t len, uint32_t ip_addr) {
    // Set the destination IP address and port.
    os_memcpy(udp_proto_tx.remote_ip, &ip_addr, 4);
    udp_proto_tx.remote_port = 1234;

    // Prepare the UDP "connection" structure.
    udp_tx.type = ESPCONN_UDP;
    udp_tx.state = ESPCONN_NONE;
    udp_tx.proto.udp = &udp_proto_tx;

    // Send the UDP packet.
    espconn_create(&udp_tx);
    espconn_send(&udp_tx, data, len);
    espconn_delete(&udp_tx);
}

/*
 * Call-back for when the blink timer expires. This toggles the GPIO 4 state, and send a UDP packet about the new state
 * to whoever sent us a packet last.
 */
LOCAL void ICACHE_FLASH_ATTR blink_cb(void *arg) {
    // Update the LED's status.
    led_state = !led_state;
    GPIO_OUTPUT_SET(4, led_state);

    // Send a UDP packet with the new LED's state to the last address to send anything to us, if any.
    if (last_addr != 0) {
        char message[16];
        int len = os_sprintf(message, "LED state - %d.\n", led_state);
        udp_tx_data(message, len, last_addr);
    }
}

/*
 * Sets the interval of the timer controlling the blinking of the LED.
 */
LOCAL void ICACHE_FLASH_ATTR set_blink_timer(uint16_t interval) {
    // Start a timer for the flashing of the LED on GPIO 4, running continuously.
    os_timer_disarm(&blink_timer);
    os_timer_setfn(&blink_timer, (os_timer_func_t *)blink_cb, (void *)0);
    os_timer_arm(&blink_timer, interval, 1);
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
            set_blink_timer(2000);
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
            set_blink_timer(4000);
            last_addr = 0;
            break;
        }
        case EVENT_STAMODE_GOT_IP:
            // We have an IP address, ready to run. Return the IP address, too.
            os_printf("Received EVENT_STAMODE_GOT_IP. IP = "IPSTR", mask = "IPSTR", gateway = "IPSTR"\n", 
                      IP2STR(&event->event_info.got_ip.ip.addr), 
                      IP2STR(&event->event_info.got_ip.mask.addr),
                      IP2STR(&event->event_info.got_ip.gw));
            set_blink_timer(1000);
            break;
        case EVENT_STAMODE_DHCP_TIMEOUT:
            // We couldn't get an IP address via DHCP, so we'll have to try re-connecting.
            os_printf("Received EVENT_STAMODE_DHCP_TIMEOUT.\n");
            wifi_station_disconnect();
            wifi_station_connect();
            set_blink_timer(4000);
            break;
    }
}

/*
 * Handles the receiving of information, either from TCP or UDP (as we've registered this call-back for both).
 */
LOCAL void ICACHE_FLASH_ATTR recv_cb(void *arg, char *data, uint16_t len) {
    // Store the IP address from the sender of this data.
    struct espconn *conn = (struct espconn *)arg;
    uint8_t *addr_array = NULL;
    if (conn->type == ESPCONN_TCP) {
        addr_array = conn->proto.tcp->remote_ip;
    } else {
        addr_array = conn->proto.udp->remote_ip;
    }
    if (addr_array != NULL) {
        ip_addr_t addr;
        IP4_ADDR(&addr, addr_array[0], addr_array[1], addr_array[2], addr_array[3]);
        last_addr = addr.addr;
        os_printf("Received data from "IPSTR"\n", IP2STR(&last_addr));
    }

    // Parse the data received.
    // NOTE: This is NOT a safe method to choose, as there is no checking for overflow of the delay variable.
    //       In addition, it does not handle TCP data being split up into multiple packets!
    uint16_t delay = 0;
    for (uint16_t ii = 0; ii < len; ii++) {
        if ((data[ii] >= '0') && (data[ii] <= '9')) {
            // This is a digit, update the delay.
            delay = (delay * 10) + (data[ii] - '0');
        } else {
            // Stop at the first non-digit.
            break;
        }
    }

    if (delay > 0) {
        // We have a valid delay, use it.
        set_blink_timer(delay);
    } else {
        // No valid delay received.
        set_blink_timer(4000);
    }
}

/*
 * Call-back for when an incoming TCP connection has been established. We use this to 
 */
LOCAL void ICACHE_FLASH_ATTR tcp_connect_cb(void *arg) {
    struct espconn *conn = (struct espconn *)arg;
    os_printf("TCP connection received from "IPSTR":%d to local port %d\n",
              IP2STR(conn->proto.tcp->remote_ip), conn->proto.tcp->remote_port, conn->proto.tcp->local_port);
    espconn_regist_recvcb(conn, recv_cb);
    set_blink_timer(500);
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

    // Set up the TCP server.
    tcp_proto.local_port = 2345;
    tcp_conn.type = ESPCONN_TCP;
    tcp_conn.state = ESPCONN_NONE;
    tcp_conn.proto.tcp = &tcp_proto;
    espconn_regist_connectcb(&tcp_conn, tcp_connect_cb);
    espconn_accept(&tcp_conn);

    // Set up the UDP server.
    udp_proto.local_port = 2345;
    udp_conn.type = ESPCONN_UDP;
    udp_conn.state = ESPCONN_NONE;
    udp_conn.proto.udp = &udp_proto;
    espconn_create(&udp_conn);
    espconn_regist_recvcb(&udp_conn, recv_cb);
}

/*
 * Receives the characters from the serial port. We're ignoring characters here.
 */
void ICACHE_FLASH_ATTR uart_rx_task(os_event_t *events) {
    if (events->sig == 0) {
        // Sig 0 is a normal receive. Get how many bytes have been received.
        uint8_t rx_len = (READ_PERI_REG(UART_STATUS(UART0)) >> UART_RXFIFO_CNT_S) & UART_RXFIFO_CNT;

        // Read the characters from the FIFO, and discard them.
        for (uint8_t ii=0; ii < rx_len; ii++) {
            READ_PERI_REG(UART_FIFO(UART0)) & 0xFF;
        }

        // Clear the interrupt condition flags and re-enable the receive interrupt.
        WRITE_PERI_REG(UART_INT_CLR(UART0), UART_RXFIFO_FULL_INT_CLR | UART_RXFIFO_TOUT_INT_CLR);
        uart_rx_intr_enable(UART0);
    }
}

/*
 * Entry point for the program. Sets up the microcontroller for use.
 */
void user_init(void) {
    // Initialise the serial port.
    uart_init(BIT_RATE_115200, BIT_RATE_115200);

    // Initialise all GPIOs.
    gpio_init();

    // GPIO 4 is an output, start with it low (off).
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO4_U, FUNC_GPIO4);
    gpio_output_set(0, BIT4, BIT4, 0);

    // Start the LED timer at 4s per change.
    set_blink_timer(4000);

    // Start the network.
    wifi_init();

    // Initialise the OTA flash system.
    ota_init();

    // Initialise the network debugging.
    dbg_init();
}
