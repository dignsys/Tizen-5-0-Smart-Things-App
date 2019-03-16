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

#include <stdint.h>
#include <unistd.h>
#include <peripheral_io.h>
#include "resource/resource_pms7003.h"
#include "log.h"

/*
 * PMS7003 Frame Packet Information
 * UART setting : 9600bps, None parity, 1 stop bit
 * frame length is 32 Bytes, 1 second period data transmition
 *
 * Frame Format : 32 Bytes = START_CHAR1[1] + START_CHAR2[1] + FRAME_LENGTH[2] + DATA[2 x 13] + CHECKSUM[2]
 * Start Byte : start of frame [ 0x42, 0x4D ]
 * Length : number of bytes except Start Byte [Frame length = 2 x 13 + 2(data + check bytes)]
 * Data : 2 x 13 data : standard particle [data1, data2, data3], under atmospheric environment [data4, data5, data6], other [data7 ~ 13]
 * Checksum : Check code = START_CHAR1 + START_CHAR2 + data1 + …….. + data13
 */

/*
 * There are two options for digital output: passive and active.
 * Default mode is active after power up.
 * In this mode sensor would send serial data to the host automatically
 * The active mode is divided into two sub-modes: stable mode and fast mode.
 * If the concentration change is small the sensor would run at stable mode with the real interval of 2.3s.
 * And if the change is big the sensor would be changed to fast mode automatically with the interval of 200~800ms,
 * the higher of the concentration, the shorter of the interval.
 */

//#define DEBUG

#define MAX_TRY_COUNT	10
#define UART_PORT				4	// ARTIK 530 : UART0
#define UART_PORT_SDTA7D		5	// SDTA7D : UART6
#define MAX_FRAME_LEN			32

int incoming_byte = 0;          // for incoming serial data
char frame_buf[MAX_FRAME_LEN];  // for save pms7003 protocol data
int byte_position = 0;          // next byte position in frame_buf
int frame_len = MAX_FRAME_LEN;  // length of frame
bool in_frame = false;          // to check start character
unsigned int calc_checksum = 0; // to save calculated checksum value

// DATA STRUCTURE FOR PMS7003 PROTOCOL
static _pms7003_protocol_t pms7003_protocol;

static bool initialized = false;
static peripheral_uart_h g_uart_h;

extern void set_sensor_value(_pms7003_protocol_t pms7003_protocol);

/*
 * open UART port and set UART handle resource
 * set BAUD rate, byte size, parity bit, stop bit, flow control
 * Appendix I：PMS7003 transport protocol-Active Mode
 * Default baud rate：9600bps Check bit：None Stop bit：1 bit
 */
bool resource_pms7003_init(void)
{
	if (initialized) return true;

	_I("----- resource_pms7003_init -----");
	peripheral_error_e ret = PERIPHERAL_ERROR_NONE;

	// Opens the UART slave device
	ret = peripheral_uart_open(UART_PORT_SDTA7D, &g_uart_h);
	if (ret != PERIPHERAL_ERROR_NONE) {
		_E("UART port [%d] open Failed, ret [%d]", UART_PORT_SDTA7D, ret);
		return false;
	}
	// Sets baud rate of the UART slave device.
	ret = peripheral_uart_set_baud_rate(g_uart_h, PERIPHERAL_UART_BAUD_RATE_9600);	// The number of signal in one second is 9600
	if (ret != PERIPHERAL_ERROR_NONE) {
		_E("uart_set_baud_rate set Failed, ret [%d]", ret);
		return false;
	}
	// Sets byte size of the UART slave device.
	ret = peripheral_uart_set_byte_size(g_uart_h, PERIPHERAL_UART_BYTE_SIZE_8BIT);	// 8 data bits
	if (ret != PERIPHERAL_ERROR_NONE) {
		_E("byte_size set Failed, ret [%d]", ret);
		return false;
	}
	// Sets parity bit of the UART slave device.
	ret = peripheral_uart_set_parity(g_uart_h, PERIPHERAL_UART_PARITY_NONE);	// No parity is used
	if (ret != PERIPHERAL_ERROR_NONE) {
		_E("parity set Failed, ret [%d]", ret);
		return false;
	}
	// Sets stop bits of the UART slave device
	ret = peripheral_uart_set_stop_bits (g_uart_h, PERIPHERAL_UART_STOP_BITS_1BIT);	// One stop bit
	if (ret != PERIPHERAL_ERROR_NONE) {
		_E("stop_bits set Failed, ret [%d]", ret);
		return false;
	}
	// Sets flow control of the UART slave device.
	// No software flow control & No hardware flow control
	ret = peripheral_uart_set_flow_control (g_uart_h, PERIPHERAL_UART_SOFTWARE_FLOW_CONTROL_NONE, PERIPHERAL_UART_HARDWARE_FLOW_CONTROL_NONE);
	if (ret != PERIPHERAL_ERROR_NONE) {
		_E("flow control set Failed, ret [%d]", ret);
		return false;
	}

	initialized = true;
	return true;
}

