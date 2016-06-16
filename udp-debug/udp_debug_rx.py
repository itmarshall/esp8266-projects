#!/usr/bin/env python
#
# udp_debug_rx.py - receives UDP debug packets, and prints them out
#
# Usage:
#   udp_debug_rx.py <IP>
#
# Where:
#   <IP> the IP address from which debug packets are printed, or all addresses, if not supplied
#
# Author: Ian Marshall
# Date: 13/06/2016
#

from __future__ import print_function
from datetime import datetime

import socket
import sys

PORT=65432

# Get the filtering details from the parameter, if any.
match_addr = ''
if len(sys.argv) > 1:
	match_addr = sys.argv[1]

# Prepare the UDP socket.
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.bind(('', PORT))

# Variables that are used to decide if we need to "inject" a new-line character in the output.
last_addr = ''
last_nl = False

# Repeat forever, to receive multiple packets.
while True:
	# Wait for a packet to arrive.
	message, (addr, port) = s.recvfrom(1024);

	# See if we're filtering for this address.
	if len(match_addr) != 0 and match_addr != addr:
		continue

	if not last_nl and addr != last_addr:
		# The last message did not end in a new-line, but was from a different IP, so we want to put a new-line in.
		print('', end='\n')
	
	# Print the received message.
	if not last_nl and addr == last_addr:
		# This is a message continuation, just print the message contents.
		print(message, end='')
	else:
		# Get the current time.
		dt = datetime.now().strftime('%Y-%m-%d %H-%M-%S.%f')[:-3]

		# Print the address, date/time and the message.
		print(addr, dt, message, sep=': ', end='')

	# Remember the status of this message, ready for the next one.
	last_addr = addr
	last_nl = message[-1:] == '\n'
