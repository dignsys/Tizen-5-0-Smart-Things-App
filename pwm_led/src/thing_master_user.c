/*
 * thing_master_user.c
 *
 *  Created on: Mar 18, 2019
 *      Author: osboxes
 */

#include <smartthings.h>

#include "log.h"
#include "thing_master_user.h"

smartthings_h st_h;
static bool is_init = false;

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
