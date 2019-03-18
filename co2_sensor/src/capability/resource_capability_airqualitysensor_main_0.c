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
#include "log.h"

static const char *PROP_AIRQUALITY = "airQuality";
static const char *PROP_RANGE = "range";
static double g_range[2] = { 0, 10000. };
static size_t g_length = 2;
int32_t g_co2_sensor_value = 400;

extern int resource_update_co2_sensor_value(void);

bool handle_get_request_on_resource_capability_airqualitysensor_main_0(smartthings_payload_h resp_payload, void *user_data)
{
	int error = SMARTTHINGS_RESOURCE_ERROR_NONE;

	g_co2_sensor_value = resource_update_co2_sensor_value();
	error = smartthings_payload_set_int(resp_payload, PROP_AIRQUALITY, g_co2_sensor_value);
	if (error != SMARTTHINGS_RESOURCE_ERROR_NONE) {
		_E("smartthings_payload_set_int() failed, [%d]", error);
		return false;
	}

	error = smartthings_payload_set_double_array(resp_payload, PROP_RANGE, (double*)&g_range, g_length);
	if (error != SMARTTHINGS_RESOURCE_ERROR_NONE) {
		_E("smartthings_payload_set_double_array() failed, [%d]", error);
		return false;
	}

	return true;
}
