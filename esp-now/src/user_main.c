/*
 * user_main.c: Main entry-point for the ESP-NOW demonstration code.
 *
 * Author: Ian Marshall
 * Date: 19/05/2018
 */
#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "eagle_soc.h"
#include "espnow.h"
#include "user_interface.h"
#include "espmissingincludes.h"

// The number of entries that may be on the reply task queue.
#define REPLY_QUEUE_LEN 2

// The modes that a node can be using.
typedef enum mode_t {SENDER, RECEIVER} mode_t;

// The number of milliseconds between transmissions from the sender.
static const uint32_t SEND_INTERVAL = 1000;

// The number of milliseconds between message receptions before a timeout.
static const uint32_t RECEIVER_TIMEOUT_INTERVAL = 1100;

// The number of milliseconds for a message reply before a timeout.
static const uint32_t RESPONSE_TIMEOUT_INTERVAL = 200;

// The priority of the reply task queue.
static const uint8_t REPLY_PRI = 1;

// The SoftAP MAC address of the node to which the sender will send messages.
uint8_t dest_mac[] = {0x5e, 0xcf, 0x7f, 0x29, 0xb5, 0x94};

// The mode of this node.
LOCAL mode_t mode;

// Timer used for triggering message transmissions.
LOCAL os_timer_t tx_timer;

// Timer used for triggering receive timeouts.
LOCAL os_timer_t rx_timer;

// The "time" that the last message was transmitted.
LOCAL uint32_t send_time;

// The counter of the last message to be transmitted.
LOCAL uint32_t tx_message_count = 0;

// The last MAC address that we received a message from.
LOCAL uint8_t last_mac[6] = {0, 0, 0, 0, 0, 0};

// The last counter that we received in a message.
LOCAL uint32_t last_counter = 0;

// The task queue used for message replies.
LOCAL os_event_t reply_queue[REPLY_QUEUE_LEN];

/*
 * Timer callback for when it's time to send the next message.
 */
LOCAL void ICACHE_FLASH_ATTR send_message(void *arg) {
	// Prepare the message contents.
	tx_message_count++;
	uint8_t message[6];
	message[0] = 0xAA;
	message[1] = 0xBB;
	message[2] = ((tx_message_count & 0x000000FF));
	message[3] = ((tx_message_count & 0x0000FF00) >> 8)  & 0xFF;
	message[4] = ((tx_message_count & 0x00FF0000) >> 16) & 0xFF;
	message[5] = ((tx_message_count & 0xFF000000) >> 24) & 0xFF;

	// Send the message contents.
	send_time = system_get_time();
	esp_now_send(dest_mac, message, 6);
	os_printf("Tx message for ["MACSTR"] of length 6.\n", MAC2STR(dest_mac));

	// Start the receive timer.
	os_timer_arm(&rx_timer, RESPONSE_TIMEOUT_INTERVAL, 0);
}

/*
 * Callback for when a message has been received via ESP-NOW.
 */
LOCAL void ICACHE_FLASH_ATTR message_rx_cb(
		uint8_t *mac, uint8_t *data, uint8_t len) {
	// Disable the receive timer.
	os_timer_disarm(&rx_timer);
	
	os_printf("Rx message from ["MACSTR"] of length %d.\n", MAC2STR(mac), len);

	// Check the message contents.
	bool message_ok = false;
	if (len != 6) {
		os_printf("Rx message from ["MACSTR"] is of length %d, 6 expected.\n",
				MAC2STR(mac), len);
	} else if ((data[0] != 0xAA) || (data[1] != 0xBB)) {
		os_printf("Rx message from ["MACSTR"] has a bad header %02x, %02x.\n",
				MAC2STR(mac), data[0], data[1]);
	} else {
		// Extract the counter and compare the value to what we expect.
		uint32_t counter = (data[2] +
		                   (data[3] << 8) +
		                   (data[4] << 16) +
		                   (data[5] << 24));
		uint32_t expected;
		if (mode == SENDER) {
			// Senders expect the counter to be reflected back to it.
			expected = tx_message_count;
		} else {
			// Receivers expect the counter to be incremented by 1 each time.
			expected = last_counter + 1;
		}
		if (counter != expected) {
			os_printf("Rx message from ["MACSTR"] counter mismatch "
					"(%d, expecting %d).\n", MAC2STR(mac), counter, expected);
			if (mode == RECEIVER) {
				last_counter = counter;
			}
		} else {
			// The message is as we expect.
			message_ok = true;
			if (mode == RECEIVER) {
				// Store the counter and MAC for replying in a separate task.
				os_memcpy(last_mac, mac, 6);
				last_counter = counter;
				
				// Post a message to transmit the reply.
				system_os_post(REPLY_PRI, 0, 0);
			} else {
				// Check the timing of the round trip.
				uint32_t now = system_get_time();
				uint32_t diff = now - send_time;
				os_printf("Message %5d RTT - %d us.\n", tx_message_count, diff);
			}

			// Set the LEDs GPIO 12 = good, GPIO 4 = bad.
			gpio_output_set(BIT12, BIT4, BIT4 | BIT12, 0);
		}
	}

	if (!message_ok) {
		// Set the LEDs GPIO 12 = good, GPIO 4 = bad. Set both, as we received 
		// something, but it's not what we're expecting.
		gpio_output_set(BIT4 | BIT12, 0, BIT4 | BIT12, 0);
	}
}

