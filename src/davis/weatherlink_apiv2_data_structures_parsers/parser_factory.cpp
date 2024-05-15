#include "parser_factory.h"

#include <memory>
#include <map>
#include <string>

#include "davis/abstract_weatherlink_api_message.h"
#include "sentek_probe_116_parser.h"
#include "soil_probe_108_parser.h"
#include "thermohygro_probe_100_parser.h"
#include "davis_transmitter_55_parser.h"

namespace meteodata::wlv2structures
{

std::unique_ptr<AbstractParser> ParserFactory::makeParser(int sensorType, std::map<std::string, std::string> variables, AbstractWeatherlinkApiMessage::DataStructureType dataStructureType)
{
	if (sensorType == 100 || sensorType == 105) {
		return std::make_unique<ThermohygroProbe100Parser>(variables["temperature"], variables["humidity"]);
	} else if (sensorType == 108) {
		return std::make_unique<SoilProbe108Parser>(variables["soil_moisture"]);
	} else if (sensorType == 115 || sensorType == 116) {
		return std::make_unique<SentekProbe116Parser>();
	} else if (dataStructureType == AbstractWeatherlinkApiMessage::DataStructureType::WEATHERLINK_CONSOLE_ISS_CURRENT_READING ||
		   dataStructureType == AbstractWeatherlinkApiMessage::DataStructureType::WEATHERLINK_CONSOLE_ISS_ARCHIVE_RECORD  ||
		   sensorType == 55) {
		return std::make_unique<DavisTransmitter55Parser>(std::move(variables), dataStructureType);
	} else {
		return nullptr;
	}
}

}
