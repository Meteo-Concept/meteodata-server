#include <iostream>
#include <regex>
#include <experimental/optional>

int main()
{
	bool _valid = false;
	std::string _content = "2019-04-05;10:05;8.3|92|7.1|996.69|0.08|0.2|5.4|146|20.9|7.0|8.3|9.5|6.8|0.0|229|02:35|09:27";
	std::experimental::optional<float> _airTemp;
	std::experimental::optional<float> _dewPoint;
	std::experimental::optional<int> _humidity;
	std::experimental::optional<int> _windDir;
	std::experimental::optional<float> _wind;
	std::experimental::optional<float> _pressure;
	std::experimental::optional<float> _gust;
	std::experimental::optional<float> _rainRate;
	std::experimental::optional<int> _solarRad;
	std::experimental::optional<float> _computedRainfall;
	std::experimental::optional<float> _diffRainfall;

	const std::regex mandatoryPart{
		"^\\d+-\\d+-\\d+;\\d+:\\d+;" // date: already parsed
		"([^\\|]*)\\|" // temperature
		"([^\\|]*)\\|" // humidite
		"([^\\|]*)\\|" // dew point
		"([^\\|]*)\\|" // pressure
		"([^\\|]*)\\|" // pressure variable, should be null
		"([^\\|]*)\\|" // rainfall over 1 hour
		"([^\\|]*)\\|" // wind
		"([^\\|]*)\\|" // wind direction
		"([^\\|]*)\\|" // wind gusts
		"([^\\|]*)\\|" // windchill
		"([^\\|]*)(?:\\||$)" // HEATINDEX
	};

	const std::regex optionalPart{
		"([^\\|]*)\\|" // Tx since midnight
		"([^\\|]*)\\|" // Tn since midnight
		"([^\\|]*)\\|" // rainrate
		"([^\\|]*)\\|" // solar radiation
		"([^\\|]*)\\|" // hour of Tx
		"([^\\|]*)\\|?" // hour of Tn
	};

	std::smatch baseMatch;
	if (std::regex_search(_content, baseMatch, mandatoryPart) && baseMatch.size() == 12) {
		for (auto&& match : baseMatch) {
			std::cerr << "match: " << match.str() << std::endl;
		}
		if (baseMatch[1].length()) {
			try {
				_airTemp = std::stof(baseMatch[1].str());
			} catch (std::exception&) {
			}
		}
		if (baseMatch[2].length()) {
			try {
				_humidity = std::stoi(baseMatch[2].str());
			} catch (std::exception&) {
			}
		}
		if (baseMatch[3].length()) {
			try {
				_dewPoint = std::stof(baseMatch[3].str());
			} catch (std::exception&) {
			}
		}
		if (baseMatch[4].length()) {
			try {
				_pressure = std::stof(baseMatch[4].str());
			} catch (std::exception&) {
			}
		}
		// skip pressure tendency
		if (baseMatch[6].length() && _diffRainfall) {
			try {
				float f = std::stof(baseMatch[6].str()) - *_diffRainfall;
				if (f >= 0 && f < 100)
					_computedRainfall = f;
			} catch (std::exception&) {
			}
		}
		if (baseMatch[7].length()) {
			try {
				_wind = std::stof(baseMatch[7].str());
			} catch (std::exception&) {
			}
		}
		if (baseMatch[8].length()) {
			try {
				_windDir = std::stoi(baseMatch[8].str());
			} catch (std::exception&) {
			}
		}
		if (baseMatch[9].length()) {
			try {
				_gust = std::stof(baseMatch[9].str());
			} catch (std::exception&) {
			}
		}
		// skip heatindex and windchill
		std::cout << "heatindex: " << baseMatch[11].str() << std::endl;

		_valid = true;
	}

	std::smatch supplementaryMatch;
	if (std::regex_search(baseMatch[0].second, _content.cend(), supplementaryMatch, optionalPart) && supplementaryMatch.size() == 7) {
		// skip Tx and Tn
		std::cout << "baseMatch ends at: " << std::string(baseMatch[0].second,_content.cend()) << std::endl;
		std::cout << "tx: " << supplementaryMatch[1].str() << std::endl;
		if (supplementaryMatch[3].length())
			_rainRate = std::stof(supplementaryMatch[3].str());
		if (supplementaryMatch[4].length())
			_solarRad = std::stoi(supplementaryMatch[4].str());
		// skip hours of Tx and Tn
	}

	std::cout << "_airTemp: " << *_airTemp << "\n"
		  << "_dewPoint: " << *_dewPoint << "\n"
		  << "_rainRate: " << *_rainRate << "\n";
}

