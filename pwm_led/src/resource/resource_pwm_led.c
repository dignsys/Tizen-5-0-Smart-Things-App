#include <tizen.h>
#include <service_app.h>
#include "log.h"

#include <peripheral_io.h>

#define PERIOD	(100000000)
#define DUTY	( 50000000)

#define ARTIK_PWM_CHIPID	0
#define ARTIK_PWM_PIN		2

#define PWM_CHIP_SDTA7D 0
#define PWM_PIN_SDTA7D	0

peripheral_pwm_h g_pwm_h = NULL;

peripheral_error_e resource_pwm_driving(bool status)
{
	peripheral_error_e ret = PERIPHERAL_ERROR_NONE;

	int chip = PWM_CHIP_SDTA7D;		// Chip 0
	int pin  = PWM_PIN_SDTA7D;		// Pin 0

	int period = PERIOD;
	int duty_cycle;

	if (status == true)
		duty_cycle = 0;
	else
		duty_cycle = PERIOD;

	if (g_pwm_h == NULL){
		// Opening a PWM Handle : The chip and pin parameters required for this function must be set
		if ((ret = peripheral_pwm_open(chip, pin, &g_pwm_h)) != PERIPHERAL_ERROR_NONE ) {
			_E("peripheral_pwm_open() failed!![%d]", ret);
			return ret;
		}
	}

	// Setting the Period : sets the period to 20 milliseconds. The unit is nanoseconds
	if ((ret = peripheral_pwm_set_period(g_pwm_h, period)) != PERIPHERAL_ERROR_NONE) {
		_E("peripheral_pwm_set_period() failed!![%d]", ret);
		return ret;
	}

	// Setting the Duty Cycle : sets the duty cycle to 1~2 milliseconds. The unit is nanoseconds
	if ((ret = peripheral_pwm_set_duty_cycle(g_pwm_h, duty_cycle)) != PERIPHERAL_ERROR_NONE) {
		_E("peripheral_pwm_set_duty_cycle() failed!![%d]", ret);
		return ret;
	}

	// Enabling Repetition
	if ((ret = peripheral_pwm_set_enabled(g_pwm_h, true)) != PERIPHERAL_ERROR_NONE) {
		_E("peripheral_pwm_set_enabled() failed!![%d]", ret);
		return ret;
	}

	if (g_pwm_h != NULL) {
		// Closing a PWM Handle : close a PWM handle that is no longer used,
		if ((ret = peripheral_pwm_close(g_pwm_h)) != PERIPHERAL_ERROR_NONE ) {
			_E("peripheral_pwm_close() failed!![%d]", ret);
			return ret;
		}
		g_pwm_h = NULL;
	}

	return ret;
}
