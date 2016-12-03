/*
 * user_main.c: Main entry-point for the Delta Inverter gateway.
 * This is used to read the current values from the inverter, and send them to a web server for processing.
 *
 * Author: Ian Marshall
 * Date: 1/10/2016
 */
#include "ets_sys.h"
#include "osapi.h"
#include "mem.h"
#include "gpio.h"
#include "os_type.h"
#include "ip_addr.h"
#include "espconn.h"
#include "user_interface.h"
#include "espmissingincludes.h"

#include "driver/uart.h"

#include "tcp_ota.h"
#include "udp_debug.h"
#include "string_builder.h"

// Change the below values to suit your own network.
#define SSID "-----------------"
#define PASSWD "-----------------"

static const uint8_t COMMANDS[][2] = {
    {0x10, 0x01}, // Instantaneous current - Input 1
    {0x10, 0x02}, // Instantaneous voltage - Input 1
    {0x10, 0x03}, // Instantaneous power - Input 1
    {0x11, 0x01}, // Average current - Input 1
    {0x11, 0x02}, // Average voltage - Input 1
    {0x11, 0x03}, // Average power - Input 1
    {0x20, 0x05}, // Internal temperature - AC assembly
    {0x21, 0x08}, // Internal temperature - DC assembly
    {0x10, 0x07}, // Instantaneous current - AC output
    {0x10, 0x08}, // Instantaneous voltage - AC output
    {0x10, 0x09}, // Instantaneous power - AC output
    {0x10, 0x0A}, // Instantaneous frequency - AC output
    {0x11, 0x07}, // Average current - AC output
    {0x11, 0x08}, // Average voltage - AC output
    {0x11, 0x09}, // Average power - AC output
    {0x11, 0x0A}, // Average frequency - AC output
    {0x13, 0x03}, // Day energy
    {0x13, 0x04}, // Day running time
    {0x14, 0x03}, // Week energy
    {0x14, 0x04}, // Week running time
    {0x15, 0x03}, // Month energy
    {0x15, 0x04}, // Month running time
    {0x16, 0x03}, // Year energy
    {0x16, 0x04}, // Year running time
    {0x17, 0x03}, // Total energy
    {0x17, 0x04}, // Total running time
    {0x12, 0x01}, // Solar current limit - Input 1
    {0x12, 0x02}, // Solar voltage limit - Input 1
    {0x12, 0x03}, // Solar power limit - Input 1
    {0x12, 0x07}, // AC current max
    {0x12, 0x08}, // AC voltage min
    {0x12, 0x09}, // AC voltage max
    {0x12, 0x0A}, // AC power
    {0x12, 0x0B}, // AC frequency min
    {0x12, 0x0C}, // AC frequency max
    {0x03, 0x05}, // Starting voltage
    {0x03, 0x06}, // Under voltage 1
    {0x03, 0x07}, // Under voltage 2
    {0x08, 0x02}, // Min MPP
    {0x08, 0x02}, // Max MPP
    {0x08, 0x02}, // Increment
    {0x08, 0x02}, // Exponential factor
    {0x08, 0x02}, // Min MPP power
    {0x08, 0x02}, // MPP sampling
    {0x08, 0x02}, // MPP scan rate
    {0x08, 0x02}, // Number of MPP trackers
    {0x08, 0x02}  // Startup emmissions
};

// The number of non-overhead bytes in the reply for each command.
static const uint8_t COMMAND_LENGTHS[] = {
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 4, 4, 4, 4, 
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 2, 2, 2, 1, 2
};

static const char *COMMAND_TAGS[] = {
    "instant-current-i1",
    "instant-voltage-i1",
    "instant-power-i1",
    "average-current-i1",
    "average-voltage-i1",
    "average-power-i1",
    "internal-temp-ac",
    "internal-temp-dc",
    "instant-current-ac",
    "instant-voltage-ac",
    "instant-power-ac",
    "instant-frequency-ac",
    "average-current-ac",
    "average-voltage-ac",
    "average-power-ac",
    "average-frequency-ac",
    "day-energy",
    "day-run-time",
    "week-energy",
    "week-run-time",
    "month-energy",
    "month-run-time",
    "year-energy",
    "year-run-time",
    "total-energy",
    "total-run-time",
    "solar-current-limit",
    "solar-voltage-limit",
    "solar-power-limit",
    "current-max-ac",
    "voltage-min-ac",
    "voltage-max-ac",
    "power-ac",
    "frequency-min-ac",
    "frequency-max-ac",
    "starting-voltage",
    "under-voltage-1",
    "under-voltage-2",
    "mpp-min",
    "mpp-max",
    "increment",
    "exp-factor",
    "mpp-power-min",
    "mpp-sampling",
    "mpp-scan-rate",
    "mpp-tracker-count",
    "startup-emmissions"
};

