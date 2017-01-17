/*
 * udp_debug.h: Sending of debug information via UDP, rather than serial.
 *
 * Author: Ian Marshall
 * Date: 14/06/2016
 */

#ifndef _UDP_DEBUG_H
#define _UDP_DEBUG_H

#include "ip_addr.h"

// The UDP destination port for the debug packets.
#define DBG_PORT 65432

// The number of bytes to use for the debug message buffer.
#define DBG_BUFFER_LEN 128

// Stores the address to which debug packets are sent in an ip_addr structure.
#define DBG_ADDR(ip) (ip)[0] = 10; (ip)[1] = 0; (ip)[2] = 1; (ip)[3] = 253;

/*
 * Performs the required initialisation to pass debug information through the network.
 */
void ICACHE_FLASH_ATTR dbg_init();

#endif
