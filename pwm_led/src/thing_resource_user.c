/*
 * thing_resource_user.c
 *
 *  Created on: Mar 18, 2019
 *      Author: osboxes
 */

#include <smartthings_resource.h>
#include <smartthings_payload.h>

#include "log.h"
#include "thing_resource_user.h"

#define THING_RESOURCE_FILE_NAME	"resource.json"

static smartthings_resource_h st_handle;
static bool is_init = false;

static const char* RES_CAPABILITY_SWITCH_MAIN_0 = "/capability/switch/main/0";

/* get and set request handlers */
extern bool handle_get_request_on_resource_capability_switch_main_0(smartthings_payload_h resp_payload, void *user_data);
extern bool handle_set_request_on_resource_capability_switch_main_0(smartthings_payload_h payload, smartthings_payload_h resp_payload, void *user_data);

void _send_response_result_cb(smartthings_resource_error_e result, void *user_data)
{
	_D("app_control reply callback for send_response : result=[%d]", result);
}

void _notify_result_cb(smartthings_resource_error_e result, void *user_data)
{
	_D("app_control reply callback for notify : result=[%d]", result);
}

void _request_cb(smartthings_resource_h st_h, int req_id, const char *uri, smartthings_resource_req_type_e req_type,
				smartthings_payload_h payload, void *user_data)
{
	START;

	smartthings_payload_h resp_payload = NULL;

	smartthings_payload_create(&resp_payload);
	if (!resp_payload) {
		_E("Response payload is NULL");
		return;
	}

	bool result = false;

	if (req_type == SMARTTHINGS_RESOURCE_REQUEST_GET) {
		if (0 == strncmp(uri, RES_CAPABILITY_SWITCH_MAIN_0, strlen(RES_CAPABILITY_SWITCH_MAIN_0))) {
			result = handle_get_request_on_resource_capability_switch_main_0(resp_payload, user_data);
		}
	} else if (req_type == SMARTTHINGS_RESOURCE_REQUEST_SET) {
		if (0 == strncmp(uri, RES_CAPABILITY_SWITCH_MAIN_0, strlen(RES_CAPABILITY_SWITCH_MAIN_0))) {
			result = handle_set_request_on_resource_capability_switch_main_0(payload, resp_payload, user_data);
		}
	} else {
		_E("Invalid request type");
		smartthings_payload_destroy(resp_payload);
		END;
		return;
	}

	int error = SMARTTHINGS_RESOURCE_ERROR_NONE;

	error = smartthings_resource_send_response(st_h, req_id, uri, resp_payload, result);
	if (error != SMARTTHINGS_RESOURCE_ERROR_NONE) {
			smartthings_payload_destroy(resp_payload);
			_E("smartthings_resource_send_response() failed, err=[%d]", error);
			END;
			return;
	}

	if (req_type == SMARTTHINGS_RESOURCE_REQUEST_SET) {
			error = smartthings_resource_notify(st_h, uri, resp_payload);
			if (error != SMARTTHINGS_RESOURCE_ERROR_NONE) {
				_E("smartthings_resource_notify() failed, err=[%d]", error);
			}
	}

	if (smartthings_payload_destroy(resp_payload) != 0) {
		_E("smartthings_payload_destroy failed");
	}

	END;
	return;
}

static void _resource_connection_status_cb(smartthings_resource_h handle, smartthings_resource_connection_status_e status, void *user_data)
{
	START;

	if (status == SMARTTHINGS_RESOURCE_CONNECTION_STATUS_CONNECTED) {
		if (smartthings_resource_set_request_cb(st_handle, _request_cb, NULL) != 0) {
			_E("smartthings_resource_set_request_cb() is failed");
			return;
		}
	} else {
		_I("connection failed, status=[%d]", status);
	}

	END;
	return;
}

int init_resource_app()
{
	START;

	if (is_init) {
		_I("Already initialized!");
		return 0;
	}

	if (smartthings_resource_initialize(&st_handle, _resource_connection_status_cb, NULL) != 0) {
		_E("smartthings_resource_initialize() is failed");
		goto _out;
	}

	is_init = true;

	END;
	return 0;

_out :
	END;
	return -1;
}

int deinit_resource_app()
{
	START;

	if (!st_handle)
		return 0;

	smartthings_resource_unset_request_cb(st_handle);

	if (smartthings_resource_deinitialize(st_handle) != 0)
		return -1;

	is_init = false;

	END;
	return 0;
}