/*
 * To write data to a slave device
 */
bool resource_write_data(uint8_t *data, uint32_t length)
{
	peripheral_error_e ret = PERIPHERAL_ERROR_NONE;

	// write length byte data to UART
	ret = peripheral_uart_write(g_uart_h, data, length);
	if (ret != PERIPHERAL_ERROR_NONE) {
		_E("UART write failed, ret [%d]", ret);
		return false;
	}

	return true;
}

/*
 * To read data from a slave device
 */
bool resource_read_data(uint8_t *data, uint32_t length, bool blocking_mode)
{
	int try_again = 0;
	peripheral_error_e ret = PERIPHERAL_ERROR_NONE;

	if (g_uart_h == NULL)
		return false;

	while (1) {
		// read length byte from UART
		ret = peripheral_uart_read(g_uart_h, data, length);
		if (ret == PERIPHERAL_ERROR_NONE)
			return true;

		// if data is not ready, try again
		if (ret == PERIPHERAL_ERROR_TRY_AGAIN) {
			// if blocking mode, read again
			if (blocking_mode == true) {
				usleep(100 * 1000);
				_I(".");
				continue;
			} else {
				// if non-blocking mode, retry MAX_TRY_COUNT
				if (try_again >= MAX_TRY_COUNT) {
					_E("No data to receive");
					return false;
				} else {
					try_again++;
				}
			}
		} else {
			// if return value is not (PERIPHERAL_ERROR_NONE or PERIPHERAL_ERROR_TRY_AGAIN)
			// return with false
			_E("UART read failed, , ret [%d]", ret);
			return false;
		}
	}

	return true;
}

/*
 * close UART handle and clear UART resources
 */
void resource_pms7003_fini(void)
{
	_I("----- resource_pms7003_fini -----");
	if(initialized) {
		// Closes the UART slave device
		peripheral_uart_close(g_uart_h);
		initialized = false;
		g_uart_h = NULL;
	}
}

/*
 * read sensor data from PMS7003 and format
 */
