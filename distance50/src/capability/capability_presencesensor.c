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

static const char *PROP_VALUE = "value";
extern void get_presence_status(bool *status);

bool handle_get_request_on_resource_capability_presencesensor_main_0(smartthings_payload_h resp_payload, void *user_data)
{
	bool presence_status;
	int error = SMARTTHINGS_RESOURCE_ERROR_NONE;

	get_presence_status(&presence_status);

	error = smartthings_payload_set_bool(resp_payload, PROP_VALUE, presence_status);
	if (error != SMARTTHINGS_RESOURCE_ERROR_NONE) {
		_E("smartthings_payload_set_bool() failed, [%d]", error);
		return false;
	}

	return true;
}