// The number of commands that are to be sent to the Delta inverter for data retrieval.
//#define COMMAND_COUNT 47
#define COMMAND_COUNT 38

// Stores the address to which the results from the inverter are sent via HTTP in an ip_addr structure.
//#define REMOTE_ADDR(ip) (ip)[0] = 10; (ip)[1] = 0; (ip)[2] = 1; (ip)[3] = 253;
#define REMOTE_ADDR(ip) (ip)[0] = 10; (ip)[1] = 0; (ip)[2] = 1; (ip)[3] = 48;

// The number of bytes in each packet that is not either a command or data.
static const uint8_t PACKET_OVERHEAD = 7;

// The number of bytes in each command.
static const uint8_t COMMAND_LEN = 2;

// The number of bytes to use for the incoming serial buffer for receiving data from the Delta inverter.
//static const uint8_t RX_BUFFER_LENGTH = 16;
#define RX_BUFFER_LENGTH 16

// Start of text character in the packets.
static const uint8_t STX = 0x02;

// The address used when sending packets to the inverter.
static const uint8_t INVERTER_ADDR = 0x05;

// The address expected when receiving packets from the inverter.
static const uint8_t GATEWAY_ADDR = 0x06;

// The chain ID of the inverter.
static const uint8_t INVERTER_ID = 0x01;

// The end of text character in the packets.
static const uint8_t ETX = 0x03;

// The priority of the disconnect task.
static const uint8_t DISCONNECT_PRI = 1;

// The current command index that we are processing.
static uint8_t current_command_index = 0;

// The number of bytes of data that are expected in the reply message.
uint8_t data_len = 0;

// The number of bytes expected in the reply message, including data and overhead.
uint8_t expected_len = 0;

// The current values received from the inverter.
static uint32_t inverter_values[COMMAND_COUNT];

// Incoming serial buffer for receiving data from the Delta inverter.
static uint8_t rx_buffer[RX_BUFFER_LENGTH];

// The number of bytes in the current RX serial buffer.
static uint8_t rx_buffer_len = 0;

// Flag as to whether a time out has occurred.
static bool timeout = true;

// Flag as to whether we're waiting for the remote site's response.
static bool awaiting_response = true;

// The connection used for sending HTTP messages for inverter data.
static struct espconn conn;

// The protocol information for the HTTP connection.
static esp_tcp proto;

// The timer used for knowing when to start the transmissions to the Delta inverter.
static os_timer_t transmit_timer;

// The timer used for timing out a serial transmission/reception for the inverter, in case there is no reply.
static os_timer_t serial_rx_timer;

// Buffer that is used to hold the contents of an HTTP message to be sent.
static string_builder *value_buf = NULL;

/*
 * Calculates the CRC-16 value that is used by the inverter. Note that the first
 * character of the packet is NOT included in the calculations. This is done to match
 * the calculations made by the inverter.
 *
 * @param packet The packet from which the CRC value is calculated.
 * @param end The number of bytes in the packet to include in the calculations. This
 *     number includes the first byte of the packet, which is not actually used in
 *     the CRC calculation.
 * @return The CRC-16 value computed for the packet.
 */
uint16_t calculate_crc16(uint8_t *packet, uint8_t end) {
    const uint16_t POLY = 0x0A001;
    uint16_t crc = 0;

    // We start from 1 to exclude the STX byte.
    for (int ii = 1; ii < end; ii++) {
        crc ^= packet[ii];
        for (int jj = 0; jj < 8; jj++)
        {
            if (crc & 0x01) {
                crc = (crc >> 1) ^ POLY;
            } else {
                crc = (crc >> 1);
            }
        }
    }
    
    return crc;
}

