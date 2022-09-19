#ifndef BUILD_SOIL_PROBE_108_PARSER_H
#define BUILD_SOIL_PROBE_108_PARSER_H

#include <string>
#include <functional>

#include "abstract_parser.h"
#include "../abstract_weatherlink_api_message.h"

namespace meteodata::wlv2structures
{

class SoilProbe108Parser : public AbstractParser
{
private:
	std::function<void(AbstractWeatherlinkApiMessage::DataPoint&, float)> _setSoilMoisture = [](AbstractWeatherlinkApiMessage::DataPoint&, float) {};

public:
	SoilProbe108Parser(const std::string& soilMoistureField);
	void parse(AbstractWeatherlinkApiMessage::DataPoint& obs, const pt::ptree& data) override;
};

}

#endif //BUILD_SOIL_PROBE_108_PARSER_H
