/*
 * pwm_led.c
 *
 *  Created on: Mar 18, 2019
 *      Author: osboxes
 */

#include <tizen.h>
#include <service_app.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "log.h"
#include "thing_master_user.h"
#include "thing_resource_user.h"

static bool service_app_create(void *user_data)
{
	return true;
}

static void service_app_terminate(void *user_data)
{
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
