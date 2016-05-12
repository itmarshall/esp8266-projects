/*
 * user_main.c: Main entry-point for blinking an LED.
 */
#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"    
#include "include/espmissingincludes.h"

// Timer used to determine when the LED is to be turned on/off.
LOCAL os_timer_t blink_timer;

// The current state of the LED's output.
LOCAL uint8_t led_state = 0;

/*
 * Call-back for when the blink timer expires. This simply toggles the GPIO 4 state.
 */
LOCAL void ICACHE_FLASH_ATTR blink_cb(void *arg) {
    led_state = !led_state;
    GPIO_OUTPUT_SET(4, led_state);
}

/*
 * Entry point for the program. Sets up the microcontroller for use.
 */
void user_init(void) {
    // Initialise all GPIOs.
    gpio_init();

    // GPIO 4 is an output, start with it low.
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO4_U, FUNC_GPIO4);
    gpio_output_set(0, BIT4, BIT4, 0);

    // Start a timer for the flashing of the LED on GPIO 4, every 1 second, continuous.
    os_timer_disarm(&blink_timer);
    os_timer_setfn(&blink_timer, (os_timer_func_t *)blink_cb, (void *)0);
    os_timer_arm(&blink_timer, 1000, 1);
}
