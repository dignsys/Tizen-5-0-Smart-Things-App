#include <tizen.h>
#include <service_app.h>
#include "log.h"

#include <peripheral_io.h>

#define LED_GPIO_SDTA7D 37
#define LED_ON			1
#define LED_OFF			0

peripheral_gpio_h gpio_h = NULL;

peripheral_error_e resource_led_driving(bool status)
{
	peripheral_error_e ret = PERIPHERAL_ERROR_NONE;
	int pin = LED_GPIO_SDTA7D;
	uint32_t value;

	if (status == true)
		value = LED_ON;
	else
		value = LED_OFF;

	if (gpio_h == NULL){
		// Opening a GPIO Handle
		if ((ret = peripheral_gpio_open(pin, &gpio_h)) != PERIPHERAL_ERROR_NONE ) {
			_E("peripheral_gpio_open() failed!![%d]", ret);
			return ret;
		}
	}

	// set the data transfer direction
	if ((ret = peripheral_gpio_set_direction(gpio_h, PERIPHERAL_GPIO_DIRECTION_OUT_INITIALLY_LOW)) != PERIPHERAL_ERROR_NONE) {
		_E("peripheral_gpio_set_direction() failed!![%d]", ret);
		return ret;
	}

	// write binary data to a peripheral
	if ((ret = peripheral_gpio_write(gpio_h, value)) != PERIPHERAL_ERROR_NONE) {
		_E("peripheral_gpio_write() failed!![%d]", ret);
		return ret;
	}

	if (gpio_h != NULL) {
		// Closing a GPIO Handle : close handle that is no longer used,
		if ((ret = peripheral_gpio_close(gpio_h)) != PERIPHERAL_ERROR_NONE ) {
			_E("peripheral_gpio_close() failed!![%d]", ret);
			return ret;
		}
		gpio_h = NULL;
	}

	return ret;
}