/*
 * Call-back for when we get a response from the web server.
 */
LOCAL void ICACHE_FLASH_ATTR response_cb(void *arg, char *data, uint16_t len) {
    struct espconn *conn = (struct espconn *)arg;

    // Trace out the received data:
    /*
    os_printf("Received %d bytes.\n", len);
    for (int ii = 0; ii < len; ii++) {
        os_printf("%c(%02x) ", data[ii], data[ii]);
    }
    os_printf("\n");
    */

    // Make sure we're wanting to process this, in case the response comes in two or more packets.
    if (!awaiting_response) {
        return;
    } else {
        awaiting_response = false;
    }

    // Make sure the reply starts with "HTTP/1.? "
    if ((data[0] != 'H') ||(data[1] != 'T') ||(data[2] != 'T') || (data[3] != 'P') || (data[4] != '/') ||
            (data[5] != '1') || (data[6] != '.') || (data[8] != ' ')) {
        os_printf("Unexpected HTTP header received.\n");
        for (int ii = 0; ii < 8; ii++) {
            os_printf("%02x ", data[ii]);
        }
        os_printf("\n");
    } else {
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
            os_printf("Error returned from remote server: \"%s\".\n", data);
        }
    }

    // Close the connection ASAP, now we're done with it.
    system_os_post(DISCONNECT_PRI, 0, 0);
}

/*
 * Disconnects a TCP connection, if still connected.
 */
LOCAL void ICACHE_FLASH_ATTR disconnect_task(os_event_t *event) {
    int8_t res = espconn_disconnect(&conn);
    if (value_buf != NULL) {
        free_string_builder(value_buf);
        value_buf = NULL;
    }
}

/*
 * Call-back for when we have a connection to the web server, to which we send our HTTP request.
 */
LOCAL void ICACHE_FLASH_ATTR connect_cb(void *arg) {
    struct espconn *conn = (struct espconn *)arg;
    os_printf("Connected to server.\n");

    // Register a call-back for when we receive data.
    espconn_regist_recvcb(conn, response_cb);
    awaiting_response = true;

    // Send through the HTTP request.
    if (value_buf != NULL) {
        int8_t res = espconn_send(conn, value_buf->buf, value_buf->len);
        os_printf("Sent %d bytes with result %d.\n", value_buf->len, res);
    } else {
        os_printf("Transmission cancelled, buffer is NULL.\n");
    }
}

/*
 * Call-back for when HTTP connection has been disconnected.
 */
LOCAL void ICACHE_FLASH_ATTR disconnect_cb(void *arg) {
    if (value_buf != NULL) {
        free_string_builder(value_buf);
        value_buf = NULL;
    }
    os_printf("Disconnected from server.\n");
}

/*
 * Call-back for when a HTTP connection has failed - reconnected is a misleading name, sadly.
 */
LOCAL void ICACHE_FLASH_ATTR reconnect_cb(void *arg, int8_t err) {
    if (value_buf != NULL) {
        free_string_builder(value_buf);
        value_buf = NULL;
    }
    os_printf("Connection failed to server - %d.\n", err);
}

/*
 * Sends the current values received from the inverter via HTTP for use (e.g. logging).
 */
LOCAL void ICACHE_FLASH_ATTR send_inverter_values() {
    // Set up the structures for the connection.
    REMOTE_ADDR(proto.remote_ip);
    proto.remote_port = 8074;

    conn.type = ESPCONN_TCP;
    conn.state = ESPCONN_NONE;
    conn.proto.tcp = &proto;

    // Register the required call-back functions.
    espconn_regist_connectcb(&conn, connect_cb);
    espconn_regist_disconcb(&conn, disconnect_cb);
    espconn_regist_reconcb(&conn, reconnect_cb);

    // Connect to the server.
    os_printf("Connecting to server.\n");
    int8_t res = espconn_connect(&conn);
    switch (res) {
        case 0:
            // This is normal, ignore it.
            break;
        case ESPCONN_MEM:
            os_printf("Unable to connect to server - out of memory.\n");
            break;
        case ESPCONN_TIMEOUT:
            os_printf("Unable to connect to server - timeout.\n");
            break;
        case ESPCONN_ISCONN:
            os_printf("Unable to connect to server - already connected.\n");
            break;
        case ESPCONN_ARG:
            os_printf("Unable to connect to server - illegal argument.\n");
            break;
        default:
            os_printf("Unable to connect to server - unknown error - %d.\n", res);
            break;
    }
}

