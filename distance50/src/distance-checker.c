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
#include "smartthings.h"
#include "smartthings_resource.h"
#include "smartthings_payload.h"
#include "log.h"
#include <Ecore.h>
#include <peripheral_io.h>

#define _DEBUG_PRINT_
#ifdef _DEBUG_PRINT_
#include <sys/time.h>
#endif

#define VALUE_STR_LEN_MAX		32
#define EVENT_INTERVAL_SECOND	0.2f	// periodic sensor event timer
#define MUTEX_LOCK				pthread_mutex_lock(&mutex_lock)
#define MUTEX_UNLOCK			pthread_mutex_unlock(&mutex_lock)
#define MIN_RANGE				30		// Minimum distance range (Cm)
#define MAX_RANGE				120		// Maximum distance range (Cm)

#define GREEN_LED				35		// GPIO2_IO03
#define RED_LED					37		// GPIO2_IO05
//#define GREEN_LED				129		// GPIO_129
//#define RED_LED					128		// GPIO_128

#define LED_ON					1		// High
#define LED_OFF					0		// Low

static const char *RES_CAPABILITY_PRESENCESENSOR_MAIN_0 = "/capability/presenceSensor/main/0";
static const char *RES_CAPABILITY_SWITCH_MAIN_0 = "/capability/switch/main/0";
static const char *RES_CAPABILITY_ILLUMINANCEMEASUREMENT_MAIN_0 = "/capability/illuminanceMeasurement/main/0";

static const char *PROP_ILLUMINANCE = "illuminance";
static const char *PROP_VALUE = "value";

static Ecore_Timer *distance_sensor_event_timer;
static bool g_presence_status = false;
static bool g_switch_status = false;
static uint16_t g_distance = 0;
static pthread_mutex_t mutex_lock = PTHREAD_MUTEX_INITIALIZER;

static peripheral_gpio_h green_led_h = NULL;
static peripheral_gpio_h red_led_h = NULL;

#define THING_RESOURCE_FILE_NAME	"resource.json"

static smartthings_resource_h st_handle = NULL;
static bool is_init = false;
static bool is_resource_init = false;
static smartthings_h st_h;

smartthings_status_e st_things_status = -1;

extern bool resource_read_distance_sensor(uint16_t *sensor_value);
extern int resource_close_distance_sensor(void);
extern int resource_write_led(peripheral_gpio_h handle, int write_value);
extern void resource_close_led(peripheral_gpio_h handle);
extern int resource_open_led(int gpio_pin, peripheral_gpio_h *handle);

/* get and set request handlers */
extern bool handle_get_request_on_resource_capability_switch_main_0(smartthings_payload_h resp_payload, void *user_data);
extern bool handle_set_request_on_resource_capability_switch_main_0(smartthings_payload_h payload, smartthings_payload_h resp_payload, void *user_data);
extern bool handle_get_request_on_resource_capability_illuminancemeasurement_main_0(smartthings_payload_h resp_payload, void *user_data);
extern bool handle_get_request_on_resource_capability_presencesensor_main_0(smartthings_payload_h resp_payload, void *user_data);

bool get_switch_status(void)
{
	bool status = false;

	MUTEX_LOCK;
	status = g_switch_status;
	MUTEX_UNLOCK;

	return status;
}

void set_switch_status(bool status)
{
	MUTEX_LOCK;
	g_switch_status = status;
	MUTEX_UNLOCK;

}

uint16_t get_sensor_value(void)
{
	uint16_t value = 0;

	MUTEX_LOCK;
	value = g_distance;
	MUTEX_UNLOCK;

	return value;
}

static void set_sensor_value(uint16_t value)
{
	MUTEX_LOCK;
	g_distance = value;
	MUTEX_UNLOCK;
}

void get_presence_status(bool *status)
{
	MUTEX_LOCK;
	*status = g_presence_status;
	MUTEX_UNLOCK;
}

static void set_presence_status(bool status)
{
	MUTEX_LOCK;
	g_presence_status = status;
	MUTEX_UNLOCK;
}

