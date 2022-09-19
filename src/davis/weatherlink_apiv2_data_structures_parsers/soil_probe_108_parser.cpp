#include <string>

#include <syslog.h>

#include "soil_probe_108_parser.h"
#include "../vantagepro2_message.h"
#include "../abstract_weatherlink_api_message.h"

namespace meteodata::wlv2structures
{

SoilProbe108Parser::SoilProbe108Parser(const std::string& soilMoistureField)
{
	if (soilMoistureField.empty()) {
		// noop
	} else if (soilMoistureField == "soil_moisture_1") {
		_setSoilMoisture = [](AbstractWeatherlinkApiMessage::DataPoint& obs, float value) { obs.soilMoisture[0] = int(value); };
	} else if (soilMoistureField == "soil_moisture_2") {
		_setSoilMoisture = [](AbstractWeatherlinkApiMessage::DataPoint& obs, float value) { obs.soilMoisture[1] = int(value); };
	} else if (soilMoistureField == "soil_moisture_3") {
		_setSoilMoisture = [](AbstractWeatherlinkApiMessage::DataPoint& obs, float value) { obs.soilMoisture[2] = int(value); };
	} else if (soilMoistureField == "soil_moisture_4") {
		_setSoilMoisture = [](AbstractWeatherlinkApiMessage::DataPoint& obs, float value) { obs.soilMoisture[3] = int(value); };
	} else {
		std::cerr << "<" << LOG_ERR << ">Invalid field name " << soilMoistureField << ", ignoring" << std::endl;
	}
}

void SoilProbe108Parser::parse(AbstractWeatherlinkApiMessage::DataPoint& obs, const pt::ptree& data)
{
	AbstractParser::parse(obs, data);
	_setSoilMoisture(obs, data.get<float>("soil_moist_last", AbstractWeatherlinkApiMessage::INVALID_FLOAT));
}

}