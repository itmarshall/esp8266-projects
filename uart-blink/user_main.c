/*
 * user_main.c: Main entry-point for blinking an LED, with the rate controlled via messages from UART 0.
 *
 * Author: Ian Marshall
 * Date: 14/05/2016
 */
#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"    
#include "driver/uart.h"
#include "espmissingincludes.h"

// Timer used to determine when the LED is to be turned on/off.
LOCAL os_timer_t blink_timer;

// The current state of the LED's output.
LOCAL uint8_t led_state = 0;

// The new interval, while it's being received character by character from the UART.
uint16_t tmp_interval = 0;

/*
 * Call-back for when the blink timer expires. This simply toggles the GPIO 4 state.
 */
LOCAL void ICACHE_FLASH_ATTR blink_cb(void *arg) {
    // Update the LED's status.
    led_state = !led_state;
    GPIO_OUTPUT_SET(4, led_state);

    // Write a binary message (the 0x00 would terminate any string to os_printf).
    uint8_t message[] = {0x03, 0x02, 0x01, 0x00, 0x01, 0x02, 0x03, '\r', '\n'};
    uart0_tx_buffer(message, 9);

    // Write the LED status as a string.
    os_printf("LED state - %d.\n", led_state);
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
 * Receives the characters from the serial port.
 */
void ICACHE_FLASH_ATTR uart_rx_task(os_event_t *events) {
    if (events->sig == 0) {
        // Sig 0 is a normal receive. Get how many bytes have been received.
        uint8_t rx_len = (READ_PERI_REG(UART_STATUS(UART0)) >> UART_RXFIFO_CNT_S) & UART_RXFIFO_CNT;

        // Parse the characters, taking any digits as the new timer interval.
        char rx_char;
        for (uint8_t ii=0; ii < rx_len; ii++) {
            rx_char = READ_PERI_REG(UART_FIFO(UART0)) & 0xFF;
            if ((rx_char >= '0') && (rx_char <= '9')) {
                // This is a digit, update the interval.
                tmp_interval = (tmp_interval * 10) + (rx_char - '0');
            } else {
                // We have finished receiving digits.
                if (tmp_interval > 0) {
                    set_blink_timer(tmp_interval);
                    tmp_interval = 0;
                }
            }
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

    // Start the LED timer at 2s per change.
    set_blink_timer(2000);
}
