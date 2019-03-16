/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "smartthings_resource.h"
#include <peripheral_io.h>
#include <stdint.h>
#include "log.h"

static const char *PROP_ILLUMINANCE = "illuminance";
extern uint16_t get_sensor_value(void);

bool handle_get_request_on_resource_capability_illuminancemeasurement_main_0(smartthings_payload_h resp_payload, void *user_data)
{
	uint16_t sensor_value;
	int error = SMARTTHINGS_RESOURCE_ERROR_NONE;

	// get distance sensor value
	sensor_value = get_sensor_value();

	// set sensor value
	error = smartthings_payload_set_int(resp_payload, PROP_ILLUMINANCE, sensor_value);
	if (error != SMARTTHINGS_RESOURCE_ERROR_NONE) {
		_E("smartthings_payload_set_int() failed, [%d]", error);
		return false;
	}

	return true;
}
