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

#include <service_app.h>
#include <stdio.h>
#include "smartthings.h"
#include "smartthings_resource.h"
#include "smartthings_payload.h"
#include "resource/resource_co2_sensor.h"
#include "log.h"
#include <signal.h>

static const char *RES_CAPABILITY_SWITCH = "/capability/switch/main/0";
static const char *RES_CAPABILITY_THERMOSTATCOOLINGSETPOINT = "/capability/thermostatCoolingSetpoint/main/0";
static const char *RES_CAPABILITY_AIRQUALITYSENSOR = "/capability/airQualitySensor/main/0";

smartthings_resource_h st_handle = NULL;
static bool is_init = false;
static bool is_resource_init = false;
static smartthings_h st_h;

smartthings_status_e st_things_status = -1;

pthread_mutex_t  mutex = PTHREAD_MUTEX_INITIALIZER;
extern int thread_done; /* resource_co2_sensor.c */

/* get and set request handlers */
extern bool handle_get_request_on_resource_capability_switch_main_0(smartthings_payload_h resp_payload, void *user_data);
extern bool handle_set_request_on_resource_capability_switch_main_0(smartthings_payload_h payload, smartthings_payload_h resp_payload, void *user_data);
extern bool handle_get_request_on_resource_capability_airqualitysensor_main_0(smartthings_payload_h resp_payload, void *user_data);
extern bool handle_get_request_on_resource_capability_thermostatcoolingsetpoint_main_0(smartthings_payload_h resp_payload, void *user_data);
extern bool handle_set_request_on_resource_capability_thermostatcoolingsetpoint_main_0(smartthings_payload_h payload, smartthings_payload_h resp_payload, void *user_data);

extern void *thread_sensor_main(void *arg);
extern void *thread_sensor_notify(void *arg);

void sig_handler(int sig)
{
	switch (sig) {
	case SIGINT:
		_I("SIGINT received");
		thread_done = 1;
		break;
	case SIGSEGV:
		_I("SIGSEGV received");
		thread_done = 1;
		break;
	case SIGTERM:
		_I("SIGTERM received");
		thread_done = 1;
		break;
	default:
		_E("wasn't expecting that sig [%d]", sig);
		abort();
	}
}