static void update_sensor_value(void)
{
	bool status;
	int error = SMARTTHINGS_RESOURCE_ERROR_NONE;
	uint16_t sensor_value = 0;

	#ifdef _DEBUG_PRINT_
		sensor_value = get_sensor_value();
		struct timeval tv;
		gettimeofday(&tv, NULL);
		_I("[%d.%06d] distance : %d Cm", tv.tv_sec, tv.tv_usec, sensor_value);
	#endif


	status = get_switch_status();

	if (status == true) {
		sensor_value = get_sensor_value();

		#ifndef _DEBUG_PRINT_
		struct timeval tv;
		gettimeofday(&tv, NULL);
		_I("[%d.%06d] distance : %d Cm", tv.tv_sec, tv.tv_usec, sensor_value);
		#endif

		smartthings_payload_h resp_payload = NULL;

		// send notification to cloud server
		error = smartthings_payload_create(&resp_payload);
		if (error != SMARTTHINGS_RESOURCE_ERROR_NONE || !resp_payload) {
			_E("smartthings_payload_create() failed, [%d]", error);
			return;
		}

		error = smartthings_payload_set_int(resp_payload, PROP_ILLUMINANCE, sensor_value);
		if (error != SMARTTHINGS_RESOURCE_ERROR_NONE) {
			_E("smartthings_payload_set_int() failed, [%d]", error);
			smartthings_payload_destroy(resp_payload);
			return;
		}

		error = smartthings_resource_notify(st_handle, RES_CAPABILITY_ILLUMINANCEMEASUREMENT_MAIN_0, resp_payload);
		if (error != SMARTTHINGS_RESOURCE_ERROR_NONE) {
			_E("smartthings_resource_notify() failed, [%d]", error);
			smartthings_payload_destroy(resp_payload);
			return;
		}

		if (smartthings_payload_destroy(resp_payload)) {
			_E("smartthings_payload_destroy() failed");
			return;
		}
	}
}

/*
 * update presence status
 * On presence detected, set Red led:ON, Green led:OFF
 * On leace status, set Red led:OFF, Green led:ON
 */
static void update_presence_status(bool status)
{
	static bool present = false;
	int error = SMARTTHINGS_RESOURCE_ERROR_NONE;

	if (present == status) {
		// same status, do nothing
		return;
	}

	if (status == true) {
		//_I("Arrive - Red:ON, Green:OFF");
		present = true;
		set_presence_status(present);

		// set presence detected status
		resource_write_led(green_led_h, LED_OFF);
		resource_write_led(red_led_h, LED_ON);
	}
	else {
		//_I("Leave - Red:OFF, Green:ON");
		present = false;
		set_presence_status(present);

		// clear presence detected status
		resource_write_led(green_led_h, LED_ON);
		resource_write_led(red_led_h, LED_OFF);
	}

	if (g_switch_status == true) {
		smartthings_payload_h resp_payload = NULL;

		// create response payload
		error = smartthings_payload_create(&resp_payload);
		if (error != SMARTTHINGS_RESOURCE_ERROR_NONE || !resp_payload) {
			_E("smartthings_payload_create() failed, [%d]", error);
			return;
		}

		// set response data into payload
		error = smartthings_payload_set_bool(resp_payload, PROP_VALUE, status);
		if (error != SMARTTHINGS_RESOURCE_ERROR_NONE) {
			_E("smartthings_payload_set_bool() failed, [%d]", error);
			smartthings_payload_destroy(resp_payload);
			return;
		}

		// send notification to cloud server
		error = smartthings_resource_notify(st_handle, RES_CAPABILITY_PRESENCESENSOR_MAIN_0, resp_payload);
		if (error != SMARTTHINGS_RESOURCE_ERROR_NONE) {
			_E("smartthings_resource_notify() failed, [%d]", error);
			smartthings_payload_destroy(resp_payload);
			return;
		}

		if (smartthings_payload_destroy(resp_payload)) {
			_E("smartthings_payload_destroy() failed");
			return;
		}

		_I("notify presence status : %d", status);
	}
}

/*
 * timer event callback function
 * read distance sensor value from distance sensor
 * save sensor value and check for value is in MIN ~ MAX range
 * if value is detected, then update presence status
 */
