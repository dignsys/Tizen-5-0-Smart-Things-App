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
#include <unistd.h>
#include <math.h>
#include <app_common.h>
#include "smartthings_resource.h"
#include "resource/resource_co2_sensor.h"
#include "log.h"

#define CO2_DATA			"co2_data"	// save co2 data

#define MAX_PATH_LEN		128
#define DC_GAIN				(8.500)		// refer to schematic, (R2 + R3) / R2 = 8.5
#define DEFAULT_ZERO_VOLTS	(2.950)
#define DEFAULT_RANGE_VOLTS	(0.400)		// refer to datasheet
#define DEFAULT_NOTIFY_TIME	(100)		// 1000msec

#define ADC_PIN				0			// adc pin number
#define ADC_ERROR			(-9999)		// adc read error
#define ADC_MAX_VOLT		(4096.f)	// 12bit resolution
#define ADC_REF_VOLT		(3300.f + 20.f)

#define SPI_MAX_VOLT		(3200.f)
#define SPI_REF_VOLT		(3000.f)

#define POINT_X_ZERO	(2.60206)	// the start point, log(400)=2.6020606
#define POINT_X_MAX		(4.000)		// the start point, log(10000)=4.0
#define	VOLT_V_ZERO		(0.3245)	// refer to datasheet, 400ppm(V)
#define VOLT_V_MAX		(0.2645)	// refer to datasheet, 10000ppm(V)
#define VOLT_V_REACT	(VOLT_V_ZERO - VOLT_V_MAX)

static const char* RES_CAPABILITY_AIRQUALITYSENSOR = "/capability/airQualitySensor/main/0";
static const char* PROP_AIRQUALITY = "airQuality";

static float mg811_co2_slope;	// refer to datasheet, mg811 co2 slope value
static sensor_mg811_t mg_sensor;

co2_sensor_data_t	co2_sensor;
int thread_done = 0;
extern int32_t g_co2_sensor_value;
extern bool g_switch_is_on;
extern smartthings_resource_h st_handle;

extern int resource_read_adc_mcp3008(int ch_num, unsigned int *out_value); /* resource_adc_mcp3008.c */
extern int resource_adc_mcp3008_init(void); /* resource_adc_mcp3008.c */

static int _get_sensor_parameter(int index);

/*
 * initial co2 mg811 sensor for ppm caculation
 */
static void _init_co2_mg811_set(float zero_volts, float max_volts)
{
	sensor_mg811_t *sen = &mg_sensor;
	float reaction_volts;

	if (zero_volts == 0.f)
		zero_volts = VOLT_V_ZERO;
	if (max_volts == 0.f)
		max_volts = VOLT_V_MAX;

	sen->zero_point_volts = zero_volts;
	sen->max_point_volts = max_volts;
	reaction_volts = VOLT_V_REACT;
	mg811_co2_slope = reaction_volts / (POINT_X_ZERO - POINT_X_MAX);
#if defined(__DEBUG__)
	_D("CO2Volage zero_volts: %.f mV, max_volts: %.f mV, reaction %.f mV",
			sen->zero_point_volts * 1000., sen->max_point_volts * 1000., reaction_volts * 1000.);
	_D("mg811_ppm: %.3f V, %.3f", POINT_X_ZERO, mg811_co2_slope);
#endif
}

/*
 * get ppm value in co2 mg811 sensor
 */
static int _get_co2_mg811_ppm(float volts)
{
	static int debug = 0;
	sensor_mg811_t *sen = &mg_sensor;
	float log_volts = 0;

	volts = volts / DC_GAIN;
	if (!(volts <= sen->zero_point_volts && volts >= sen->max_point_volts)) {
		if ((debug++ % 10) == 0) {
			_D("wrong input %.f mV, voltage range %.f ~ %.f (400 ppm ~ 10000 ppm)",
				volts * 1000., sen->zero_point_volts * 1000., sen->max_point_volts * 1000.);
		}
		if (volts < sen->max_point_volts) {
			return 10000;
		}
		return -1;
	}
	log_volts = (volts - sen->zero_point_volts) / mg811_co2_slope;

	return pow(10, log_volts + POINT_X_ZERO);
}