bool resource_pms7003_read(void)
{
	uint8_t data;
	bool packet_received = false;
	calc_checksum = 0;

	// clear frame buffer
	memset(frame_buf, 0, MAX_FRAME_LEN);

	// clear pms7003_protocol structure
	memset(&pms7003_protocol, 0, sizeof(_pms7003_protocol_t));

	// set Frame length = 2x13 + 2 (data+check bytes)
	pms7003_protocol.frame_len = MAX_FRAME_LEN - sizeof(pms7003_protocol.checksum);

	if (!initialized) {
		// open UART port and set UART handle resource
		// set BAUD rate, byte size, parity bit, stop bit, flow control
		if (!resource_pms7003_init()) {
			_E("resource initialization failed");
			return false;
		}
	}

	while (!packet_received) {
		 // read data from a slave device, block until data is received
		if (resource_read_data(&data, 1, true) == false) {
			_E("resource_read_data failed");
			return false;
		}

		#ifdef DEBUG
		_I("READ: [0x%02X]", incoming_byte);
		#endif

		if (!in_frame) {
			if (data == 0x42 && byte_position == 0) {
				#ifdef DEBUG
				_I("READ: [0x%02X] ST1", data);
				#endif
				frame_buf[byte_position] = data;            // add start character 1 into buffer
				pms7003_protocol.frame_header[0] = data;
				calc_checksum = data;                       // add data to Checksum
				byte_position++;                            // set to next position
			}
			else if (data == 0x4D && byte_position == 1) {
				#ifdef DEBUG
				_I("READ: [0x%02X] ST2", data);
				#endif
				frame_buf[byte_position] = data;            // add start character 2 into buffer
				pms7003_protocol.frame_header[1] = data;
				calc_checksum += data;                      // add data to Checksum

				// we have valid frame header
				in_frame = true;                                     // received start char 1[0x42] and char 2[0x4d]
				byte_position++;                                     // set to next position
			}
			else {
				// data is not in synced, ignore data
				#ifdef DEBUG
				_I("Frame syncing... [0x%02X]", data);
				#endif
			}
		}
		else {
			#ifdef DEBUG
			_I("READ: [0x%02X]", data);
			#endif
			// save data into frame buffer
			frame_buf[byte_position] = data;
			calc_checksum += data;
			byte_position++;

			unsigned int val = (frame_buf[byte_position - 1] & 0xff) + (frame_buf[byte_position - 2] << 8);
			switch (byte_position) {
			case 4:
				pms7003_protocol.frame_len = val;
				frame_len = val + byte_position;
				#ifdef DEBUG
				_I("frame_len: [%d]", frame_len);
				#endif
				break;
			case 6:
				pms7003_protocol.standard_particle.PM1_0 = val;
				break;
			case 8:
				pms7003_protocol.standard_particle.PM2_5 = val;
				break;
			case 10:
				pms7003_protocol.standard_particle.PM10 = val;
				break;
			case 12:
				pms7003_protocol.atmospheric_env.PM1_0 = val;
				break;
			case 14:
				pms7003_protocol.atmospheric_env.PM2_5 = val;
				break;
			case 16:
				pms7003_protocol.atmospheric_env.PM10 = val;
				break;
			case 32:
				pms7003_protocol.checksum = val;
				calc_checksum -= ((val>>8) + (val&0xFF));
				break;
			default:
				break;
			}

			// check if all data is received
			if (byte_position >= frame_len) {
				#ifdef DEBUG
				_I("READ: byte_position[%d] : frame_len[%d]", byte_position, frame_len);
				#endif
				packet_received = true;
				byte_position = 0;
				in_frame = false;
			}
		}
	}

	// check received checksum and calculated checksum is same or not
	// Checksum : Check code = START_CHAR1 + START_CHAR2 + data1 + …….. + data13
	if (calc_checksum == pms7003_protocol.checksum) {
		// save sensor data and return true
		set_sensor_value(pms7003_protocol);
		return true;
	} else {
		// return false
		_E("Checksum error, calc_checksum[0x%X] != [0x%X] pms7003_protocol.checksum", calc_checksum, pms7003_protocol.checksum);
		return false;
	}
}

