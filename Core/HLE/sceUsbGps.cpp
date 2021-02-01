// Copyright (c) 2012- PPSSPP Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0 or later versions.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official git repository and contact information can be found at
// https://github.com/hrydgard/ppsspp and http://www.ppsspp.org/.

#include <ctime>

#include "Common/System/System.h"
#include "Common/Serialize/Serializer.h"
#include "Common/Serialize/SerializeFuncs.h"
#include "Core/HLE/HLE.h"
#include "Core/HLE/FunctionWrappers.h"
#include "Core/HLE/sceUsbGps.h"
#include "Core/MemMapHelpers.h"

enum GpsStatus {
	GPS_STATE_OFF = 0,
	GPS_STATE_ACTIVATING1 = 1,
	GPS_STATE_ACTIVATING2 = 2,
	GPS_STATE_ON = 3,
};

GpsStatus gpsStatus = GPS_STATE_OFF;

void __UsbGpsInit() {
	gpsStatus = GPS_STATE_OFF;
}

void __UsbGpsDoState(PointerWrap &p) {
	auto s = p.Section("sceUsbGps", 0, 1);
	if (!s)
		return;

	Do(p, gpsStatus);
}

void __UsbGpsShutdown() {
    gpsStatus = GPS_STATE_OFF;
    System_SendMessage("gps_command", "close");
};

static int sceUsbGpsGetInitDataLocation(u32 addr) {
    return 0;
}

static int sceUsbGpsGetState(u32 stateAddr) {
	if (Memory::IsValidAddress(stateAddr)) {
		Memory::Write_U32(gpsStatus, stateAddr);
	}
	return 0;
}

static int sceUsbGpsOpen() {
	ERROR_LOG(HLE, "UNIMPL sceUsbGpsOpen");
	GPS::init();
	gpsStatus = GPS_STATE_ON;
	System_SendMessage("gps_command", "open");
	return 0;
}

static int sceUsbGpsClose() {
	ERROR_LOG(HLE, "UNIMPL sceUsbGpsClose");
	gpsStatus = GPS_STATE_OFF;
	System_SendMessage("gps_command", "close");
	return 0;
}

static int sceUsbGpsGetData(u32 gpsDataAddr, u32 satDataAddr) {
	if (Memory::IsValidRange(gpsDataAddr, sizeof(GpsData))) {
		Memory::WriteStruct(gpsDataAddr, GPS::getGpsData());
	}
	if (Memory::IsValidRange(satDataAddr, sizeof(SatData))) {
		Memory::WriteStruct(satDataAddr, GPS::getSatData());
	}
	return 0;
}

const HLEFunction sceUsbGps[] =
{
	{0X268F95CA, nullptr,                                 "sceUsbGpsSetInitDataLocation",  '?', "" },
	{0X31F95CDE, nullptr,                                 "sceUsbGpsGetPowerSaveMode",     '?', "" },
	{0X54D26AA4, &WrapI_U<sceUsbGpsGetInitDataLocation>,  "sceUsbGpsGetInitDataLocation",  'i', "x" },
	{0X5881C826, nullptr,                                 "sceUsbGpsGetStaticNavMode",     '?', "" },
	{0X63D1F89D, nullptr,                                 "sceUsbGpsResetInitialPosition", '?', "" },
	{0X69E4AAA8, nullptr,                                 "sceUsbGpsSaveInitData",         '?', "" },
	{0X6EED4811, &WrapI_V<sceUsbGpsClose>,                "sceUsbGpsClose",                'i', "" },
	{0X7C16AC3A, &WrapI_U<sceUsbGpsGetState>,             "sceUsbGpsGetState",             'i', "x" },
	{0X934EC2B2, &WrapI_UU<sceUsbGpsGetData>,             "sceUsbGpsGetData",              'i', "xx" },
	{0X9D8F99E8, nullptr,                                 "sceUsbGpsSetPowerSaveMode",     '?', "" },
	{0X9F267D34, &WrapI_V<sceUsbGpsOpen>,                 "sceUsbGpsOpen",                 'i', "" },
	{0XA259CD67, nullptr,                                 "sceUsbGpsReset",                '?', "" },
	{0XA8ED0BC2, nullptr,                                 "sceUsbGpsSetStaticNavMode",     '?', "" },
};

void Register_sceUsbGps()
{
	RegisterModule("sceUsbGps", ARRAY_SIZE(sceUsbGps), sceUsbGps);
}

GpsData gpsData;
SatData satData;

void GPS::init() {
	time_t currentTime;
	time(&currentTime);
	setGpsTime(&currentTime);

	gpsData.hdop      = 1.0f;
	gpsData.latitude  = 51.510357f;
	gpsData.longitude = -0.116773f;
	gpsData.altitude  = 19.0f;
	gpsData.speed     = 3.0f;
	gpsData.bearing   = 35.0f;

	satData.satellites_in_view = 6;
	for (unsigned char i = 0; i < satData.satellites_in_view; i++) {
		satData.satInfo[i].id = i + 1; // 1 .. 32
		satData.satInfo[i].elevation = i * 10;
		satData.satInfo[i].azimuth = i * 50;
		satData.satInfo[i].snr = 0;
		satData.satInfo[i].good = 1;
	}
}

void GPS::setGpsTime(time_t *time) {
	struct tm *gpsTime;
	gpsTime = gmtime(time);

	gpsData.year   = (short)(gpsTime->tm_year + 1900);
	gpsData.month  = (short)(gpsTime->tm_mon + 1);
	gpsData.date   = (short)gpsTime->tm_mday;
	gpsData.hour   = (short)gpsTime->tm_hour;
	gpsData.minute = (short)gpsTime->tm_min;
	gpsData.second = (short)gpsTime->tm_sec;
}

void GPS::setGpsData(long long gpsTime, float hdop, float latitude, float longitude, float altitude, float speed, float bearing) {
	setGpsTime((time_t*)&gpsTime);

	gpsData.hdop      = hdop;
	gpsData.latitude  = latitude;
	gpsData.longitude = longitude;
	gpsData.altitude  = altitude;
	gpsData.speed     = speed;
	gpsData.bearing   = bearing;
}

void GPS::setSatInfo(short index, unsigned char id, unsigned char elevation, short azimuth, unsigned char snr, unsigned char good) {
	satData.satInfo[index].id = id;
	satData.satInfo[index].elevation = elevation;
	satData.satInfo[index].azimuth = azimuth;
	satData.satInfo[index].snr = snr;
	satData.satInfo[index].good = good;
	satData.satellites_in_view = index + 1;
}

GpsData* GPS::getGpsData() {
	return &gpsData;
}

SatData* GPS::getSatData() {
	return &satData;
}