/*
 * Sends an HTTP POST message to the tagwriter service with the supplied contents.
 */
LOCAL void ICACHE_FLASH_ATTR tagwriter_post(string_builder *content) {
    if (value_buf != NULL) {
        // The old value buffer didn't get freed for some reason, free it now.
        free_string_builder(value_buf);
        value_buf = NULL;
    }

    // Create a buffer for holding the full HTTP request, header + contents.
    string_builder *sb = create_string_builder(content->len + 100);
    if (sb == NULL) {
        free_string_builder(content);
        os_printf("Unable to create string builder to send packet.");
    } else {
        // Create the HTTP header.
        bool add_ok = true;
        add_ok &= append_string_builder(sb, "POST /tagwriter HTTP/1.1\r\n" \
                "Content-Type: application/json\r\n" \
                "Connection: close\r\n" \
                "Content-Length: ");
        add_ok &= append_int32_string_builder(sb, content->len);
        add_ok &= append_string_builder(sb, "\r\n\r\n");

        // Add the contents.
        add_ok &= append_string_builder_to_string_builder(sb, content);
        free_string_builder(content);

        if (add_ok) {
            // Store the buffer in the value buffer pointer.
            value_buf = sb;

            // Send the HTTP request.
            send_inverter_values();
        } else {
            // Something went wrong with the creation of the request.
            value_buf = NULL;
            free_string_builder(sb);
            os_printf("Unable to prepare HTTP message contents for transmission.\n");
        }
    }
}

// Debugs out the contents of a packet.
void ICACHE_FLASH_ATTR debug_print_packet(uint8_t *packet, uint8_t length) {
    for (int ii = 0; ii < length; ii++) {
        os_printf("%02x ", packet[ii]);
    }
    os_printf("\n");
}

/*
 * Transmits an array of data to the UART. This is provided to make it easy to switch between UARTs 0/1.
 */
void ICACHE_FLASH_ATTR uart_tx_array(uint8_t *array, uint8_t len) {
    // UART 0.
    //uart0_tx_buffer(array, len);

    // UART 1.
    os_printf("tx (%d): ", len);
    for (uint8_t ii = 0; ii < len; ii++) {
        uart_tx_one_char(UART1, array[ii]);
        os_printf("%02x ", array[ii]);
    }
    os_printf("\n");
}

/*
 * Sends a request to the inverter for a single data point.
 */
void ICACHE_FLASH_ATTR send_data_request() {
    // Prepare the packet for transmission to the inverter.
    uint8_t tx_packet[9];
    os_printf("Preparing packet for command #%d\n", current_command_index);
    tx_packet[0] = STX;
    tx_packet[1] = INVERTER_ADDR;
    tx_packet[2] = INVERTER_ID;
    tx_packet[3] = COMMAND_LEN;
    tx_packet[4] = COMMANDS[current_command_index][0];
    tx_packet[5] = COMMANDS[current_command_index][1];
    uint16_t crc = calculate_crc16(tx_packet, 6);
    tx_packet[6] = (crc & 0x00FF);
    tx_packet[7] = (crc & 0xFF00) >> 8;
    tx_packet[8] = ETX;

    // Determine how much data to expect as a reply.
    data_len = COMMAND_LENGTHS[current_command_index];
    expected_len = data_len + PACKET_OVERHEAD + COMMAND_LEN;
    os_printf("Expected len = %d.\n", expected_len);

    // Start the timeout timer.
    os_timer_disarm(&serial_rx_timer);
    os_timer_arm(&serial_rx_timer, 10000, 0);

    // Send the request to the inverter, setting GPIO 4 to high for the transmission (for the RS485 converter).
    gpio_output_set(BIT4, 0, BIT4, 0);
    os_delay_us(100);
    uart_tx_array(tx_packet, 9);
    os_delay_us(1000);
    gpio_output_set(0, BIT4, BIT4, 0);
}

