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

#ifndef _RESOURCE_CO2_SENSOR_H_
#define _RESOURCE_CO2_SENSOR_H_

#include <pthread.h>

extern pthread_mutex_t	mutex;

#define UNUSED(x)		(void)(x)	// unused argument
#define MUTEX_LOCK		pthread_mutex_lock(&mutex)
#define MUTEX_UNLOCK	pthread_mutex_unlock(&mutex)
#define ADC_MAX_SIZE	1024		// adc max array size

typedef struct __co2_sensor_data__ {	// common adc variable structure
	int index;
	int count;						// adc read count per one secound
	int bsize;						// sensor_value buffer size (up to ADC_MAX_SIZE)
	short sensor_value[ADC_MAX_SIZE];	// save adc buffer in round-robin method
	float co2_sensor_average;			// co2 average value(ppm) per one secound
} co2_sensor_data_t;

typedef struct sensor_mg811__ {
	float zero_point_volts;		// refer to datasheet, start co2 valtage
	float max_point_volts;		// refer to datasheet, max co2 voltage
} sensor_mg811_t;

#endif /* _RESOURCE_CO2_SENSOR_H_ */
