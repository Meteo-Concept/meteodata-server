#include <string>

#include <syslog.h>

#include "sentek_probe_116_parser.h"
#include "../vantagepro2_message.h"
#include "../abstract_weatherlink_api_message.h"

namespace meteodata::wlv2structures
{

void SentekProbe116Parser::parse(AbstractWeatherlinkApiMessage::DataPoint& obs, const pt::ptree& data)
{
	AbstractParser::parse(obs, data);
	obs.soilMoisture10cm = data.get<float>("moist_soil_last_1", AbstractWeatherlinkApiMessage::INVALID_FLOAT);
	obs.soilMoisture20cm = data.get<float>("moist_soil_last_2", AbstractWeatherlinkApiMessage::INVALID_FLOAT);
	obs.soilMoisture30cm = data.get<float>("moist_soil_last_3", AbstractWeatherlinkApiMessage::INVALID_FLOAT);
	obs.soilMoisture40cm = data.get<float>("moist_soil_last_4", AbstractWeatherlinkApiMessage::INVALID_FLOAT);
	obs.soilMoisture50cm = data.get<float>("moist_soil_last_5", AbstractWeatherlinkApiMessage::INVALID_FLOAT);
	obs.soilMoisture60cm = data.get<float>("moist_soil_last_6", AbstractWeatherlinkApiMessage::INVALID_FLOAT);
	obs.soilTemp10cm = data.get<float>("temp_last_1", AbstractWeatherlinkApiMessage::INVALID_FLOAT);
	obs.soilTemp20cm = data.get<float>("temp_last_2", AbstractWeatherlinkApiMessage::INVALID_FLOAT);
	obs.soilTemp30cm = data.get<float>("temp_last_3", AbstractWeatherlinkApiMessage::INVALID_FLOAT);
	obs.soilTemp40cm = data.get<float>("temp_last_4", AbstractWeatherlinkApiMessage::INVALID_FLOAT);
	obs.soilTemp50cm = data.get<float>("temp_last_5", AbstractWeatherlinkApiMessage::INVALID_FLOAT);
	obs.soilTemp60cm = data.get<float>("temp_last_6", AbstractWeatherlinkApiMessage::INVALID_FLOAT);
}

}
