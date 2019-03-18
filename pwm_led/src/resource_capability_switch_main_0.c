/*
 * resource_capability_switch_main_0.c
 *
 *  Created on: Mar 15, 2019
 *      Author: osboxes
 */

#include <smartthings_resource.h>
#include "log.h"
#include <peripheral_io.h>

#define SWITCH_POWER_ON "on"
#define SWITCH_POWER_OFF "off"

#define RES_CAPABILITY_SWITCH_MAIN_0 "/capability/switch/main/0"
#define PROP_POWER "power"

static bool switch_status = false;

extern peripheral_error_e resource_pwm_driving(bool status);


void set_switch_status(bool status)
{
	switch_status = status;
	if (switch_status == true)
		resource_pwm_driving(true);
	else
		resource_pwm_driving(false);
}

bool handle_get_request_on_resource_capability_switch_main_0(smartthings_payload_h resp_payload, void *user_data)
{
	int error = SMARTTHINGS_RESOURCE_ERROR_NONE;
	char *str = NULL;

    _D("Received a GET request\n");

	if (switch_status == true)
		str = SWITCH_POWER_ON;
	else
		str = SWITCH_POWER_OFF;

	error = smartthings_payload_set_string(resp_payload, PROP_POWER, str);
	if (error != SMARTTHINGS_RESOURCE_ERROR_NONE)
		_E("smartthings_payload_set_string() failed, [%d]", error);

	_D("Power : %s", str);

    return true;
}

bool handle_set_request_on_resource_capability_switch_main_0(smartthings_payload_h payload, smartthings_payload_h resp_payload, void *user_data)
{
	char *str = NULL;
	char *res_str = NULL;
	int error = SMARTTHINGS_RESOURCE_ERROR_NONE;

    _D("Received a SET request");

    error = smartthings_payload_get_string(payload, PROP_POWER, &str);
	if (error != SMARTTHINGS_RESOURCE_ERROR_NONE)
		_E("smartthings_payload_get_string() failed, [%d]", error);

	if (strncmp(str, SWITCH_POWER_ON, strlen(SWITCH_POWER_ON))) {
		set_switch_status(false);
		res_str = SWITCH_POWER_OFF;
	} else {
		set_switch_status(true);
		res_str = SWITCH_POWER_ON;
	}

	free(str);

	error = smartthings_payload_set_string(resp_payload, PROP_POWER, res_str);
	if (error != SMARTTHINGS_RESOURCE_ERROR_NONE)
		_E("smartthings_payload_set_string() failed, [%d]", error);

    return true;
}
