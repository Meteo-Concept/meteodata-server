#include <map>
#include <string>

#include "davis_transmitter_55_parser.h"
#include "../vantagepro2_message.h"

namespace meteodata::wlv2structures
{
DavisTransmitter55Parser::DavisTransmitter55Parser(std::map<std::string, std::string> variables)
{
	auto temp = variables.find("temperature");
	if (temp != variables.end()) {
		if (temp->second == "outside_temperature") {
			_setTemperature = [](AbstractWeatherlinkApiMessage::DataPoint& obs, float temp) {
				obs.temperatureF = temp;
				obs.temperature = from_Farenheit_to_Celsius(temp);
			};
		} else if (temp->second == "extra_temperature_1") {
			_setTemperature = [](AbstractWeatherlinkApiMessage::DataPoint& obs, float temp) {
				obs.extraTemperature[0] = temp;
			};
		} else if (temp->second == "extra_temperature_2") {
			_setTemperature = [](AbstractWeatherlinkApiMessage::DataPoint& obs, float temp) {
				obs.extraTemperature[1] = temp;
			};
		} else if (temp->second == "extra_temperature_3") {
			_setTemperature = [](AbstractWeatherlinkApiMessage::DataPoint& obs, float temp) {
				obs.extraTemperature[2] = temp;
			};
		}
	}

	auto hum = variables.find("humidity");
	if (hum != variables.end()) {
		if (hum->second == "outside_humidity") {
			_setHumidity = [](AbstractWeatherlinkApiMessage::DataPoint& obs, int hum) {
				obs.humidity = hum;
			};
		} else if (hum->second == "extra_humidity_1") {
			_setHumidity = [](AbstractWeatherlinkApiMessage::DataPoint& obs, int hum) {
				obs.extraHumidity[0] = hum;
			};
		} else if (hum->second == "extra_humidity_2") {
			_setHumidity = [](AbstractWeatherlinkApiMessage::DataPoint& obs, int hum) {
				obs.extraHumidity[1] = hum;
			};
		}
	}

	auto wind = variables.find("wind");
	if (wind != variables.end()) {
		_setWindValues = [](AbstractWeatherlinkApiMessage::DataPoint& obs, float wind, float gust, int windDir) {
			obs.windSpeed = wind;
			obs.windGustSpeed = gust;
			obs.windDir = windDir;
		};
	}

	auto solar = variables.find("solar");
	if (solar != variables.end()) {
		_setSolarRadiationValues = [](AbstractWeatherlinkApiMessage::DataPoint& obs, int rad) {
			obs.solarRad = rad;
		};
	}

	auto uv = variables.find("uv");
	if (uv != variables.end()) {
		_setUvValues = [](AbstractWeatherlinkApiMessage::DataPoint& obs, float uv) {
			obs.uvIndex = uv;
		};
	}
}

void DavisTransmitter55Parser::parse(AbstractWeatherlinkApiMessage::DataPoint& obs, const pt::ptree& data)
{
	AbstractParser::parse(obs, data);

	float temperature = data.get<float>("temp", AbstractWeatherlinkApiMessage::INVALID_FLOAT);
	if (!AbstractWeatherlinkApiMessage::isInvalid(temperature)) {
		_setTemperature(obs, temperature);
	}

	float humidity = data.get<float>("hum", AbstractWeatherlinkApiMessage::INVALID_FLOAT);
	if (!AbstractWeatherlinkApiMessage::isInvalid(humidity)) {
		_setHumidity(obs, int(humidity));
	}

	float wind = data.get<float>("wind_speed_avg_10_min", AbstractWeatherlinkApiMessage::INVALID_FLOAT);
	float gust = data.get<float>("wind_speed_hi_last_10_min", AbstractWeatherlinkApiMessage::INVALID_FLOAT);
	float dir = data.get<float>("wind_dir_scalar_avg_last_10_min", AbstractWeatherlinkApiMessage::INVALID_FLOAT);
	if (!AbstractWeatherlinkApiMessage::isInvalid(wind) &&
		!AbstractWeatherlinkApiMessage::isInvalid(gust) &&
		!AbstractWeatherlinkApiMessage::isInvalid(dir)) {
		_setWindValues(obs, wind, gust, int(dir));
	}

	float solar = data.get<float>("solar_rad", AbstractWeatherlinkApiMessage::INVALID_FLOAT);
	if (!AbstractWeatherlinkApiMessage::isInvalid(solar)) {
		_setSolarRadiationValues(obs, int(solar));
	}

	float uv = data.get<float>("uv_index", AbstractWeatherlinkApiMessage::INVALID_FLOAT);
	if (!AbstractWeatherlinkApiMessage::isInvalid(uv)) {
		_setUvValues(obs, uv);
	}
}

}
