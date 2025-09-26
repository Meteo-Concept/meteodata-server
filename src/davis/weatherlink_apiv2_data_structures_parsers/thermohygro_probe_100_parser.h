#ifndef BUILD_THERMOHYGRO_PROBE_100_PARSER_H
#define BUILD_THERMOHYGRO_PROBE_100_PARSER_H

#include <string>
#include <functional>

#include "abstract_parser.h"
#include "../abstract_weatherlink_api_message.h"

namespace meteodata::wlv2structures
{

class ThermohygroProbe100Parser : public AbstractParser
{
private:
	std::function<void(AbstractWeatherlinkApiMessage::DataPoint&, float, const pt::ptree*)> _setTemp = [](AbstractWeatherlinkApiMessage::DataPoint&, float, const pt::ptree*) {};
	std::function<void(AbstractWeatherlinkApiMessage::DataPoint&, int)> _setHum = [](AbstractWeatherlinkApiMessage::DataPoint&, int) {};

public:
	ThermohygroProbe100Parser(const std::string& temperatureField, const std::string& humField);
	void parse(AbstractWeatherlinkApiMessage::DataPoint& obs, const pt::ptree& data) override;
};

}

#endif //BUILD_THERMOHYGRO_PROBE_100_PARSER_H