/*
 * get adc to co2 sensor analog value
 */
static short resource_get_co2_sensor_analog(int ch_num)
{
	int ret = 0;
	unsigned int out_value = 0;
	float sensor_value = 0;

	ret = resource_read_adc_mcp3008(ch_num, &out_value);
	if (ret < 0)
		return ret;
	sensor_value = ((float)out_value * 4.) * (SPI_REF_VOLT / SPI_MAX_VOLT);	// 10bit -> 12bit, calibration adc volt

	return (short)sensor_value;
}

/*
 * update to average co2 ppm value
 */
int resource_update_co2_sensor_value(void)
{
	co2_sensor_data_t *sensorp = &co2_sensor;
	float sensor_value = 0;
	float sensor_fvalue = 0;
	int n, percentage;
	int size = 0;
	static int debug = 0;

	MUTEX_LOCK;
	if (sensorp->bsize < ADC_MAX_SIZE)
		size = sensorp->bsize;
	else
		size = ADC_MAX_SIZE;
	MUTEX_UNLOCK;

	for (n = 0; n < size; n++) {
		MUTEX_LOCK;
		sensor_value += (float)sensorp->sensor_value[n];
		MUTEX_UNLOCK;
	}
	sensor_value = sensor_value / (float)size;

	sensor_fvalue = (sensor_value * ADC_REF_VOLT) / ADC_MAX_VOLT;
	percentage = _get_co2_mg811_ppm(sensor_fvalue / 1000.f);

	if (percentage < 0 || percentage >= 10000) {
		if ((debug++ % 5) == 0)
			_D("sensor: %.f, volt: %.2f mV, CO2: %d ppm", sensor_value, sensor_fvalue, percentage);
	}

	return percentage;
}

/*
 * initial adc funtion
 */
void resource_init_co2_sensor(void)
{
	float zero_volts;	// measurement mutimeter voltage (mV)

	zero_volts = (float)_get_sensor_parameter(0) / 1000.0;
	if (zero_volts == 0.0)
		zero_volts = DEFAULT_ZERO_VOLTS;

	_init_co2_mg811_set(zero_volts/DC_GAIN, (zero_volts - DEFAULT_RANGE_VOLTS)/DC_GAIN);
}

/*
 * get adc parameters
 * 0: calibration min voltage
 * 1: calibration max voltage (no used)
 * 2: notification loop count (default 100 is 1000msec delay)
 */
int _get_sensor_parameter(int index)
{
	FILE *fp;
	char buffer[16];
	int volts[3];

	char path[MAX_PATH_LEN];
	char *app_data_path = NULL;

	app_data_path = app_get_data_path();
	sprintf(path, "%s%s", app_data_path, CO2_DATA);

	if((fp = fopen(path, "r")) == NULL) {
		_E("Error: [%s] can't open adc data file", path);
		if (index == 2)
			return 0;
		return (int)(DEFAULT_ZERO_VOLTS * 1000);
	}
	fgets(buffer, 16, fp);
	fclose(fp);
	sscanf(buffer, "%d %d %d", &volts[0], &volts[1], &volts[2]);
	_D("get parameter: %s, zero: %d, max: %d, count: %d", buffer, volts[0], volts[1], volts[2]);

	free(app_data_path);

	return volts[index];
}

/*
 * set adc parameters
 */
void resource_set_sensor_parameter(int zero_volts)
{
	FILE *fp;
	char buffer[16];
	char path[MAX_PATH_LEN];
	char *app_data_path = NULL;

	app_data_path = app_get_data_path();
	sprintf(path, "%s%s", app_data_path, CO2_DATA);

	if((fp = fopen(path, "w+")) == NULL) {
		_E("ERROR: can't fopen file: %s", CO2_DATA);
		return;
	}
	memset(buffer, 0, sizeof(buffer));
	sprintf(buffer, "%d %d %d", zero_volts, zero_volts - (int)(DEFAULT_RANGE_VOLTS * 1000), DEFAULT_NOTIFY_TIME);
	fputs(buffer, fp);
	fclose(fp);
	free(app_data_path);
}