/* main loop */
void handle_main_loop(void)
{
	pthread_t p_thread[2];
	int ret = 0;

	signal(SIGSEGV, sig_handler);
	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);

	pthread_mutex_init(&mutex, NULL);

	ret = pthread_create(&p_thread[0], NULL, &thread_sensor_main, NULL);
	if (ret != 0) {
		_E("[ERROR] thread_sensor_main create failed, ret=%d", ret);
		return;
	}
	ret = pthread_create(&p_thread[1], NULL, &thread_sensor_notify, NULL);
	if (ret != 0) {
		_E("[ERROR] thread_sensor_notify create failed, ret=%d", ret);
		return;
	}
}

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
		if (0 == strncmp(uri, RES_CAPABILITY_SWITCH, strlen(RES_CAPABILITY_SWITCH))) {
			result = handle_get_request_on_resource_capability_switch_main_0(resp_payload, user_data);
		}
		if (0 == strncmp(uri, RES_CAPABILITY_AIRQUALITYSENSOR, strlen(RES_CAPABILITY_AIRQUALITYSENSOR))) {
			result = handle_get_request_on_resource_capability_airqualitysensor_main_0(resp_payload, user_data);
		}
		if (0 == strncmp(uri, RES_CAPABILITY_THERMOSTATCOOLINGSETPOINT, strlen(RES_CAPABILITY_THERMOSTATCOOLINGSETPOINT))) {
			result = handle_get_request_on_resource_capability_thermostatcoolingsetpoint_main_0(resp_payload, user_data);
		}
	} else if (req_type == SMARTTHINGS_RESOURCE_REQUEST_SET) {
		if (0 == strncmp(uri, RES_CAPABILITY_SWITCH, strlen(RES_CAPABILITY_SWITCH))) {
			result = handle_set_request_on_resource_capability_switch_main_0(payload, resp_payload, user_data);
		}
		if (0 == strncmp(uri, RES_CAPABILITY_THERMOSTATCOOLINGSETPOINT, strlen(RES_CAPABILITY_THERMOSTATCOOLINGSETPOINT))) {
			result = handle_set_request_on_resource_capability_thermostatcoolingsetpoint_main_0(payload, resp_payload, user_data);
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

	if (is_resource_init) {
		_I("Already initialized!");
		return 0;
	}

	if (smartthings_resource_initialize(&st_handle, _resource_connection_status_cb, NULL) != 0) {
		_E("smartthings_resource_initialize() is failed");
		goto _out;
	}

	is_resource_init = true;

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

void _user_confirm_cb(smartthings_h handle, void *user_data)
{
	START;

	if (smartthings_send_user_confirm(handle, true) != 0)
		_E("smartthings_send_user_confirm() is failed");

	END;
	return;
}

void _reset_confirm_cb(smartthings_h handle, void *user_data)
{
	START;

	if (smartthings_send_reset_confirm(handle, true) != 0)
		_E("smartthings_send_reset_confirm() is failed");

	END;
	return;
}

static void _reset_result_cb(smartthings_h handle, bool result, void *user_data)
{
	START;

	_I("reset result = [%d]", result);

	END;
	return;
}

static void _thing_status_cb(smartthings_h handle, smartthings_status_e status, void *user_data)
{
	START;

	_D("Received status changed cb : status = [%d]", status);

	switch (status) {
	case SMARTTHINGS_STATUS_NOT_READY:
			_I("status: [%d] [%s]", status, "SMARTTHINGS_STATUS_NOT_READY");
			break;
	case SMARTTHINGS_STATUS_INIT:
			_I("status: [%d] [%s]", status, "SMARTTHINGS_STATUS_INIT");
			break;
	case SMARTTHINGS_STATUS_ES_STARTED:
			_I("status: [%d] [%s]", status, "SMARTTHINGS_STATUS_ES_STARTED");
			break;
	case SMARTTHINGS_STATUS_ES_DONE:
			_I("status: [%d] [%s]", status, "SMARTTHINGS_STATUS_ES_DONE");
			break;
	case SMARTTHINGS_STATUS_ES_FAILED_ON_OWNERSHIP_TRANSFER:
			_I("status: [%d] [%s]", status, "SMARTTHINGS_STATUS_ES_FAILED_ON_OWNERSHIP_TRANSFER");
			break;
	case SMARTTHINGS_STATUS_CONNECTING_TO_AP:
			_I("status: [%d] [%s]", status, "SMARTTHINGS_STATUS_CONNECTING_TO_AP");
			break;
	case SMARTTHINGS_STATUS_CONNECTED_TO_AP:
			_I("status: [%d] [%s]", status, "SMARTTHINGS_STATUS_CONNECTED_TO_AP");
			break;
	case SMARTTHINGS_STATUS_CONNECTING_TO_AP_FAILED:
			_I("status: [%d] [%s]", status, "SMARTTHINGS_STATUS_CONNECTING_TO_AP_FAILED");
			break;
	case SMARTTHINGS_STATUS_REGISTERING_TO_CLOUD:
			_I("status: [%d] [%s]", status, "SMARTTHINGS_STATUS_REGISTERING_TO_CLOUD");
			break;
	case SMARTTHINGS_STATUS_REGISTERED_TO_CLOUD:
			_I("status: [%d] [%s]", status, "SMARTTHINGS_STATUS_REGISTERED_TO_CLOUD");
			break;
	case SMARTTHINGS_STATUS_REGISTERING_FAILED_ON_SIGN_IN:
			_I("status: [%d] [%s]", status, "SMARTTHINGS_STATUS_REGISTERING_FAILED_ON_SIGN_IN");
			break;
	case SMARTTHINGS_STATUS_REGISTERING_FAILED_ON_PUB_RES:
			_I("status: [%d] [%s]", status, "SMARTTHINGS_STATUS_REGISTERING_FAILED_ON_PUB_RES");
			break;
	default:
			_E("status: [%d][%s]", status, "Unknown Status");
			break;
	}
	END;
	return;
}

static void _things_connection_status_cb(smartthings_h handle, smartthings_connection_status_e status, void *user_data)
{
	START;

	_D("Received connection status changed cb : status = [%d]", status);

	if (status == SMARTTHINGS_CONNECTION_STATUS_CONNECTED) {
		const char* dev_name = "IoT Test Device";
		int wifi_mode = SMARTTHINGS_WIFI_MODE_11B | SMARTTHINGS_WIFI_MODE_11G | SMARTTHINGS_WIFI_MODE_11N;
		int wifi_freq = SMARTTHINGS_WIFI_FREQ_24G | SMARTTHINGS_WIFI_FREQ_5G;

		if (smartthings_set_device_property(handle, dev_name, wifi_mode, wifi_freq) != 0) {
			_E("smartthings_initialize() is failed");
			return;
		}

		if (smartthings_set_certificate_file(handle, "certificate.pem", "privatekey.der") != 0) {
			_E("smartthings_set_certificate_file() is failed");
			return;
		}

		if (smartthings_set_user_confirm_cb(st_h, _user_confirm_cb, NULL) != 0) {
			_E("smartthings_set_user_confirm_cb() is failed");
			return;
		}

		if (smartthings_set_reset_confirm_cb(handle, _reset_confirm_cb, NULL) != 0) {
			_E("smartthings_set_reset_confirm_cb() is failed");
			return;
		}

		if (smartthings_set_reset_result_cb(handle, _reset_result_cb, NULL) != 0) {
			_E("smartthings_set_reset_confirm_cb() is failed");
			return;
		}

		if (smartthings_set_status_changed_cb(handle, _thing_status_cb, NULL) != 0) {
			_E("smartthings_set_status_changed_callback() is failed");
			return;
		}

		if (smartthings_start(handle) != 0) {
			_E("smartthings_start() is failed");
			return;
		}

		bool is_es_completed = false;
		if (smartthings_get_easysetup_status(handle, &is_es_completed) != 0) {
			_E("smartthings_get_easysetup_status() is failed");
			return;
		}

		if (is_es_completed == true) {
			_I("Easysetup is already done");
			return;
		}

		if (smartthings_start_easysetup(handle) != 0) {
			_E("smartthings_start_easysetup() is failed");
			return;
		}
	} else {
			_E("connection failed, status=[%d]", status);
	}

	END;
	return;
}

int init_master_app()
{
	START;

	if (is_init) {
		_I("Already initialized!");
		END;
		return 0;
	}

	if (smartthings_initialize(&st_h, _things_connection_status_cb, NULL) != 0) {
		_E("smartthings_initialize() is failed");
		goto _out;
	}

	is_init = true;

	END;
	return 0;

_out :
	END;
	return -1;
}

int deinit_master_app()
{
	is_init = false;

	if (!st_h) {
		_I("handle is already NULL");
		return 0;
	}

	smartthings_unset_user_confirm_cb(st_h);
	smartthings_unset_reset_confirm_cb(st_h);
	smartthings_unset_reset_result_cb(st_h);
	smartthings_unset_status_changed_cb(st_h);

	if (smartthings_deinitialize(st_h) != 0)  {
		_E("smartthings_deinitialize() is failed");
		return -1;
	}

	return 0;
}

static bool service_app_create(void *user_data)
{
	handle_main_loop();

	return true;
}

static void service_app_terminate(void *user_data)
{
	MUTEX_LOCK;
	thread_done = 1;
	MUTEX_UNLOCK;

	int status = pthread_mutex_destroy(&mutex);
    _I("mutex destroy status = %d", status);
}

static void service_app_control(app_control_h app_control, void *user_data)
{
	if (app_control == NULL) {
		_E("app_control is NULL");
		return;
	}

	init_master_app();
	init_resource_app();
}

int main(int argc, char *argv[])
{
	service_app_lifecycle_callback_s event_callback;

	event_callback.create = service_app_create;
	event_callback.terminate = service_app_terminate;
	event_callback.app_control = service_app_control;

	return service_app_main(argc, argv, &event_callback, NULL);
}
