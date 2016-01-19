#include "vantagepro2connector.h"
#include "message.h"

// assume the CRC is the last two bytes
bool VantagePro2Connector::validateCrc(const Message& msg)
{
	unsigned int crc = 0;
	for (char byte : msg) {
		unsigned int index = (crc >> 8) ^ byte;
		crc = VantagePro2Connector::CRC_VALUES[index] ^((crc << 8) & 0xFFFF);
	}

	return crc == 0;
}
