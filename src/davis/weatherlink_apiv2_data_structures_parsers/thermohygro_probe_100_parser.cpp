#include <string>

#include <syslog.h>

#include "thermohygro_probe_100_parser.h"
#include "../vantagepro2_message.h"
#include "../abstract_weatherlink_api_message.h"

namespace meteodata::wlv2structures
{

ThermohygroProbe100Parser::ThermohygroProbe100Parser(const std::string& temperatureField, const std::string& humidityField)
{
	if (temperatureField.empty()) {
		// noop
	} else if (temperatureField == "temperature") {
		_setTemp = [](AbstractWeatherlinkApiMessage::DataPoint& obs, float value) { obs.temperature = value; };
	} else if (temperatureField == "extra_temperature_1") {
		_setTemp = [](AbstractWeatherlinkApiMessage::DataPoint& obs, float value) { obs.extraTemperature[0] = value; };
	} else if (temperatureField == "extra_temperature_2") {
		_setTemp = [](AbstractWeatherlinkApiMessage::DataPoint& obs, float value) { obs.extraTemperature[1] = value; };
	} else if (temperatureField == "extra_temperature_3") {
		_setTemp = [](AbstractWeatherlinkApiMessage::DataPoint& obs, float value) { obs.extraTemperature[2] = value; };
	} else if (temperatureField == "leaf_temperature_1") {
		_setTemp = [](AbstractWeatherlinkApiMessage::DataPoint& obs, float value) { obs.leafTemperature[0] = value; };
	} else if (temperatureField == "leaf_temperature_2") {
		_setTemp = [](AbstractWeatherlinkApiMessage::DataPoint& obs, float value) { obs.leafTemperature[1] = value; };
	} else if (temperatureField == "soil_temperature_1") {
		_setTemp = [](AbstractWeatherlinkApiMessage::DataPoint& obs, float value) { obs.soilTemperature[0] = value; };
	} else if (temperatureField == "soil_temperature_2") {
		_setTemp = [](AbstractWeatherlinkApiMessage::DataPoint& obs, float value) { obs.soilTemperature[1] = value; };
	} else if (temperatureField == "soil_temperature_3") {
		_setTemp = [](AbstractWeatherlinkApiMessage::DataPoint& obs, float value) { obs.soilTemperature[2] = value; };
	} else if (temperatureField == "soil_temperature_4") {
		_setTemp = [](AbstractWeatherlinkApiMessage::DataPoint& obs, float value) { obs.soilTemperature[3] = value; };
	} else {
		std::cerr << "<" << LOG_ERR << ">Invalid field name " << temperatureField << ", ignoring" << std::endl;
	}

	if (humidityField.empty()) {
		// noop
	} else if (humidityField == "humidity") {
		_setHum = [](AbstractWeatherlinkApiMessage::DataPoint& obs, int value) { obs.humidity = value; };
	} else if (humidityField == "extra_humidity_1") {
		_setHum = [](AbstractWeatherlinkApiMessage::DataPoint& obs, int value) { obs.extraHumidity[0] = value; };
	} else if (humidityField == "extra_humidity_2") {
		_setHum = [](AbstractWeatherlinkApiMessage::DataPoint& obs, int value) { obs.extraHumidity[1] = value; };
	}
}

void ThermohygroProbe100Parser::parse(AbstractWeatherlinkApiMessage::DataPoint& obs, const pt::ptree& data)
{
	AbstractParser::parse(obs, data);
	_setTemp(obs, from_Farenheit_to_Celsius(data.get<float>("temp_last", AbstractWeatherlinkApiMessage::INVALID_FLOAT)));
	_setHum(obs, data.get<int>("hum_last", AbstractWeatherlinkApiMessage::INVALID_INT));
}

}