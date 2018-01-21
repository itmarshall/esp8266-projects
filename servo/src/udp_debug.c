/*
 * udp_debug.c: Sending of debug information via UDP, rather than serial.
 *
 * Author: Ian Marshall
 * Date: 14/06/2016
 */
#include "ets_sys.h"
#include "osapi.h"
#include "os_type.h"    
#include "ip_addr.h"
#include "espconn.h"
#include "user_interface.h"
#include "espmissingincludes.h"
#include "udp_debug.h"

// Structure holding the TCP connection information for the debug communications.
LOCAL struct espconn dbg_conn;

// UDP specific protocol structure for the debug communications.
LOCAL esp_udp dbg_proto;

// Buffer used for storing debug message bytes until a new line (\n) character is received, or it fills up.
LOCAL char dbg_buffer[DBG_BUFFER_LEN];

// The number of bytes currently used in the debug buffer.
LOCAL uint8_t dbg_buffer_len = 0;

/*
 * Receives a single character of output for debugging. This is used to send through the data via UDP.
 */
LOCAL void dbg_putc(char c) {
    // Add the character to the buffer.
    dbg_buffer[dbg_buffer_len++] = c;

    // See if we're ready to send the buffer through - a new-line (with other data) or full buffer will trigger a transmission.
    if (((c == '\n') && (dbg_buffer_len > 1)) || (dbg_buffer_len == DBG_BUFFER_LEN)) {
        // Set the destination IP address and port.
        DBG_ADDR(dbg_proto.remote_ip);
        dbg_proto.remote_port = DBG_PORT;

        // Prepare the UDP "connection" structure.
        dbg_conn.type = ESPCONN_UDP;
        dbg_conn.state = ESPCONN_NONE;
        dbg_conn.proto.udp = &dbg_proto;

        // Send the debug message via a UDP packet.
        espconn_create(&dbg_conn);
        espconn_send(&dbg_conn, dbg_buffer, dbg_buffer_len);
        espconn_delete(&dbg_conn);

        // Reset the buffer.
        dbg_buffer_len = 0;
    }
}

/*
 * Performs the required initialisation to pass debug information through the network.
 */
void ICACHE_FLASH_ATTR dbg_init() {
    os_install_putc1(dbg_putc);
}
