#ifndef BUILD_PARSER_FACTORY_H
#define BUILD_PARSER_FACTORY_H

#include <memory>
#include <map>
#include <string>

#include "../abstract_weatherlink_api_message.h"
#include "abstract_parser.h"


namespace meteodata::wlv2structures
{

class ParserFactory
{
public:
	static std::unique_ptr<AbstractParser> makeParser(int sensorType, std::map<std::string, std::string> variables, AbstractWeatherlinkApiMessage::DataStructureType dataStructureType);

};

}

#endif //BUILD_PARSER_FACTORY_H
