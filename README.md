# esp8266-projects

This repository is used to house sample projects for the ESP8266 microcontroller. Mostly, this will be used to house example code that I put in [my blog] [blog] related to the ESP8266 that are not worthy of their own repository. Each sub-directory in this repository is a wholly self-contained, separate build, with its own make file.

[blog]: http://smallbits.marshall-tribe.net

So far, this repository contains:

* blink - The "Hello World" of microcontrollers! Simply blinks a LED on/off every second. From [this blog post] [blinkpost].
* uart-blink - Extends the "blink" example to read intervals for the blinking from the serial UART. [Blog post] [uartblinkpost].
* net-blink - The "blink" example now using TCP or UDP to set the blinking interval. [Blog post] [netblinkpost].
* ota-tcp - A simple way of updating the firmware of an ESP8266 wirelessly, or "over the air" (OTA) using TCP/IP. [Blog post] [otatcppost].
* udp-debug - Redirecting the output from debug code in "os_printf" to a computer for remote debugging. [Blog post] [udpdebugpost].
* dot - My first ESP8266 project, for a doorbell notification system that sends a notification to my phone via Telegram. [Blog post] [dotpost].
* uart-suppression - A way of hiding the initial boot messages that the ESP8266 writes upon startup. This can cause problems for some systems that aren't expecting this information. [Blog post] [uartsuppressionpost].
* delta-reader - Another ESP8266 project, for reading values from a Delta Solivia Inverter for my solar panels and sending it to my server for storage. [Blog post] [deltareaderpost].
* web-bootstrap - How do you get your ESP8266 configured for your LAN without having to hard-code the network name/password? You run a bootstrap code that starts a web server, and lets you simply type it in! [Blog post] [webbootstrappost].
* servo - Getting the ESP8266 to move a servo motor. This one is controlled via an in-built web server. [Blog post] [servopost].
* esp-now - Making two ESP8266s talk to each other without the usual overheads. [Blog post] [espnowpost].

[blinkpost]: http://smallbits.marshall-tribe.net/blog/2016/05/07/esp8266-first-steps
[uartblinkpost]: http://smallbits.marshall-tribe.net/blog/2016/05/14/esp8266-uart-fun
[netblinkpost]: http://smallbits.marshall-tribe.net/blog/2016/05/21/esp8266-networking-basics
[otatcppost]: http://smallbits.marshall-tribe.net/blog/2016/05/29/esp8266-pushing-ota-upgrades
[udpdebugpost]: http://smallbits.marshall-tribe.net/blog/2016/06/15/esp8266-remote-debug
[dotpost]: http://smallbits.marshall-tribe.net/blog/2016/07/02/dot-first-project
[uartsuppressionpost]: http://smallbits.marshall-tribe.net/blog/2016/11/13/esp8266-quiet-uart
[deltareaderpost]: http://smallbits.marshall-tribe.net/blog/2017/01/17/delta-reader-project
[webbootstrappost]: http://smallbits.marshall-tribe.net/blog/2017/12/31/esp8266-web-bootstrap
[servopost]: http://smallbits.marshall-tribe.net/blog/2018/01/21/esp8266-move-servo
[espnowpost]: http://smallbits.marshall-tribe.net/blog/2018/05/20/esp8266-now-talk

All code in this repository is under the "unlicence", which means that you can use it in your own projects without worrying about its compatibility with the licence of your code.
