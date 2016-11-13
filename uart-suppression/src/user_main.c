/*
 * user_main.c: Main entry-point for the Delta Inverter gateway.
 * This is used to read the current values from the inverter, and send them to a web server for processing.
 *
 * Author: Ian Marshall
 * Date: 1/10/2016
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
#define SSID "CRESTWOOD_PRIVATE"
#define PASSWD "WilsMarshSkye2003"

// Timer used to send out messages via serial.
LOCAL os_timer_t message_timer;

// The number of messages that have been transmitted.
LOCAL uint32_t message_count = 0;

/*
 * Call-back for when the message timer expires.
 */
LOCAL void ICACHE_FLASH_ATTR message_cb(void *arg) {
    // Write a message to UART 0.
    uint8_t message[] = {'T', 'i', 'm', 'e', 'r', ' ', 'e', 'x', 'p', 'i', 'r', 'e', 'd', '.', '\n'};
    uart0_tx_buffer(message, 15);

    // Write a message to UART 1.
    uart_tx_one_char(UART1, 'U');
    uart_tx_one_char(UART1, 'A');
    uart_tx_one_char(UART1, 'R');
    uart_tx_one_char(UART1, 'T');
    uart_tx_one_char(UART1, '-');
    uart_tx_one_char(UART1, '1');
    uart_tx_one_char(UART1, ' ');
    uart_tx_one_char(UART1, 'E');
    uart_tx_one_char(UART1, 'x');
    uart_tx_one_char(UART1, 'p');
    uart_tx_one_char(UART1, 'i');
    uart_tx_one_char(UART1, 'r');
    uart_tx_one_char(UART1, 'y');
    uart_tx_one_char(UART1, '.');
    uart_tx_one_char(UART1, '\n');
    
    os_printf("Timer expiry - %d.\n", ++message_count);
}

/*
 * Receives the characters from the serial port.
 */
void ICACHE_FLASH_ATTR uart_rx_task(os_event_t *events) {
    if (events->sig == 0) {
        // Sig 0 is a normal receive. Get how many bytes have been received.
        uint8_t rx_len = (READ_PERI_REG(UART_STATUS(UART0)) >> UART_RXFIFO_CNT_S) & UART_RXFIFO_CNT;

        char rx_char;
        for (uint8_t ii=0; ii < rx_len; ii++) {
            rx_char = READ_PERI_REG(UART_FIFO(UART0)) & 0xFF;
            os_printf("rx: %x\n", rx_char);
        }

        // Clear the interrupt condition flags and re-enable the receive interrupt.
        WRITE_PERI_REG(UART_INT_CLR(UART0), UART_RXFIFO_FULL_INT_CLR | UART_RXFIFO_TOUT_INT_CLR);
        uart_rx_intr_enable(UART0);
    }
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
    // Initialise the serial port.
    uart_init(BIT_RATE_19200, BIT_RATE_19200);

    // Swap the UART over, to suppress it's output.
    system_uart_swap();

    // Start the network.
    wifi_init();

    // Initialise the OTA flash system.
    ota_init();

    // Initialise the network debugging.
    dbg_init();

    // Start a timer for sending messages, every second.
    os_timer_disarm(&message_timer);
    os_timer_setfn(&message_timer, (os_timer_func_t *)message_cb, (void *)0);
    os_timer_arm(&message_timer, 1000, 1);
}
