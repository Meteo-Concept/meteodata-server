#ifndef BUILD_PARSER_H
#define BUILD_PARSER_H

#include <chrono>

#include <date/date.h>
#include <boost/property_tree/ptree.hpp>

#include "../abstract_weatherlink_api_message.h"

namespace meteodata::wlv2structures
{

class AbstractParser
{
public:
	virtual void parse(AbstractWeatherlinkApiMessage::DataPoint& obs, const pt::ptree& data)
	{
		obs.time = date::floor<std::chrono::milliseconds>(std::chrono::system_clock::from_time_t(data.get<time_t>("ts")));
	}
	virtual ~AbstractParser() = default;
};

}

#endif //BUILD_PARSER_H
