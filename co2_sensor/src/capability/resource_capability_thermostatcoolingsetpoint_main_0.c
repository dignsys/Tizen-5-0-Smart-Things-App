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

static const char* PROP_TEMPERATURE = "temperature";
static const char* PROP_RANGE = "range";
static const char* PROP_UNITS = "units";

static double g_temperature = 0;
static double g_range[2] = { 0, 5000. };
static size_t g_length = 2;
static char g_unit[4] = "mV";

extern void resource_init_co2_sensor(void);
extern int resource_get_co2_sensor_parameter(void);
extern void resource_set_sensor_parameter(int zero_volts);

bool handle_get_request_on_resource_capability_thermostatcoolingsetpoint_main_0(smartthings_payload_h resp_payload, void *user_data)
{
	int error = SMARTTHINGS_RESOURCE_ERROR_NONE;

	if (0 == g_temperature) {
		g_temperature = (double)resource_get_co2_sensor_parameter();
		error = smartthings_payload_set_double(resp_payload, PROP_TEMPERATURE, g_temperature);
		if (error != SMARTTHINGS_RESOURCE_ERROR_NONE) {
			_E("smartthings_payload_set_double() failed, [%d]", error);
			return false;
		}
	}

	error = smartthings_payload_set_double_array(resp_payload, PROP_RANGE, (double*)&g_range, g_length);
	if (error != SMARTTHINGS_RESOURCE_ERROR_NONE) {
		_E("smartthings_payload_set_double_array() failed, [%d]", error);
		return false;
	}

	error = smartthings_payload_set_string(resp_payload, PROP_UNITS, g_unit);
	if (error != SMARTTHINGS_RESOURCE_ERROR_NONE) {
		_E("smartthings_payload_set_string() failed, [%d]", error);
		return false;
	}

	return true;
}

bool handle_set_request_on_resource_capability_thermostatcoolingsetpoint_main_0(smartthings_payload_h payload, smartthings_payload_h resp_payload, void *user_data)
{
	int error = SMARTTHINGS_RESOURCE_ERROR_NONE;

	double dvalue = 0;
    error = smartthings_payload_get_double(payload, PROP_TEMPERATURE, &dvalue);
	if (error != SMARTTHINGS_RESOURCE_ERROR_NONE) {
		_E("smartthings_payload_get_double() failed, [%d]", error);
		return false;
	}
	g_temperature = dvalue;
	resource_set_sensor_parameter((int32_t)g_temperature);
	resource_init_co2_sensor();

	error = smartthings_payload_set_double(resp_payload, PROP_TEMPERATURE, dvalue);
	if (error != SMARTTHINGS_RESOURCE_ERROR_NONE) {
		_E("smartthings_payload_set_double() failed, [%d]", error);
		return false;
	}

	char *str_value = NULL;
    error = smartthings_payload_get_string(payload, PROP_UNITS, &str_value);
	if (error != SMARTTHINGS_RESOURCE_ERROR_NONE) {
		_E("smartthings_payload_get_string() failed, [%d]", error);
		free(str_value);
		return false;
	}
	error = smartthings_payload_set_string(resp_payload, PROP_UNITS, g_unit);
	if (error != SMARTTHINGS_RESOURCE_ERROR_NONE) {
		_E("smartthings_payload_set_string() failed, [%d]", error);
		free(str_value);
		return false;
	}
	free(str_value);

	double davalue[2] = { 0, };
	size_t dlength = 0;
	error = smartthings_payload_get_double_array(payload, PROP_RANGE, (double**)&davalue, &dlength);
	if (error != SMARTTHINGS_RESOURCE_ERROR_NONE) {
		_E("smartthings_payload_get_double_array() failed, [%d]", error);
		return false;
	}

	error = smartthings_payload_set_double_array(resp_payload, PROP_RANGE, g_range, g_length);
	if (error != SMARTTHINGS_RESOURCE_ERROR_NONE) {
		_E("smartthings_payload_set_double_array() failed, [%d]", error);
		return false;
	}

	return true;
}