static Eina_Bool _distance_sensor_interval_event_cb(void *data)
{
	uint16_t sensor_value;
	int ret = 0;

	// read sensor value
	ret = resource_read_distance_sensor(&sensor_value);
	if (ret != 0) {
		_E("invalid sensor value");
		return ECORE_CALLBACK_RENEW;
	} else {
		if (st_things_status != SMARTTHINGS_STATUS_REGISTERED_TO_CLOUD) {
			return ECORE_CALLBACK_RENEW;
		}

		if (!st_handle) {
			_D("st_handle is NULL");
			return ECORE_CALLBACK_RENEW;
		}

		// save sensor value
		set_sensor_value(sensor_value);
		update_sensor_value();

		#ifndef _DEBUG_PRINT_
		struct timeval tv;
		gettimeofday(&tv, NULL);
		_I("[%d.%06d] distance : %d Cm", tv.tv_sec, tv.tv_usec, sensor_value);
		#endif

		// check if value is within MIN ~ MAX range
		if ((sensor_value > MIN_RANGE) && (sensor_value < MAX_RANGE)) {
			update_presence_status(true);
		} else {
			update_presence_status(false);
		}
	}
	// reset event timer periodic
	return ECORE_CALLBACK_RENEW;
}

void clear_timer_resource(void)
{
	_I("clear_timer_resource...");
	if (distance_sensor_event_timer) {
		ecore_timer_del(distance_sensor_event_timer);
		distance_sensor_event_timer = NULL;
	}
}

void init_mutex(void)
{
	pthread_mutex_init(&mutex_lock, NULL);
}

void deinit_mutex(void)
{
	_I("deinit_mutex...");
	pthread_mutex_destroy(&mutex_lock);
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
		if (0 == strncmp(uri, RES_CAPABILITY_ILLUMINANCEMEASUREMENT_MAIN_0, strlen(RES_CAPABILITY_ILLUMINANCEMEASUREMENT_MAIN_0))) {
			result = handle_get_request_on_resource_capability_illuminancemeasurement_main_0(resp_payload, user_data);
		}
		if (0 == strncmp(uri, RES_CAPABILITY_PRESENCESENSOR_MAIN_0, strlen(RES_CAPABILITY_PRESENCESENSOR_MAIN_0))) {
			result = handle_get_request_on_resource_capability_presencesensor_main_0(resp_payload, user_data);
		}
	} else if (req_type == SMARTTHINGS_RESOURCE_REQUEST_SET) {
		if (0 == strncmp(uri, RES_CAPABILITY_SWITCH_MAIN_0, strlen(RES_CAPABILITY_SWITCH_MAIN_0))) {
			result = handle_set_request_on_resource_capability_switch_main_0(payload, resp_payload, user_data);
		}
	} else {
		_E("Invalid request type");
		smartthings_payload_destroy(resp_payload);
		return;
	}

	int error = SMARTTHINGS_RESOURCE_ERROR_NONE;

	error = smartthings_resource_send_response(st_h, req_id, uri, resp_payload, result);
	if (error != SMARTTHINGS_RESOURCE_ERROR_NONE) {
			smartthings_payload_destroy(resp_payload);
			_E("smartthings_resource_send_response() failed, err=[%d]", error);
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

	is_resource_init = false;

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
	st_things_status = status;

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
	START;

	is_init = false;

	if (!st_h) {
		_I("handle is already NULL");
		END;
		return 0;
	}

	smartthings_unset_user_confirm_cb(st_h);
	smartthings_unset_reset_confirm_cb(st_h);
	smartthings_unset_reset_result_cb(st_h);
	smartthings_unset_status_changed_cb(st_h);

	if (smartthings_deinitialize(st_h) != 0)  {
		_E("smartthings_deinitialize() is failed");
		END;
		return -1;
	}

	END;
	return 0;
}

static bool service_app_create(void *user_data)
{
	bool ret = true;

	init_mutex();

	if (resource_open_led(GREEN_LED, &green_led_h) != 0) {
		_E("GREEN LED resource open failed");
	}
	if (resource_open_led(RED_LED, &red_led_h) != 0) {
		_E("RED LED resource open failed");
	}

	distance_sensor_event_timer = ecore_timer_add(EVENT_INTERVAL_SECOND, _distance_sensor_interval_event_cb, NULL);
	if (!distance_sensor_event_timer) {
		_E("Failed to add distance_sensor_event_timer");
		ret = false;
	}

	return ret;
}

static void service_app_terminate(void *user_data)
{
	// clear event timer resource
	clear_timer_resource();

	// clear GPIO resource
	resource_close_led(green_led_h);
	resource_close_led(red_led_h);

	// clear distance sensor resource
	resource_close_distance_sensor();

	// deinit mutex resource
	deinit_mutex();
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