/*
 * Called to reply to a message out of the main receive code, which is time 
 * critical.
 */
LOCAL void ICACHE_FLASH_ATTR reply_to_message(os_event_t *event) {
	// Relay the message back to the sender.
	uint8_t message[6];
	message[0] = 0xAA;
	message[1] = 0xBB;
	message[2] = ((last_counter & 0x000000FF));
	message[3] = ((last_counter & 0x0000FF00) >> 8)  & 0xFF;
	message[4] = ((last_counter & 0x00FF0000) >> 16) & 0xFF;
	message[5] = ((last_counter & 0xFF000000) >> 24) & 0xFF;
	esp_now_send(last_mac, message, 6);
	os_printf("Tx message for ["MACSTR"] of length 6.\n", MAC2STR(dest_mac));

	// Start the receive timer for the next message.
	os_timer_arm(&rx_timer, RECEIVER_TIMEOUT_INTERVAL, 0);
}

/*
 * Called when we haven't received a message we were expecting.
 */
LOCAL void ICACHE_FLASH_ATTR message_timeout() {
	// Set the LEDs GPIO 0 = good, GPIO 4 = bad.
	gpio_output_set(BIT4, BIT12, BIT4 | BIT12, 0);
	os_printf("Timeout received.\n");
}

/*
 * Performs the setup routines for ESP-NOW after the ESP8266 is ready for it.
 */
LOCAL void ICACHE_FLASH_ATTR system_ready_cb() {
	os_printf("In system callback function.\n");

	// Decide if we're an input or an output.
	bool gpio5 = GPIO_INPUT_GET(5);
    if (gpio5) {
		mode = SENDER;
	} else {
		mode = RECEIVER;
	}

	// Print some information over serial.
	uint8_t softap_mac[6];
	uint8_t station_mac[6];
	wifi_get_macaddr(SOFTAP_IF, softap_mac);
	wifi_get_macaddr(STATION_IF, station_mac);
	os_printf("In %s mode.\n", (mode == SENDER) ? "sending" : "receiving");
	os_printf("SoftAP MAC address : "MACSTR"\n", MAC2STR(softap_mac));
	os_printf("Station MAC address: "MACSTR"\n", MAC2STR(station_mac));

	if (esp_now_init()) {
		// We couldn't set up ESP-NOW.
		os_printf("Unable to start ESP-NOW.\n");
	} else {
		// Create a timer for checking if we have missed any packets.
		os_printf("ESP-NOW mode enabled.\n");
		os_timer_disarm(&rx_timer);
		os_timer_setfn(&rx_timer,
				(os_timer_func_t *)message_timeout, (void *)0);
		if (mode == SENDER) {
			// Make the sender a controller.
			esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);

			// Start a timer for sending packets every second.
			os_timer_disarm(&tx_timer);
			os_timer_setfn(&tx_timer,
					(os_timer_func_t *)send_message, (void *)0);
			os_timer_arm(&tx_timer, SEND_INTERVAL, 1);
		} else {
			// Make the receiver a slave.
			esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);

			// Set up the system task for replying to messages.
			system_os_task(reply_to_message, REPLY_PRI, reply_queue, REPLY_QUEUE_LEN);

			// Start the receive timer.
			os_timer_arm(&rx_timer, RECEIVER_TIMEOUT_INTERVAL, 0);
		}

		// Set up the callback for receiving messages.
		esp_now_register_recv_cb(message_rx_cb);
	}

	os_printf("Completed system callback function.\n");
}

/*
 * Entry point for the program. Sets up the microcontroller for use.
 */
void user_init(void) {
	// Initialise the serial port.
	uart_div_modify(0, UART_CLK_FREQ / 76800);

	// Initialise the GPIO.
	os_printf("Initialising the GPIO.\n");
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO4_U, FUNC_GPIO4);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO5_U, FUNC_GPIO5);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, FUNC_GPIO12);
	gpio_output_set(0, BIT4 | BIT12, BIT4 | BIT12, BIT12);
	PIN_PULLUP_EN(PERIPHS_IO_MUX_MTDI_U);

	// Call the initialisation function when the system is up and running.
	os_printf("Setting the system call-back function.\n");
	system_init_done_cb(system_ready_cb);
}
