#ifndef BUILD_DAVIS_TRANSMITTER_55_PARSER_H
#define BUILD_DAVIS_TRANSMITTER_55_PARSER_H

#include <map>
#include <string>
#include <functional>

#include "abstract_parser.h"
#include "../abstract_weatherlink_api_message.h"

namespace meteodata::wlv2structures
{

class DavisTransmitter55Parser : public AbstractParser
{
private:
	int _dataStructureType = -1;
	std::function<void(AbstractWeatherlinkApiMessage::DataPoint&, float)> _setTemperature = [](AbstractWeatherlinkApiMessage::DataPoint&, float) {};
	std::function<void(AbstractWeatherlinkApiMessage::DataPoint&, int)> _setHumidity = [](AbstractWeatherlinkApiMessage::DataPoint&, int) {};
	std::function<void(AbstractWeatherlinkApiMessage::DataPoint&, float, float, int)> _setWindValues = [](AbstractWeatherlinkApiMessage::DataPoint&, float, float, int) {};
	std::function<void(AbstractWeatherlinkApiMessage::DataPoint&, int)> _setSolarRadiationValues = [](AbstractWeatherlinkApiMessage::DataPoint&, int) {};
	std::function<void(AbstractWeatherlinkApiMessage::DataPoint&, float)> _setUvValues = [](AbstractWeatherlinkApiMessage::DataPoint&, float) {};

public:
	explicit DavisTransmitter55Parser(std::map<std::string, std::string> variables, int dataStructureType);
	void parse(AbstractWeatherlinkApiMessage::DataPoint& obs, const pt::ptree& data) override;
};

}

#endif //BUILD_DAVIS_TRANSMITTER_55_PARSER_H
