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

#include <unistd.h>
#include <peripheral_io.h>
#include "log.h"

#define ARTIK_I2C_BUS               1      // ARTIK I2C BUS
#define IMX7D_I2C_BUS               0      // IMX7D I2C BUS
#define SRF02_ADDR                  0x70   // Address of the SRF02 shifted right one bit

static peripheral_i2c_h distance_sensor_h = NULL;
static int i2c_bus = IMX7D_I2C_BUS;
static int i2c_address = SRF02_ADDR;

static int _open_distance_sensor(void)
{
	int ret;

	// open i2c handle for I2C read/write
	if ((ret = peripheral_i2c_open(i2c_bus, i2c_address, &distance_sensor_h)) != 0 ) {
		_E("peripheral_i2c_open() failed!![%d]", ret);
		return ret;
	}

	return ret;
}

int resource_close_distance_sensor(void)
{
	int ret = PERIPHERAL_ERROR_NONE;

	if (distance_sensor_h != NULL) {
		// close i2c handle
		if ((ret = peripheral_i2c_close(distance_sensor_h)) != 0 ) {
			_E("peripheral_i2c_close() failed!![%d]", ret);
		}
		distance_sensor_h = NULL;
	}
	_I("resource_close_distance_sensor...");
	return ret;
}

int resource_read_distance_sensor(uint16_t *out_value)
{
	int ret = PERIPHERAL_ERROR_NONE;
	unsigned char buf[10] = { 0, };		// Buffer for data being read/ written on the i2c bus
	uint16_t range;

	if (distance_sensor_h == NULL) {
		// open i2c handle for I2C read/write
		if ((ret = _open_distance_sensor()) != PERIPHERAL_ERROR_NONE ) {
			_E("peripheral_i2c_open() failed!![%d]", ret);
			return ret;
		}
	}

	buf[0] = 0;		// Commands for performing a ranging
	buf[1] = 0x51;	// Real Ranging Mode - Result in centimeters

	// write mode command to sensor
	if ((ret = peripheral_i2c_write(distance_sensor_h, buf, 2)) != PERIPHERAL_ERROR_NONE) {
		_E("peripheral_i2c_write() failed!![%d]", ret);
		return ret;
	}

	usleep(80000);                       // This sleep waits for the ping to come back
	buf[0] = 0;                          // This is the register we wish to read from

	// Send the register to read from
	if ((ret = peripheral_i2c_write(distance_sensor_h, buf, 1)) != PERIPHERAL_ERROR_NONE) {
		_E("peripheral_i2c_write() failed!![%d]", ret);
		return ret;
	}

	// // Read back data into buf[]
	if ((ret = peripheral_i2c_read(distance_sensor_h, buf, 6)) != PERIPHERAL_ERROR_NONE) {
		_E("peripheral_i2c_read() failed!![%d]", ret);
		return ret;
	} else {
		/*
		 * Location   Read                  Write
		 * 0          Software Revision     Command Register  // If this is 255, then the ping has not yet returned
		 * 1          Unused (reads 0x80)   N/A
		 * 2          Range High Byte       N/A
		 * 3          Range Low Byte        N/A
		 * 4          Autotune              N/A
		 *            Minimum - High Byte
		 * 5          Autotune              N/A
		 *            Minimum - Low Byte
		 */

		range = (buf[2] << 8) + buf[3];      // Calculate range as a word value
	}

#ifdef DEBUG
	_I("Range : %u Cm", range);
#endif

	*out_value = range;

	return ret;
}