#if 0
bool read_serial_data(void)
{
	uint8_t data;
	uint8_t rx_data[MAX_STATUS_LEN];
	int ret = 0;
	bool packet_received = false;
	calc_checksum = 0;

	// clear receive buffer
	memset(rx_data, 0, MAX_STATUS_LEN);
	memset(receive_buf, 0, MAX_STATUS_LEN);

	if (!initialized) {
		// open UART port and set UART handle resource
		// set BAUD rate, byte size, parity bit, stop bit, flow control
		if (!resource_pms7003_init()) {
			_E("resource initialization failed");
			return false;
		}
	}

	while (!packet_received) {
		// read byte from UART
		if (resource_read_data(&data, 1, true) == false) {
			_E("resource_read_data failed\n");
			return false;
		}

		#ifdef DEBUG
		_D("READ: [0x%02X] ", data);
		#endif

		if (!in_frame) {
			if (data == 0x42 && byte_position == 0) {
				frame_buf[byte_position] = data;						// add start code into buffer
				byte_position++;										// set to next position
			} else if (data == 0x4D && byte_position == 1) {
				frame_buf[byte_position] = data;						// add address code into buffer

				// we have valid frame header
				in_frame = true;                                     // received start / address / function code
				byte_position++;                                     // set to next position
				break;
			}
			else {
				in_frame = false;                                     // received start / address / function code
				byte_position = 0;                                     // set to next position
				// data is not in synced, ignore data
				#ifdef DEBUG
				_D("Frame syncing... [0x%02X]\n", data);
				#endif
			}
		}
	}

	// read length data
	ret = peripheral_uart_read(g_uart_h, &data, 1);
	receive_buf[byte_position] = data;
	byte_position++;
	frame_len = data;
	_D("length [%d]\n", frame_len);

	int request_len = frame_len - 3;
	int read_len = 0;
	int sleep_count = 0;
	//clock_gettime(CLOCK_MONOTONIC, &start_time);
	while (!packet_received) {

		// read byte from UART
		read_len = peripheral_uart_read(g_uart_h, rx_data, request_len);
		if (read_len < 0) {
			sleep_count++;
			if (sleep_count >= 1000) {
				_D("sleep_count >>>> \n");
				resource_mc_fini();
				break;
			}
			usleep(100);
			continue;
		}
		if (read_len != request_len) {
			_D("read len = %d : req_len = %d", read_len, request_len);
		}
		if (read_len == 0) {
//			for (int j=0; j<request_len; j++)
//				_D("%02X ", rx_data[j]);
//

			packet_received = true;
			byte_position = 0;
			in_frame = false;

			break;
		}

//		if (resource_read_data(rx_data, request_len, false) == false) {
//			_E("resource_read_data failed\n");
//			return false;
//		}


		// save data into buffer
		for (int i=0; i<read_len; i++) {
			receive_buf[byte_position++] = rx_data[i];
		}
		request_len -= read_len;

		// check if all data is received
		if (byte_position == frame_len) {
			#ifdef DEBUG
			//_D("READ: byte_position[%d] : frame_len[%d]\n", byte_position, frame_len);
			#endif
			packet_received = true;
			receive_buf[byte_position] = '\0';
			byte_position = 0;
			in_frame = false;
		}
	}

	clock_gettime(CLOCK_MONOTONIC, &last_read);

	_I("%s[%d] sleep count : %d%s\n", LOG_GREEN, diff_ms(&last_read, &start_time), sleep_count, LOG_END);

#ifdef DEBUG
	_D("%sRX data : %s", LOG_GREEN, LOG_END);
	dump_buffer(status_frame_buf);
#endif

	rcvd_checksum = receive_buf[frame_len-1];
	calc_checksum = chksum8(&receive_buf[0], frame_len-1);

#if 1
	decode_protocol(3, frame_len, receive_buf);
#else
	// check received checksum and calculated checksum is same or not
	if (calc_checksum == rcvd_checksum) {
		//_D("\nChecksum Good\n");
		decode_protocol(3, frame_len, receive_buf);
	} else {
		_E("\nChecksum error, calc_checksum[0x%02X] != [0x%02X] rcvd_checksum\n", calc_checksum, rcvd_checksum);
		dump_buffer(receive_buf);
		return false;
	}
#endif
	return true;
}
#endif
