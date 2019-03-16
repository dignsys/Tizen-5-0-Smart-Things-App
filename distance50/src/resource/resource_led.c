/*
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *
 * Contact: Jin Yoon <jinny.yoon@samsung.com>
 *          Geunsun Lee <gs86.lee@samsung.com>
 *          Eunyoung Lee <ey928.lee@samsung.com>
 *          Junkyu Han <junkyu.han@samsung.com>
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

#include <stdlib.h>
#include <unistd.h>
#include <peripheral_io.h>
#include "log.h"

int resource_close_led(peripheral_gpio_h handle)
{
	int ret = PERIPHERAL_ERROR_NONE;

	_I("LED is finishing...");
	ret = peripheral_gpio_close(handle);
	if (ret != PERIPHERAL_ERROR_NONE) {
		_E("peripheral_gpio_close failed");
	}
	return ret;
}

int resource_open_led(int gpio_pin, peripheral_gpio_h *handle)
{
	int ret = PERIPHERAL_ERROR_NONE;

	_I("LED(gpio_pin:%d) is opening...", gpio_pin);
	ret = peripheral_gpio_open(gpio_pin, handle);
	if (ret != PERIPHERAL_ERROR_NONE) {
		_E("peripheral_gpio_open failed, ret=[%d]", ret);
	}

	ret = peripheral_gpio_set_direction(*handle, PERIPHERAL_GPIO_DIRECTION_OUT_INITIALLY_LOW);
	if (ret != PERIPHERAL_ERROR_NONE) {
		_E("peripheral_gpio_set_direction failed, ret=[%d]", ret);
	}

	return ret;
}

int resource_write_led(peripheral_gpio_h handle, int write_value)
{
	int ret = PERIPHERAL_ERROR_NONE;

	if (!handle) {
		_E("peripheral_gpio_open failed, ret=[%d]", ret);
		return -1;
	}

	ret = peripheral_gpio_write(handle, write_value);
	if (ret != PERIPHERAL_ERROR_NONE) {
		_E("peripheral_gpio_write failed, ret=[%d]", ret);
		return -1;
	}

	//_I("LED Value : %s", write_value ? "ON":"OFF");

	return 0;
}
