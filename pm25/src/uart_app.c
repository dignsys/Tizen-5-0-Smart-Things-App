/*
 * uart_app.c
 *
 *  Created on: Mar 15, 2019
 *      Author: osboxes
 */

#include <tizen.h>
#include <service_app.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <Ecore.h>
#include "log.h"
#include "thing_master_user.h"
#include "thing_resource_user.h"
#include "resource/resource_pms7003.h"
#include <smartthings_resource.h>
#include <smartthings_payload.h>

//#define _DEBUG_PRINT_
#ifdef _DEBUG_PRINT_
#include <sys/time.h>
#endif

#define EVENT_INTERVAL_SECOND	(1.0f)	// sensor event timer : 1 second interval

Ecore_Timer *sensor_event_timer = NULL;
pthread_mutex_t  mutex_lock = PTHREAD_MUTEX_INITIALIZER;
static bool g_switch_status;
static const char *RES_CAPABILITY_DUSTSENSOR_MAIN_0 = "/capability/dustSensor/main/0";

#define MUTEX_LOCK		pthread_mutex_lock(&mutex_lock)
#define MUTEX_UNLOCK	pthread_mutex_unlock(&mutex_lock)
#define UNUSED(x)		(void)(x)

_concentration_unit_t	standard_particle;	// CF=1，standard particle
_concentration_unit_t	atmospheric_env;	// under atmospheric environment

extern smartthings_resource_h st_handle;
extern const char *PROP_DUSTLEVEL;
extern const char *PROP_FINEDUSTLEVEL;

/* resource pms7003 functions */
extern bool resource_pms7003_init(void);
extern void resource_pms7003_fini(void);
extern bool resource_pms7003_read(void);

static void _init_mutex(void)
{
	pthread_mutex_init(&mutex_lock, NULL);
}

static void _deinit_mutex(void)
{
	_I("deinit_mutex...");
	pthread_mutex_destroy(&mutex_lock);
}

static bool _get_switch_status(void)
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

void set_sensor_value(_pms7003_protocol_t pms7003_protocol)
{
	MUTEX_LOCK;
	standard_particle.PM1_0 = pms7003_protocol.standard_particle.PM1_0;
	standard_particle.PM2_5 = pms7003_protocol.standard_particle.PM2_5;
	standard_particle.PM10  = pms7003_protocol.standard_particle.PM10;
	MUTEX_UNLOCK;

#ifdef _DEBUG_PRINT_
	struct timeval tv;
	gettimeofday(&tv, NULL);
	_I("[%d.%06d] [ PM1.0: %d ug/m3 | PM2.5: %d ug/m3 | PM10: %d ug/m3 ]",
			tv.tv_sec, tv.tv_usec,
			standard_particle.PM1_0,
			standard_particle.PM2_5,
			standard_particle.PM10);
#endif
}

// get PM10 level
void get_dust_level(uint32_t *dust_level)
{
	MUTEX_LOCK;
	*dust_level = standard_particle.PM10;
	MUTEX_UNLOCK;
}

// get PM2.5 level
void get_fine_dust_level(uint32_t *fine_dust_level)
{
	MUTEX_LOCK;
	*fine_dust_level = standard_particle.PM2_5;
	MUTEX_UNLOCK;
}

static Eina_Bool _sensor_interval_event_cb(void *data)
{
	int error = SMARTTHINGS_RESOURCE_ERROR_NONE;
	smartthings_payload_h resp_payload = NULL;
	bool switch_status = false;

	// read sensor data from PMS7003 module
	if (!resource_pms7003_read()) {
		_E("resource_pms7003_read Failed");

		// cancel periodic event timer operation
		return ECORE_CALLBACK_RENEW;
	} else {
		// get sensor value from PMS7003 module
		uint32_t dust = 0;
		uint32_t fine = 0;
		get_dust_level(&dust);			// PM10 level
		get_fine_dust_level(&fine);		// PM2.5 level

		// send notification when switch is on state.
		switch_status = _get_switch_status();
		if (switch_status) {
			#ifndef _DEBUG_PRINT_
				struct timeval tv;
				gettimeofday(&tv, NULL);
				_I("[%d.%06d] dustLevel : %d ug/m3, fineDustLevel : %d ug/m3", tv.tv_sec, tv.tv_usec, dust, fine);
			#endif

			// send notification to cloud server
			error = smartthings_payload_create(&resp_payload);
			if (error != SMARTTHINGS_RESOURCE_ERROR_NONE || !resp_payload) {
				_E("smartthings_payload_create() failed, [%d]", error);
				return ECORE_CALLBACK_CANCEL;
			}

			error = smartthings_payload_set_int(resp_payload, PROP_DUSTLEVEL, (int)dust);
			if (error != SMARTTHINGS_RESOURCE_ERROR_NONE) {
				_E("smartthings_payload_set_int() failed, [%d]", error);
				smartthings_payload_destroy(resp_payload);
				return ECORE_CALLBACK_CANCEL;
			}
			error = smartthings_payload_set_int(resp_payload, PROP_FINEDUSTLEVEL, (int)fine);
			if (error != SMARTTHINGS_RESOURCE_ERROR_NONE) {
				_E("smartthings_payload_set_int() failed, [%d]", error);
				smartthings_payload_destroy(resp_payload);
				return ECORE_CALLBACK_CANCEL;
			}

			error = smartthings_resource_notify(st_handle, RES_CAPABILITY_DUSTSENSOR_MAIN_0, resp_payload);
			if (error != SMARTTHINGS_RESOURCE_ERROR_NONE) {
				_E("smartthings_resource_notify() failed, [%d]", error);
				smartthings_payload_destroy(resp_payload);
				return ECORE_CALLBACK_CANCEL;
			}

			if (smartthings_payload_destroy(resp_payload))
				_E("smartthings_payload_destroy() failed");
		}
	}

	// reset next event timer
	return ECORE_CALLBACK_RENEW;
}

static void _clear_timer_resource(void)
{
	_I("clear_timer_resource...");

	if (sensor_event_timer) {
		ecore_timer_del(sensor_event_timer);
		sensor_event_timer = NULL;
	}
}

static bool service_app_create(void *user_data)
{
	bool ret = true;
	_init_mutex();

	ret = resource_pms7003_init();
	if (ret == false) {
		_E("Failed to resource_pms7003_init");
		ret = false;
	}

	sensor_event_timer = ecore_timer_add(EVENT_INTERVAL_SECOND, _sensor_interval_event_cb, NULL);
	if (!sensor_event_timer) {
		_E("Failed to add sensor_event_timer");
		ret = false;
	}
	return ret;
}

static void service_app_terminate(void *user_data)
{
	_clear_timer_resource();
	resource_pms7003_fini();
	_deinit_mutex();

	/*terminate resource*/
	if (deinit_resource_app() != 0) {
		_E("deinit_resource_app failed");
	}

	/*terminate master*/
	if (deinit_master_app() != 0) {
		_E("deinit_master_app failed");
	}

	return;
}

static void service_app_control(app_control_h app_control, void *user_data)
{
	if (app_control == NULL) {
		_E("app_control is NULL");
		return;
	}

	init_master_app();
	init_resource_app();

	return;
}

int main(int argc, char *argv[])
{
	service_app_lifecycle_callback_s event_callback;

	event_callback.create = service_app_create;
	event_callback.terminate = service_app_terminate;
	event_callback.app_control = service_app_control;

	return service_app_main(argc, argv, &event_callback, NULL);
}
