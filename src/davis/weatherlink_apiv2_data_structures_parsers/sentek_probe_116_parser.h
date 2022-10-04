#ifndef BUILD_SENTEK_PROBE_116_PARSER_H
#define BUILD_SENTEK_PROBE_116_PARSER_H

#include <string>
#include <functional>

#include "abstract_parser.h"
#include "../abstract_weatherlink_api_message.h"

namespace meteodata::wlv2structures
{

class SentekProbe116Parser : public AbstractParser
{
public:
	void parse(AbstractWeatherlinkApiMessage::DataPoint& obs, const pt::ptree& data) override;
};

}

#endif //BUILD_SENTEK_PROBE_116_PARSER_H
