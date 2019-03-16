/*
 * resource_capability_dustsensor_main_0.c
 *
 *  Created on: Mar 15, 2019
 *      Author: osboxes
 */
#include <stdint.h>
#include <smartthings_resource.h>
#include "log.h"

const char *PROP_DUSTLEVEL = "dustLevel";
const char *PROP_FINEDUSTLEVEL = "fineDustLevel";

extern void get_dust_level(uint32_t *dust_level);
extern void get_fine_dust_level(uint32_t *fine_dust_level);

bool handle_get_request_on_resource_capability_dustsensor_main_0(smartthings_payload_h resp_payload, void *user_data)
{
	_D("Received a GET request\n");
	int error = SMARTTHINGS_RESOURCE_ERROR_NONE;

	// A value representation of PM 10, micrograms per cubic meter
	uint32_t dust_level;

	get_dust_level(&dust_level);
	error = smartthings_payload_set_int(resp_payload, PROP_DUSTLEVEL, dust_level);
	if (error != SMARTTHINGS_RESOURCE_ERROR_NONE)
		_E("smartthings_payload_set_int() failed, [%d]", error);

	// A value representation of PM 2.5, micrograms per cubic meter
	uint32_t fine_dust_level;

	get_fine_dust_level(&fine_dust_level);
	error = smartthings_payload_set_int(resp_payload, PROP_FINEDUSTLEVEL, fine_dust_level);
	if (error != SMARTTHINGS_RESOURCE_ERROR_NONE)
		_E("smartthings_payload_set_int() failed, [%d]", error);

	return true;
}