/*
 * Call-back used to begin the transmission of requests for values from the Delta inverter.
 */
void ICACHE_FLASH_ATTR transmit_cb() {
    current_command_index = 0;
    send_data_request();
}

/*
 * Call-back used when the serial timer has expired. This means that the inverter didn't respond to our request in time.
 */
void ICACHE_FLASH_ATTR serial_timeout_cb() {
    // Set the timeout flag, and reset the current command index to indicate that we shouldn't process any data.
    os_printf("Timeout received while waiting for response for command %d.\n", current_command_index);
    timeout = true;
    current_command_index = -1;

    // Send a message to mark the group as unhealthy.
    string_builder *content = create_string_builder(30);
    if (content == NULL) {
        os_printf("Unable to create string builder to send timeout message.");
    } else {
        // Create the content portion of the HTTP request.
        bool add_ok = true;
        add_ok &= append_string_builder(content, "{\"groups\":{\"2\":\"unhealthy\"}}");
        
        // Send the contents to the server via an HTTP POST for processing.
        if (add_ok) {
            tagwriter_post(content);
        }
    }
}

/*
 * Receives a response from a command request, either preparing for the next send, or completing the HTTP
 * call to the server, as necessary.
 */
void ICACHE_FLASH_ATTR process_response() {
    // Cancel the receive timer.
    os_timer_disarm(&serial_rx_timer);

    // Validate the packet.
    if ((rx_buffer[0] != STX) ||
        (rx_buffer[1] != GATEWAY_ADDR) ||
        (rx_buffer[2] != INVERTER_ID) ||
        (rx_buffer[3] != (COMMAND_LENGTHS[current_command_index] + COMMAND_LEN)) ||
        (rx_buffer[4] != COMMANDS[current_command_index][0]) ||
        (rx_buffer[5] != COMMANDS[current_command_index][1]) ||
        (rx_buffer[COMMAND_LENGTHS[current_command_index] + 8] != ETX)) {
        // The packet's contents are not valid.
        os_printf("Packet mismatch. Received: ");
        debug_print_packet(rx_buffer, rx_buffer_len);

        uint8_t *expected = (uint8_t *)os_malloc(current_command_index + 8);
        if (expected) {
            expected[0] = STX;
            expected[1] = GATEWAY_ADDR;
            expected[2] = INVERTER_ID;
            expected[3] = COMMAND_LENGTHS[current_command_index] + COMMAND_LEN;
            expected[4] = COMMANDS[current_command_index][0];
            expected[5] = COMMANDS[current_command_index][1];
            expected[COMMAND_LENGTHS[current_command_index] + 8] = ETX;
            os_printf("Expected: ");
            debug_print_packet(expected, rx_buffer_len);
            os_free(expected);
        }
    }
    
    // Check the checksum.
    uint16_t msg_crc =  (rx_buffer[data_len + 6] & 0xFF) | 
                       ((rx_buffer[data_len + 7] << 8) & 0xFF00);
    uint16_t crc = calculate_crc16(rx_buffer, data_len + 6);
    if (msg_crc != crc) {
        // The CRC's don't match.
        os_printf("Packet CRC mismatch, received %x, expected %x.\n", msg_crc, crc);
        debug_print_packet(rx_buffer, expected_len);
        return;
    }
        
    // If we get here, then we're good, store the received value.
    os_printf("Response %d accepted.\n", current_command_index);
    if (COMMAND_LENGTHS[current_command_index] == 1) {
        inverter_values[current_command_index] = rx_buffer[6];
    } else if (COMMAND_LENGTHS[current_command_index] == 2) {
        inverter_values[current_command_index] = (rx_buffer[6] << 8) + rx_buffer[7];
    } else if (COMMAND_LENGTHS[current_command_index] == 4) {
        inverter_values[current_command_index] = (rx_buffer[6] << 24) +
                                                 (rx_buffer[7] << 16) + 
                                                 (rx_buffer[8] <<  8) +
                                                  rx_buffer[9];
    }

    if (current_command_index == COMMAND_COUNT - 1) {
        // We've finished retrieving all of the values, send them to the server.
        os_printf("Preparing transmission of tag values.\n");
        string_builder *content = create_string_builder(128);
        if (content == NULL) {
            os_printf("Unable to create string builder to send result contents.");
        } else {
            // Create the content portion of the HTTP request.
            bool add_ok = true;
            add_ok &= append_string_builder(content, "{\"tags\":{");
            for (uint8_t ii = 0; ii < COMMAND_COUNT; ii++) {
                if (ii > 0) {
                    add_ok &= append_string_builder(content, ",\"");
                } else {
                    add_ok &= append_string_builder(content, "\"");
                }
                add_ok &= append_string_builder(content, COMMAND_TAGS[ii]);
                add_ok &= append_string_builder(content, "\":");
                add_ok &= append_int32_string_builder(content, inverter_values[ii]);
            }
            if (timeout) {
                timeout = false;
                add_ok &= append_string_builder(content, "},\"groups\":{\"2\":\"healthy\"}}");
            } else {
                add_ok &= append_string_builder(content, "}}");
            }

            // Send the contents to the server via an HTTP POST for processing.
            if (add_ok) {
                os_printf("Prepared tag data contents of length %d.\n", content->len);
                tagwriter_post(content);
            } else {
                os_printf("Unable to prepare tag data contents, current length %d.\n", content->len);
            }
        }
    } else {
        // There's still more to go. Advance to the next command index.
        current_command_index++;
        send_data_request();
    }
}

