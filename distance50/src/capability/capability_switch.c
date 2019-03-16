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

#include <stdio.h>
#include <stdbool.h>
#include "smartthings_resource.h"
#include "log.h"

#define VALUE_STR_LEN_MAX        32

static const char *PROP_POWER = "power";
static const char *VALUE_SWITCH_ON = "on";
static const char *VALUE_SWITCH_OFF = "off";
static char g_switch[VALUE_STR_LEN_MAX] = "off";

extern bool get_switch_status(void);
extern void set_switch_status(bool status);

bool handle_get_request_on_resource_capability_switch_main_0(smartthings_payload_h resp_payload, void *user_data)
{
	bool switch_status;
	int error = SMARTTHINGS_RESOURCE_ERROR_NONE;

	switch_status = get_switch_status();
	if (switch_status == true) {
		error = smartthings_payload_set_string(resp_payload, PROP_POWER, VALUE_SWITCH_ON);
		if (error != SMARTTHINGS_RESOURCE_ERROR_NONE) {
			_E("smartthings_payload_set_string() failed, [%d]", error);
			return false;
		}
	} else {
		error = smartthings_payload_set_string(resp_payload, PROP_POWER, VALUE_SWITCH_OFF);
		if (error != SMARTTHINGS_RESOURCE_ERROR_NONE) {
			_E("smartthings_payload_set_string() failed, [%d]", error);
			return false;
		}
	}

	return true;
}

bool handle_set_request_on_resource_capability_switch_main_0(smartthings_payload_h payload, smartthings_payload_h resp_payload, void *user_data)
{
	char *str_value = NULL;
	int error = SMARTTHINGS_RESOURCE_ERROR_NONE;

    error = smartthings_payload_get_string(payload, PROP_POWER, &str_value);
	if (error != SMARTTHINGS_RESOURCE_ERROR_NONE) {
		_E("smartthings_payload_get_string() failed, [%d]", error);
		return false;
	}

	/* check validation */
	if ((0 != strncmp(str_value, VALUE_SWITCH_ON, strlen(VALUE_SWITCH_ON)))
		&& (0 != strncmp(str_value, VALUE_SWITCH_OFF, strlen(VALUE_SWITCH_OFF)))) {
		_E("Not supported value!!");
		free(str_value);
		return false;
	}

	if (0 != strncmp(str_value, g_switch, strlen(g_switch))) {
		strncpy(g_switch, str_value, VALUE_STR_LEN_MAX);
		if (0 == strncmp(g_switch, VALUE_SWITCH_ON, strlen(VALUE_SWITCH_ON))) {
			set_switch_status(true);
		} else {
			set_switch_status(false);
		}
	}
	free(str_value);

	error = smartthings_payload_set_string(resp_payload, PROP_POWER, g_switch);
	if (error != SMARTTHINGS_RESOURCE_ERROR_NONE) {
		_E("smartthings_payload_set_string() failed, [%d]", error);
		return false;
	}

	return true;
}
