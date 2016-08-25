/*
 * This file is part of OpenCorsairLink.
 * Copyright (C) 2014  Sean Nelson <audiohacked@gmail.com>

 * OpenCorsairLink is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * any later version.

 * OpenCorsairLink is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with OpenCorsairLink.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "Proto.h"
#include "Link.h"
// #include "Fan.h"

#define ARRAY_SIZE(a) (sizeof(a)/sizeof(a[0]))

struct corsairlink_device_info {
        unsigned short vendor_id;
        unsigned short product_id;
        unsigned short device_id;
        unsigned char name[6];
} corsairlink_devices[] = { 
        {0x1b1c, 0x0c04, 0x3b, "H80i"},
        {0x1b1c, 0x0c04, 0x3c, "H100i"},
        {0x1b1c, 0x0c04, 0x41, "H110i"},
        {0x1b1c, 0x0c0a, 0x00, "H115i"},
        {}  
};

struct corsairlink_command {
        unsigned char command;
        unsigned char cmd_opcode;
        unsigned char send_packet_length;
        unsigned char recv_packet_length;
} corsairlink_commands[] = { 
        {DeviceID,              ReadOneByte,    0x03, 17},
        {FirmwareID,    ReadTwoBytes,   0x03, 17},
        {ProductName,   ReadThreeBytes, 0x04, 17},
        {Status,                ReadOneByte,    0x03, 17},
};

CorsairLink::CorsairLink() {
	handle = NULL;
	CommandId = 0x81;
	max_ms_read_wait = 5000;
}

int CorsairLink::Initialize()
{
	if(handle == NULL)
	{
		if (hid_init())
			return 0;

		// Set up the command buffer.
		//memset(buf,0x00,sizeof(buf));
		//buf[0] = 0x01;
		//buf[1] = 0x81;

		// Open the device using the VID, PID,
		// and optionally the Serial number.
		// open Corsair H80i, H100i, or H110i cooler

		//** figure out how to support multiple devices in software
		int i = 0;
		for (i=0; i<ARRAY_SIZE(corsairlink_devices); i++)
		{
			struct corsairlink_device_info dev = corsairlink_devices[i];
			handle = hid_open(dev.vendor_id, dev.product_id, NULL);
			if (handle)
			{
				hid_set_nonblocking(handle, 1);
				if (this->GetDeviceId() == dev.device_id)
				{
					fprintf(stdout, "Device Found: %s", dev.name);
				}
				break;
			}
			else
			{
				this->Close();
				return 0;
			}
		}
	} else {
		fprintf(stderr, "Cannot initialize twice\n" );
		return 0;
	}
	return 1;
}

void CorsairLink::SendCommand(struct corsairlink_command command)
{
	// memset(buf,0,sizeof(buf));

	// Read Device ID: 0x3b = H80i. 0x3c = H100i. 0x41 = H110i
	buf[0] = command.send_packet_length; // Length
	buf[1] = this->CommandId++; // Command ID
	buf[2] = command.cmd_opcode; // Command Opcode
	buf[3] = command.command; // Command data...
	buf[4] = 0x00;

	int res = hid_write(handle, buf, 17);
	if (res < 0) {
		fprintf(stderr, "Error: Unable to write() %s\n", (char*)hid_error(handle) );
	}

	hid_read_wrapper(handle, buf);
	if (res < 0) {
		fprintf(stderr, "Error: Unable to read() %s\n", (char*)hid_error(handle) );
	}

	// return buf;
}

int CorsairLink::GetDeviceId(void)
{
	memset(buf,0,sizeof(buf));
	this->SendCommand(corsairlink_commands[DeviceID]);
	return buf[2];
}

int CorsairLink::GetFirmwareVersion()
{
	memset(buf,0,sizeof(buf));
	this->SendCommand(corsairlink_commands[FirmwareID]);
	int firmware = buf[3]<<8;
	firmware += buf[2];
	return firmware;
}

int CorsairLink::GetProductName(char *ostring)
{
	memset(buf,0,sizeof(buf));
	this->SendCommand(corsairlink_commands[ProductName]);
	memcpy(ostring, buf + 3, 8);
	return 0;
}

int CorsairLink::GetDeviceStatus()
{
	memset(buf,0,sizeof(buf));
	this->SendCommand(corsairlink_commands[Status]);
	return buf[2];
}

char* CorsairLink::_GetManufacturer()
{
	char *str;
	char wstr[MAX_STR];
	wstr[0] = 0x0000;

	int res = hid_get_manufacturer_string(handle, (wchar_t*)wstr, MAX_STR);
	if (res < 0)
		fprintf(stderr, "Unable to read manufacturer string\n");

	str = wstr;
	return str;
}

char* CorsairLink::_GetProduct()
{
	char* str;
	char wstr[MAX_STR];
	wstr[0] = 0x0000;
	
	int res = hid_get_product_string(handle, (wchar_t*)wstr, MAX_STR);
	if (res < 0)
		fprintf(stderr, "Unable to read product string\n");

	str = wstr;
	return str;
}

int CorsairLink::hid_read_wrapper (hid_device *handle, unsigned char *buf)
{
	// Read requested state. hid_read() has been set to be
	// non-blocking by the call to hid_set_nonblocking() above.
	// This loop demonstrates the non-blocking nature of hid_read().
	int res = 0;
	int sleepTotal = 0;
	while (res == 0 && sleepTotal < this->max_ms_read_wait)
	{
		res = hid_read(handle, buf, sizeof(buf));
		if (res < 0)
			fprintf(stderr, "Unable to read()\n");
		
		this->sleep(100);
		sleepTotal += 100;
	}
	if(sleepTotal == this->max_ms_read_wait)
	{
		res = 0;
	}

#if DEBUG
	int i = 0;
	for (i = 0; i < sizeof(buf); i++)
	{
		fprintf(stdout, "Debug-hid_read_wrapper: %02X\n", buf[i]);
	}
#endif
	return 1;
}

int CorsairLink::hid_wrapper (hid_device *handle, unsigned char *buf, size_t buf_size)
{
	int res = hid_write(handle, buf, buf_size);
	if (res < 0) {
		fprintf(stderr, "Error: Unable to write() %s\n", (char*)hid_error(handle) );
		//return -1;
	}
	res = this->hid_read_wrapper(handle, buf);
	if (res < 0) {
		fprintf(stderr, "Error: Unable to read() %s\n", (char*)hid_error(handle) );
		//return -1;
	}
	return res;	
}

void CorsairLink::sleep(int ms)
{
	#ifdef WIN32
	Sleep(ms);
	#else
	usleep(ms*1000);
	#endif
}

void CorsairLink::Close()
{
	if(handle != NULL)
	{
		hid_close(handle);
		hid_exit();
		handle = NULL;
	}
}

CorsairLink::~CorsairLink()
{
	this->Close();
//	if(fans != NULL) {
//		free(fans);
//	}
}