/*
 * get co2 sensor percentage (ppm)
 */
int resource_get_co2_sensor_parameter(void)
{
	int param = _get_sensor_parameter(0);

	return param;
}

/*
 * notify sensor value to cloud
 */
static int notify_sensor_value(void)
{
	int error = SMARTTHINGS_RESOURCE_ERROR_NONE;

	smartthings_payload_h resp_payload = NULL;

	// send notification to cloud server
	error = smartthings_payload_create(&resp_payload);
	if (error != SMARTTHINGS_RESOURCE_ERROR_NONE || !resp_payload) {
		_E("smartthings_payload_create() failed, [%d]", error);
		return error;
	}

	error = smartthings_payload_set_int(resp_payload, PROP_AIRQUALITY, g_co2_sensor_value);
	if (error != SMARTTHINGS_RESOURCE_ERROR_NONE) {
		_E("smartthings_payload_set_int() failed, [%d]", error);
		smartthings_payload_destroy(resp_payload);
		return error;
	}

	error = smartthings_resource_notify(st_handle, RES_CAPABILITY_AIRQUALITYSENSOR, resp_payload);
	if (error != SMARTTHINGS_RESOURCE_ERROR_NONE) {
		_E("smartthings_resource_notify() failed, [%d]", error);
		smartthings_payload_destroy(resp_payload);
		return error;
	}

	if (smartthings_payload_destroy(resp_payload)) {
		_E("smartthings_payload_destroy() failed");
		return error;
	}

	return error;
}

/*
 * main thread function to get sensor data
 */
void *thread_sensor_main(void *arg)
{
	int ret = 0;
	int pin = ADC_PIN;
	int err_count = 0;
	short sensor_value;
	co2_sensor_data_t *sensorp = &co2_sensor;

	_D("%s starting...\n", __func__);

	memset((void *)sensorp, 0, sizeof(co2_sensor_data_t));
	resource_init_co2_sensor();

	ret = resource_adc_mcp3008_init();
	_D("resource_adc_mcp3008_init ret: %d", ret);

	while (true)
	{
		if (thread_done) break;

		sensor_value = resource_get_co2_sensor_analog(pin);
		if (sensor_value >= 0)
		{
			MUTEX_LOCK;
			sensorp->sensor_value[sensorp->index++] = sensor_value;
			if (sensorp->index >= ADC_MAX_SIZE)
				sensorp->index = 0;
			if (sensorp->bsize < ADC_MAX_SIZE)
				sensorp->bsize++;
			sensorp->count++;
			MUTEX_UNLOCK;
			err_count = 0;
			usleep(10);				// 10usec
		}
		else
		{
			if (++err_count >= 100)
			{
				memset((void *)sensorp, 0, sizeof(co2_sensor_data_t));
			}
			usleep(10 * 1000);		// 10msec
		}
	}
	_D("%s exiting...\n", __func__);
	pthread_exit((void *) 0);
}

void *thread_sensor_notify(void *arg)
			{
	int count = 0;
	int nloop = 0;

	count = _get_sensor_parameter(2);
	if (count < 10)
		count = DEFAULT_NOTIFY_TIME;
	while (true) {
		if (thread_done) break;

		if (nloop++ >= count) {
			nloop = 0;
			// notify sensor value to server
			co2_sensor_data_t *sensorp = &co2_sensor;

			if (g_co2_sensor_value > 0 && g_co2_sensor_value < 10000)
				_D("CO2 value: %d, count: %d", g_co2_sensor_value, sensorp->count);

			MUTEX_LOCK;
			sensorp->count = 0;
			MUTEX_UNLOCK;

			if (g_switch_is_on) {
				notify_sensor_value();
			}
		}
		usleep(10 * 1000);		// 10msec
	}
	_D("%s exiting...\n", __func__);
	pthread_exit((void *) 0);
}

