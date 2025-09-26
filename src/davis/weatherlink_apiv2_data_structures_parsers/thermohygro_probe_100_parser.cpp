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
		_setTemp = [](AbstractWeatherlinkApiMessage::DataPoint& obs, float value, const pt::ptree* data) {
			if (!AbstractWeatherlinkApiMessage::isInvalid(value)) {
				obs.temperatureF = value;
				obs.temperature = from_Farenheit_to_Celsius(value);
				float min = data->get<float>("temp_last", AbstractWeatherlinkApiMessage::INVALID_FLOAT);
				if (!AbstractWeatherlinkApiMessage::isInvalid(min)) {
					obs.temperatureMinF = value;
					obs.minTemperature = from_Farenheit_to_Celsius(value);
				}
				float max = data->get<float>("temp_last", AbstractWeatherlinkApiMessage::INVALID_FLOAT);
				if (!AbstractWeatherlinkApiMessage::isInvalid(max)) {
					obs.temperatureMaxF = value;
					obs.maxTemperature = from_Farenheit_to_Celsius(value);
				}
			}
		};
	} else if (temperatureField == "extra_temperature_1") {
		_setTemp = [](AbstractWeatherlinkApiMessage::DataPoint& obs, float value, const pt::ptree*) { obs.extraTemperature[0] = value; };
	} else if (temperatureField == "extra_temperature_2") {
		_setTemp = [](AbstractWeatherlinkApiMessage::DataPoint& obs, float value, const pt::ptree*) { obs.extraTemperature[1] = value; };
	} else if (temperatureField == "extra_temperature_3") {
		_setTemp = [](AbstractWeatherlinkApiMessage::DataPoint& obs, float value, const pt::ptree*) { obs.extraTemperature[2] = value; };
	} else if (temperatureField == "leaf_temperature_1") {
		_setTemp = [](AbstractWeatherlinkApiMessage::DataPoint& obs, float value, const pt::ptree*) { obs.leafTemperature[0] = value; };
	} else if (temperatureField == "leaf_temperature_2") {
		_setTemp = [](AbstractWeatherlinkApiMessage::DataPoint& obs, float value, const pt::ptree*) { obs.leafTemperature[1] = value; };
	} else if (temperatureField == "soil_temperature_1") {
		_setTemp = [](AbstractWeatherlinkApiMessage::DataPoint& obs, float value, const pt::ptree*) { obs.soilTemperature[0] = value; };
	} else if (temperatureField == "soil_temperature_2") {
		_setTemp = [](AbstractWeatherlinkApiMessage::DataPoint& obs, float value, const pt::ptree*) { obs.soilTemperature[1] = value; };
	} else if (temperatureField == "soil_temperature_3") {
		_setTemp = [](AbstractWeatherlinkApiMessage::DataPoint& obs, float value, const pt::ptree*) { obs.soilTemperature[2] = value; };
	} else if (temperatureField == "soil_temperature_4") {
		_setTemp = [](AbstractWeatherlinkApiMessage::DataPoint& obs, float value, const pt::ptree*) { obs.soilTemperature[3] = value; };
	} else {
		std::cerr << "<" << LOG_ERR << ">Invalid field name " << temperatureField << ", ignoring" << std::endl;
	}

	if (humidityField.empty()) {
		// noop
	} else if (humidityField == "humidity") {
		_setHum = [](AbstractWeatherlinkApiMessage::DataPoint& obs, float value) { obs.humidity = int(value); };
	} else if (humidityField == "extra_humidity_1") {
		_setHum = [](AbstractWeatherlinkApiMessage::DataPoint& obs, float value) { obs.extraHumidity[0] = int(value); };
	} else if (humidityField == "extra_humidity_2") {
		_setHum = [](AbstractWeatherlinkApiMessage::DataPoint& obs, float value) { obs.extraHumidity[1] = int(value); };
	}
}

void ThermohygroProbe100Parser::parse(AbstractWeatherlinkApiMessage::DataPoint& obs, const pt::ptree& data)
{
	AbstractParser::parse(obs, data);
	_setTemp(obs, data.get<float>("temp_last", AbstractWeatherlinkApiMessage::INVALID_FLOAT), &data);
	_setHum(obs, data.get<float>("hum_last", AbstractWeatherlinkApiMessage::INVALID_FLOAT));
}

}
