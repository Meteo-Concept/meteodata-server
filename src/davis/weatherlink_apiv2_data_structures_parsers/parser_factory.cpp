#include "parser_factory.h"

#include <memory>
#include <map>
#include <string>

#include "sentek_probe_116_parser.h"
#include "soil_probe_108_parser.h"
#include "thermohygro_probe_100_parser.h"

namespace meteodata::wlv2structures
{

std::unique_ptr<AbstractParser> ParserFactory::makeParser(int sensorType, std::map<std::string, std::string> variables)
{
	if (sensorType == 100 || sensorType == 105) {
		return std::make_unique<ThermohygroProbe100Parser>(variables["temperature"], variables["humidity"]);
	} else if (sensorType == 108) {
		return std::make_unique<SoilProbe108Parser>(variables["soil_moisture"]);
	} else if (sensorType == 115 || sensorType == 116) {
		return std::make_unique<SentekProbe116Parser>();
	} else {
		return nullptr;
	}
}

}
