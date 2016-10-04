#ifndef VANTAGEPRO2MESSAGE_H
#define VANTAGEPRO2MESSAGE_H

#include <cstdint>
#include <array>

#include <boost/asio.hpp>

#include <cassandra.h>

#include "message.h"

using std::uint8_t;
using std::uint16_t;
using std::uint32_t;
using std::uint64_t;

namespace meteodata {

namespace asio = boost::asio;

class VantagePro2Message : public Message
{
public:
	std::array<asio::mutable_buffer, 2>& getBuffer() {
		return _messageBuffer;
	}

	bool isValid() const
	{
		return validateCRC(&_l1, sizeof(Loop1)) &&
		       validateCRC(&_l2, sizeof(Loop2));
	}

	static bool validateCRC(const void* msg, size_t len)
	{
		//byte-wise reading
		const uint8_t* bytes = reinterpret_cast<const uint8_t*>(msg);
		unsigned int crc = 0;
		for (unsigned int i=0 ; i<len ; i++) {
			uint8_t index = (crc >> 8) ^ bytes[i];
			crc = CRC_VALUES[index] ^ ((crc << 8) & 0xFFFF);
		}

		return crc == 0;
	}

	virtual void populateDataPoint(const CassUuid station, CassStatement* const statement) const override;

private:
	struct Loop1
	{
		char header[3];
		uint8_t barTrend;
		uint8_t packetType;
		uint16_t nextRecord;
		uint16_t barometer;
		uint16_t insideTemperature;
		uint8_t insideHumidity;
		uint16_t outsideTemperature;
		uint8_t windSpeed;
		uint8_t tenMinAvgWindSpeed;
		uint16_t windDir;
		uint8_t extraTemp[7];
		uint8_t soilTemp[4];
		uint8_t leafTemp[4];
		uint8_t outsideHumidity;
		uint8_t extraHum[7];
		uint16_t rainRate;
		uint8_t uv;
		uint16_t solarRad;
		uint16_t stormRain;
		unsigned int monthStartDateCurrentStorm : 4;
		unsigned int dayStartDateCurrentStorm   : 5;
		unsigned int yearStartDateCurrentStorm  : 7;
		uint16_t dayRain;
		uint16_t monthRain;
		uint16_t yearRain;
		uint16_t dayET;
		uint16_t monthET;
		uint16_t yearET;
		uint8_t soilMoistures[4];
		uint8_t leafWetnesses[4];
		uint8_t insideAlarm;
		uint8_t rainAlarm;
		uint16_t outsideAlarms;
		uint64_t extraTempHumAlarms;
		uint32_t soilLeafAlarms;
		uint8_t transmitterBatteryStatus;
		uint16_t consoleBatteryVoltage;
		uint8_t forecastIcons;
		uint8_t forecastRuleNumber;
		uint16_t timeOfSunrise;
		uint16_t timeOfSunset;
		char lf;
		char cr;
		uint16_t crcLoop1;
	} __attribute__((packed));

	struct Loop2
	{
		char header[3];
		uint8_t barTrend;
		uint8_t packetType;
		unsigned int : 16;
		uint16_t barometer;
		uint16_t insideTemperature;
		uint8_t insideHumidity;
		uint16_t outsideTemperature;
		uint8_t windSpeed;
		unsigned int : 8;
		uint16_t windDir;
		uint16_t tenMinAvgWindSpeed;
		uint16_t twoMinAvgWindSpeed;
		uint16_t tenMinWindGust;
		uint16_t windGustDir;
		unsigned int : 16;
		unsigned int : 16;
		uint16_t dewPoint;
		unsigned int : 8;
		uint8_t outsideHumidity;
		unsigned int : 8;
		uint16_t heatIndex;
		uint16_t windChill;
		uint16_t thswIndex;
		uint16_t rainRate;
		uint8_t uv;
		uint16_t solarRad;
		uint16_t stormRain;
		unsigned int monthStartDateCurrentStorm : 4;
		unsigned int dayStartDateCurrentStorm   : 5;
		unsigned int yearStartDateCurrentStorm  : 7;
		uint16_t dayRain;
		uint16_t last15MinRain;
		uint16_t lastHourRain;
		uint16_t dayET;
		uint16_t last24HoursRain;
		uint8_t barReducMethod;
		uint16_t userBarOffset;
		uint16_t barCalibNumber;
		uint16_t barSensorRaw;
		uint16_t absBarPressure;
		uint16_t altimeterSetting;
		unsigned int : 8;
		unsigned int : 8;
		uint8_t next10MinWindSpeedGraphPtr;
		uint8_t next15MinWindSpeedGraphPtr;
		uint8_t nextHourWindSpeedGraphPtr;
		uint8_t nextDayWindSpeedGraphPtr;
		uint8_t nextMinRainGraphPtr;
		uint8_t nextRainStormGraphPtr;
		uint8_t minuteInHourForRainCalculation;
		uint8_t nextMonthRainGraphPtr;
		uint8_t nextYearRainGraphPtr;
		uint8_t nextSeasonRainGraphPtr;
		unsigned int : 16;
		unsigned int : 16;
		unsigned int : 16;
		unsigned int : 16;
		unsigned int : 16;
		unsigned int : 16;
		char lf;
		char cr;
		uint16_t crc;
	} __attribute__((packed));

	static constexpr int CRC_VALUES[] =
	{
		0x0, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
		0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
		0x1231, 0x210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
		0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
		0x2462, 0x3443, 0x420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
		0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
		0x3653, 0x2672, 0x1611, 0x630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
		0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
		0x48c4, 0x58e5, 0x6886, 0x78a7, 0x840, 0x1861, 0x2802, 0x3823,
		0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
		0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0xa50, 0x3a33, 0x2a12,
		0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
		0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0xc60, 0x1c41,
		0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
		0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0xe70,
		0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
		0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
		0x1080, 0xa1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
		0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
		0x2b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
		0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
		0x34e2, 0x24c3, 0x14a0, 0x481, 0x7466, 0x6447, 0x5424, 0x4405,
		0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
		0x26d3, 0x36f2, 0x691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
		0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
		0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x8e1, 0x3882, 0x28a3,
		0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
		0x4a75, 0x5a54, 0x6a37, 0x7a16, 0xaf1, 0x1ad0, 0x2ab3, 0x3a92,
		0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
		0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0xcc1,
		0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
		0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0xed1, 0x1ef0
	};

	Loop1 _l1;
	Loop2 _l2;
	std::array<asio::mutable_buffer, 2> _messageBuffer = {
		{ asio::buffer(&_l1, sizeof(Loop1)),
		  asio::buffer(&_l2, sizeof(Loop2)) }
	};
};

}

#endif /* VANTAGEPRO2MESSAGE_H */