/*
 * Receives the characters from the serial port.
 */
void ICACHE_FLASH_ATTR uart_rx_task(os_event_t *events) {
    if (events->sig == 0) {
        // Sig 0 is a normal receive. Get how many bytes have been received.
        uint8_t rx_len = (READ_PERI_REG(UART_STATUS(UART0)) >> UART_RXFIFO_CNT_S) & UART_RXFIFO_CNT;

        // Ensure we're still OK to process the data.
        if (current_command_index != -1) {
            // Add the received bytes to the buffer.
            char rx_char;
            os_printf("rx (%d): ", rx_len);
            for (uint8_t ii=0; ii < rx_len; ii++) {
                rx_char = READ_PERI_REG(UART_FIFO(UART0)) & 0xFF;
                os_printf("%02x ", rx_char);
                if ((rx_char == 0) && (rx_buffer_len == 0)) {
                    // Discard this leading zero, it's a comms artifact.
                } else if (rx_buffer_len >= RX_BUFFER_LENGTH) {
                    // Too many bytes, discard.
                } else {
                    // Store the received byte.
                    rx_buffer[rx_buffer_len++] = rx_char;
                }

                if (rx_buffer_len >= expected_len) {
                    // We have received enough characters to process the message.
                    process_response();
                    rx_buffer_len = 0;
                }
            }
            os_printf("\n");
        } else {
            // We're not supposed to process this message - it's probably a late reception from the inverter.
            char rx_char;
            for (uint8_t ii=0; ii < rx_len; ii++) {
                rx_char = READ_PERI_REG(UART_FIFO(UART0)) & 0xFF;
            }
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

    // Swap the UART 0 pins over, to suppress the start-up output.
    system_uart_swap();

    // Start the network.
    wifi_init();

    // Initialise the OTA flash system.
    ota_init();

    // Initialise the network debugging.
    dbg_init();

    // Start a timer for the transmissions, every five minutes.
    os_timer_disarm(&transmit_timer);
    os_timer_setfn(&transmit_timer, (os_timer_func_t *)transmit_cb, (void *)0);
    //os_timer_arm(&transmit_timer, 5 * 60 * 1000, 1);
    os_timer_arm(&transmit_timer, 1 * 60 * 1000, 1);

    // Prepare a timer for the timing out of a serial reception, but don't start it now (we haven't sent anything yet!)
    os_timer_disarm(&serial_rx_timer);
    os_timer_setfn(&serial_rx_timer, (os_timer_func_t *)serial_timeout_cb, (void *)0);

    // GPIO 4 is an output, start with it low.
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO4_U, FUNC_GPIO4);
    gpio_output_set(0, BIT4, BIT4, 0);

    // Call the transmit timer callback immediately, so we don't have to wait for 5 minutes for the first action.
    //transmit_cb();
}
