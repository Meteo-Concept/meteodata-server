#include "parser.h"
#include "synop_message.h"

#include <vector>
#include <string>
#include <iterator>
#include <iostream>
#include <sstream>
#include <experimental/optional>

#include <date.h>

Parser::Parser()
{
}

bool Parser::parseSection0(decltype(_groups)::iterator& it)
{
	// ICAO
	// Also present in the message later on
	++it;
	if (it == _groups.end())
		return false;

	// Timestamp
	std::istringstream timestamp{*it};
	timestamp >> date::parse("%Y,%m,%d,%H,%M", _message._observationTime);
	++it;
	if (it == _groups.end())
		return false;

	// Station type
	if (*it != "AAXX") {
		return false;
	}
	++it;
	if (it == _groups.end())
		return false;

	// Date
	//no-op

	// Hour
	//no-op

	// i_w
	switch (it->at(4)) {
		case '0':
			_message._withAnemometer = false;
			_message._windSpeedUnit = SynopMessage::WindSpeedUnit::METERS_PER_SECOND;
			break;
		case '1':
			_message._withAnemometer = true;
			_message._windSpeedUnit = SynopMessage::WindSpeedUnit::METERS_PER_SECOND;
			break;
		case '3':
			_message._withAnemometer = false;
			_message._windSpeedUnit = SynopMessage::WindSpeedUnit::KNOTS;
			break;
		case '4':
			_message._withAnemometer = true;
			_message._windSpeedUnit = SynopMessage::WindSpeedUnit::KNOTS;
			break;
	}
	++it;
	if (it == _groups.end())
		return false;

	// ICAO
	_message._stationIcao = *it;
	++it;

	return true;
}

std::experimental::optional<int>
parseInt(const std::string& s, std::string::size_type pos, std::string::size_type endpos = std::string::npos)
{
	int n = 0;
	for (auto i = pos ; i < s.length() && i <= endpos ; i++) {
		if (s[i] == '/')
			return std::experimental::optional<int>();
		n = (n * 10) + (s[i] - '0');
	}
	return std::experimental::optional<int>(n);
}

std::experimental::optional<int>
parseSInt(const std::string& s, std::string::size_type pos, std::string::size_type endpos = std::string::npos)
{
	int n = 0;
	if (s[pos] == '/')
		return std::experimental::optional<int>();
	for (auto i = pos + 1 ; i < s.length() && i <= endpos ; i++) {
		if (s[i] == '/')
			return std::experimental::optional<int>();
		n = (n * 10) + (s[i] - '0');
	}
	if (s[pos] == '1')
		n *= -1;
	return std::experimental::optional<int>(n);
}

std::experimental::optional<PrecipitationAmount> parseRain(const std::string& s)
{
	std::experimental::optional<int> rrr = parseInt(s, 1, 3);
	if (rrr) {
		PrecipitationAmount pr;
		if (rrr > 990) {
			pr._amount = (*rrr - 990) / 10.;
			pr._trace = false;
		} else if (rrr == 990) {
			pr._amount = 0;
			pr._trace = true;
		} else {
			pr._amount = *rrr;
			pr._trace = false;
		}
		pr._duration =
			s[4] == '1' ? 6 :
			s[4] == '2' ? 12 :
			s[4] == '3' ? 18 :
			s[4] == '4' ? 24 :
			s[4] == '5' ? 1 :
			s[4] == '6' ? 2 :
			s[4] == '7' ? 3 :
			s[4] == '8' ? 9 :
			s[4] == '9' ? 15 :
			              0;
		return pr;
	} else {
		return std::experimental::optional<PrecipitationAmount>();
	}
}

bool Parser::parseSection1(decltype(_groups)::iterator& it)
{
	// No section 1, that's not normal, going on anyway
	if (it->length() != 5)
		return false;

	// ## First group, mandatory
	// Rain indicator, i_R
	// no-op, not relevant

	// Station operation, i_x
	char ix = it->at(1);
	_message._manned = ix < 4;
	switch (ix) {
		case '1':
		case '4':
			_message._phenomena = SynopMessage::PhenomenaObservationsAvailable::ADVANCED_OBSERVATIONS;
			break;
		case '2':
		case '5':
			_message._phenomena = SynopMessage::PhenomenaObservationsAvailable::NO_PHENOMENON;
			break;
		case '3':
		case '6':
			_message._phenomena = SynopMessage::PhenomenaObservationsAvailable::NOT_OBSERVED;
			break;
		case '7':
			_message._phenomena = SynopMessage::PhenomenaObservationsAvailable::BASIC_OBSERVATIONS;
			break;
	}

	// Base of lowest cloud, h
	switch (it->at(2)) {
		case '0':
			_message._hBaseLowestCloud = Range<int>{0, 50, true, false};
			break;
		case '1':
			_message._hBaseLowestCloud = Range<int>{50, 100, true, false};
			break;
		case '2':
			_message._hBaseLowestCloud = Range<int>{100, 200, true, false};
			break;
		case '3':
			_message._hBaseLowestCloud = Range<int>{200, 300, true, false};
			break;
		case '4':
			_message._hBaseLowestCloud = Range<int>{300, 600, true, false};
			break;
		case '5':
			_message._hBaseLowestCloud = Range<int>{600, 1000, true, false};
			break;
		case '6':
			_message._hBaseLowestCloud = Range<int>{1000, 1500, true, false};
			break;
		case '7':
			_message._hBaseLowestCloud = Range<int>{1500, 2000, true, false};
			break;
		case '8':
			_message._hBaseLowestCloud = Range<int>{2000, 2500, true, false};
			break;
		case '9':
			_message._hBaseLowestCloud = Range<int>{2500, Range<int>::unbound(), true, false};
			break;
	}

	// Horizontal visibility, VV
	if (it->at(3) != '/' && it->at(4) != '/') {
		int vv = (*it)[3] * 10 + (*it)[4];
		if (vv == 0)
			_message._horizVisibility = Range<float>{0., 0.1, false, false};
		else if (vv >= 1 && vv <= 50)
			_message._horizVisibility = Range<float>{vv / 10., (vv + 1) / 10., true, false};
		else if (vv > 50 && vv <= 80)
			_message._horizVisibility = Range<float>{vv - 50., (vv + 1) - 50., true, false};
		else if (vv > 80 && vv <= 87)
			_message._horizVisibility = Range<float>{30. + (vv - 50) * 5., 30. + (vv + 1 - 50) * 5., true, false};
		else if (vv == 88)
			_message._horizVisibility = Range<float>{70., 70., true, true};
		else if (vv == 89)
			_message._horizVisibility = Range<float>{70., Range<float>::unbound(), false, false};
		else if (vv == 90)
			_message._horizVisibility = Range<float>{0., 0.05, false, false};
		else if (vv == 91)
			_message._horizVisibility = Range<float>{0.05, 0.2, true, false};
		else if (vv == 92)
			_message._horizVisibility = Range<float>{0.2, 0.5, true, false};
		else if (vv == 93)
			_message._horizVisibility = Range<float>{0.5, 1., true, false};
		else if (vv == 94)
			_message._horizVisibility = Range<float>{1., 2., true, false};
		else if (vv == 95)
			_message._horizVisibility = Range<float>{2., 4., true, false};
		else if (vv == 96)
			_message._horizVisibility = Range<float>{4., 10., true, false};
		else if (vv == 97)
			_message._horizVisibility = Range<float>{10., 20., true, false};
		else if (vv == 98)
			_message._horizVisibility = Range<float>{20., 50., true, false};
		else if (vv == 99)
			_message._horizVisibility = Range<float>{50., Range<float>::unbound(), true, false};
	}
	it++;

	// # Second group, mandatory
	// Nebulosity, N
	_message._cloudCover = static_cast<Nebulosity>(it->at(0));

	// Dominant direction of the wind
	std::experimental::optional<int> n = parseInt(*it, 1, 2);
	if (n)
		_message._meanWindDirection = (*n) * 10;

	// Mean wind speed
	std::experimental::optional<int> ff = parseInt(*it, 3);
	if (ff) {
		//special case first
		if (*ff == 99) {
			it++;
			if (it->at(0) != '0' || it->at(1) != '0')
				return false;
			if (it->at(2) != '/' && it->at(3) != '/' && it->at(4) != '/')
				_message._meanWindSpeed = parseInt(*it, 2);
		} else {
			_message._meanWindSpeed = ff;
		}
	}
	it++;

	// # Here begins the optional groups
	char indicative = '0';
	while (it != _groups.end() && it->compare(0, 3, "222") != 0 && it->length() != 3) {
		auto& s = *it;
		if (s[0] < indicative)
			return false;

		if (s[1] == '/' || s[2] == '/' || s[3] == '/' || s[4] == '/') {
			indicative = s[0];
			++it;
			continue;
		}

		if (s[0] == '1') {
			_message._meanTemperature = parseSInt(s, 1);
		} else if (s[0] == '2') {
			if (s[1] == '9')
				_message._relativeHumidity = parseInt(s, 2);
			else
				_message._dewPoint = parseSInt(s, 1);
		} else if (s[0] == '3') {
			_message._pressureAtStation = parseInt(s, 1);
			if (_message._pressureAtStation && *_message._pressureAtStation < 5000)
				*_message._pressureAtStation = *_message._pressureAtStation + 10000;
		} else if (s[0] == '4') {
			if (s[1] != '0' && s[1] != '9') {
				/* station is unable to give pressure at mean sea level
				 * and give pressure at the station level instead
				 */
				_message._isobaricSurfacePotential = IsobaricSurfacePotential{
						static_cast<IsobaricSurfacePotential::StandardIsobaricSurface>(s[1]), parseInt(s, 2).value()};
			} else {
				_message._pressureAtSeaLevel = parseInt(s, 1);
				if (_message._pressureAtSeaLevel && *_message._pressureAtSeaLevel < 5000)
					*_message._pressureAtSeaLevel = *_message._pressureAtSeaLevel + 10000;
			}
		} else if (s[0] == '5') {
			_message._pressureTendency = PressureTendency{static_cast<PressureTendency::Description>(s[1]),
				parseInt(s, 2).value()};
		} else if (s[0] == '6') {
			std::experimental::optional<PrecipitationAmount> pr = parseRain(s);

			if (pr)
				_message._precipitation.push_back(*pr);
		} else if (s[0] == '7') {
			// don't care
		} else if (s[0] == '8') {
			_message._lowOrMediumCloudCover = static_cast<Nebulosity>(s[1]);
			_message._lowClouds = static_cast<LowClouds>(s[2]);
			_message._mediumClouds = static_cast<MediumClouds>(s[3]);
			_message._highClouds = static_cast<HighClouds>(s[4]);
		} else if (s[0] == '9') {
			// Don't care
		}

		indicative = s[0];
		it++;
	}

	// end of section 1
	return true;
}

bool Parser::parseSection2(decltype(_groups)::iterator& it)
{
	if (it->at(0) != '2' && it->at(1) != '2' && it->at(2) != '2')
		return false;
	++it;

	// Do not parse this section
	while (it != _groups.end() && it->length() != 3)
		++it;
	return true;
}

bool Parser::parseSection3(decltype(_groups)::iterator& it)
{
	if (*it != "333")
		return false;
	++it;

	char indicative = '0';
	while (it != _groups.end() && it->compare(0, 3, "222") != 0 && it->length() != 3) {
		auto& s = *it;
		if (s[0] < indicative)
			return false;

		if (s[0] == '0') {
			// Ignore group 0
		} else if (s[0] == '1') {
			_message._maxTemperature = parseSInt(s, 1);
		} else if (s[0] == '2') {
			_message._minTemperature = parseSInt(s, 1);
		} else if (s[0] == '3') {
			_message._groundStateWithoutSnowOrIce = static_cast<GroundStateWithoutSnowOrIce>(s[1]);
			_message._minSoilTemperature = parseSInt(s, 2);
		} else if (s[0] == '4') {
			_message._groundStateWithSnowOrIce = static_cast<GroundStateWithSnowOrIce>(s[1]);
			std::experimental::optional<int> maybeSnow = parseInt(s, 2);
			if (maybeSnow) {
				SnowDepth d;
				int sss = *maybeSnow;
				if (sss <= 996) {
					d._depth = sss;
					d._cover = SnowDepth::SnowCoverageCondition::COVER_MORE_THAN_5_MM;
				} else if (sss == 997) {
					d._depth = 0;
					d._cover = SnowDepth::SnowCoverageCondition::COVER_LESS_THAN_5_MM;
				} else if (sss == 998) {
					d._depth = 0;
					d._cover = SnowDepth::SnowCoverageCondition::DISCONTINUOUS_COVER;
				} else {
					d._depth = 0;
					d._cover = SnowDepth::SnowCoverageCondition::NOT_MEASURED;
				}
			}
		} else if (s[0] == '5') {
			if (s == "55407") {
				++it;
				if (it == _groups.end())
					break;
				_message._shortWaveRadiationLastHour = parseInt(*it, 1);
			} else if (s == "55408") {
				++it;
				if (it == _groups.end())
					break;
				_message._directSolarRadiationLastHour = parseInt(*it, 1);
			} else if (s == "55507") {
				++it;
				if (it == _groups.end())
					break;
				_message._shortWaveRadiationLast24Hours = parseInt(*it, 1);
			} else if (s == "55508") {
				++it;
				if (it == _groups.end())
					break;
				_message._directSolarRadiationLast24Hours = parseInt(*it, 1);
			} else if (s[1] == '5' && s[2] == '3') {
				std::experimental::optional<int> time = parseInt(s, 3);
				if (time)
					_message._minutesOfSunshineLastHour = *time * 6; // conversion from tenths of hours to minutes
				++it;
				if (it == _groups.end())
					break;
				if (it->at(0) == '0' || it->at(0) == '1')
					_message._netRadiationLastHour = parseSInt(*it, 1);
				else if (it->at(0) == '2')
					_message._globalSolarRadiationLastHour = parseInt(*it, 1);
				else if (it->at(0) == '3')
					_message._diffusedSolarRadiationLastHour = parseInt(*it, 1);
				else if (it->at(0) == '4')
					_message._downwardLongWaveRadiationLastHour = parseInt(*it, 1);
				else if (it->at(0) == '5')
					_message._upwardLongWaveRadiationLastHour = parseInt(*it, 1);
				else if (it->at(0) == '6')
					_message._shortWaveRadiationLastHour = parseInt(*it, 1);
			} else if (s[1] == '5') {
				std::experimental::optional<int> time = parseInt(s, 2);
				if (time)
					_message._minutesOfSunshineLastDay = *time * 6; // conversion from tenths of hours to minutes
				++it;
				if (it == _groups.end())
					break;
				if (it->at(0) == '0' || it->at(0) == '1')
					_message._netRadiationLast24Hours = parseSInt(*it, 0);
				else if (it->at(0) == '2')
					_message._globalSolarRadiationLast24Hours = parseInt(*it, 1);
				else if (it->at(0) == '3')
					_message._diffusedSolarRadiationLast24Hours = parseInt(*it, 1);
				else if (it->at(0) == '4')
					_message._downwardLongWaveRadiationLast24Hours = parseInt(*it, 1);
				else if (it->at(0) == '5')
					_message._upwardLongWaveRadiationLast24Hours = parseInt(*it, 1);
				else if (it->at(0) == '6')
					_message._shortWaveRadiationLast24Hours = parseInt(*it, 1);
			} else if (s[1] == '4' || s[1] == '8' || s[1] == '9') {
				//discard
			} else if (s[1] == '6') {
				_message._lowCloudsDrift = static_cast<Direction>(s[2]);
				_message._mediumCloudsDrift = static_cast<Direction>(s[3]);
				_message._highCloudsDrift = static_cast<Direction>(s[4]);
			} else if (s[1] == '7') {
				_message._clouds.push_back(CloudElevation{static_cast<CloudGenus>(s[2]), static_cast<Direction>(s[3]),
					static_cast<CloudElevation::ElevationAngle>(s[4])});
			} else {
				std::experimental::optional<int> eee = parseInt(s, 1, 3);
				if (eee) {
					_message._evapoMaybeTranspiRation = EvapoMaybeTranspiRation{
						static_cast<EvapoMaybeTranspiRation::Instrumentation>(s[4]),
						*eee
					};
				}
			}
		} else if (s[0] == '6') {
			std::experimental::optional<PrecipitationAmount> pr = parseRain(s);

			if (pr)
				_message._precipitation.push_back(*pr);
		} else if (s[0] == '7') {
			PrecipitationAmount pr;
			std::experimental::optional<int> rrrr = parseInt(s, 1);
			if (rrrr) {
				if (*rrrr <= 9998) {
					pr._amount = *rrrr / 10.;
					pr._trace = false;
				} else {
					pr._amount = 0;
					pr._trace = true;
				}
				pr._duration = 24;
				_message._precipitation.push_back(pr);
			}
		} else if (s[0] == '8') {
			std::experimental::optional<int> hshs = parseInt(s, 3);
			if (hshs) {
				Range<int> height;
				if (*hshs <= 50) {
					height = Range<int>{30 * (*hshs - 1), 30 * (*hshs), false, true};
				} else if (*hshs <= 80) {
					height = Range<int>{300 * (*hshs - 51), 300 * (*hshs - 50), false, true};
				} else if (*hshs <= 88) {
					height = Range<int>{500 * (*hshs - 61), 500 * (*hshs - 60), false, true};
				} else if (*hshs == 89) {
					height = Range<int>{21000, Range<int>::unbound(), false, false};
				} else if (*hshs == 90) {
					height = Range<int>{0, 50, false, false};
				} else if (*hshs == 91) {
					height = Range<int>{50, 100, true, false};
				} else if (*hshs == 92) {
					height = Range<int>{100, 200, true, false};
				} else if (*hshs == 93) {
					height = Range<int>{200, 300, true, false};
				} else if (*hshs == 94) {
					height = Range<int>{300, 600, true, false};
				} else if (*hshs == 95) {
					height = Range<int>{600, 1000, true, false};
				} else if (*hshs == 96) {
					height = Range<int>{1000, 1500, true, false};
				} else if (*hshs == 97) {
					height = Range<int>{1500, 2000, true, false};
				} else if (*hshs == 98) {
					height = Range<int>{2000, 2500, true, false};
				} else if (*hshs == 99) {
					height = Range<int>{2500, Range<int>::unbound(), true, false};
				}
				_message._heightOfBaseOfClouds.push_back(
						CloudObservation{static_cast<CloudGenus>(s[2]), Direction::ALL_DIRECTIONS, height,
								 static_cast<Nebulosity>(s[1])});
			}
		} else if (s[0] == '9') {
			if (s[1] == '1' && s[2] == '0') {
				auto gust = parseInt(s, 3);
				if (gust)
					_message._gustObservations.push_back({*gust, 10});
			} else if (s[1] == '0' && s[2] == '7') {
				auto duration = parseInt(s, 3);
				++it;
				if (it == _groups.end())
					break;
				if (duration && *duration <= 60) {
					if ((*it)[1] == '1' && (*it)[2] == '1') {
						std::experimental::optional<int> gust;
						if ((*it)[3] == '9' && (*it)[4] == '9') {
							++it;
							if (it == _groups.end())
								break;
							if ((*it)[0] == '0' && (*it)[1] == '0')
								gust = parseInt(*it, 2);
						} else {
							gust = parseInt(*it, 3);
						}
						if (gust) {
							_message._gustObservations.push_back({*gust, *duration * 6});
						}
					}
				}
			} else if (s[1] == '0' && (s[2] == '2' || s[2] == '4')) {
				// Current group is a duration and is attached to the next group, not handled here
				++it;
			}
			// XXX: small issue here with group 903 which gives the ending time of the _preceding_ 9.... 
		}

		indicative = s[0];
		it++;
	}

	return true;
}

bool Parser::parseSection4(decltype(_groups)::iterator& it)
{
	if (*it != "444")
		return false;
	++it;

	// Do not parse this section
	while (it->length() != 3 && it != _groups.end())
		++it;
	return true;
}

bool Parser::parseSection5(decltype(_groups)::iterator& it)
{
	if (*it != "555")
		return false;
	++it;

	while (it != _groups.end()) {
		auto& s = *it;

		if (s[0] == '6') {
			std::experimental::optional<PrecipitationAmount> pr = parseRain(s);

			if (pr)
				_message._precipitation.push_back(*pr);
		} else if (s[0] == '9') {
			if (s[1] == '0' && s[2] == '7') {
				auto duration = parseInt(s, 3);
				++it;
				if (it == _groups.end())
					break;
				if (duration && *duration <= 60) {
					if ((*it)[1] == '1' && (*it)[2] == '1') {
						std::experimental::optional<int> gust;
						if ((*it)[3] == '9' && (*it)[4] == '9') {
							++it;
							if (it == _groups.end())
								break;
							if ((*it)[0] == '0' && (*it)[1] == '0')
								gust = parseInt(*it, 2);
						} else {
							gust = parseInt(*it, 3);
						}
						if (gust) {
							_message._gustObservations.push_back({*gust, *duration * 6});
						}
					}
				}
			} else if (s[1] == '0' && (s[2] == '2' || s[2] == '4')) {
				// Current group is a duration and is attached to the next group, not handled here
				++it;
				if (it == _groups.end())
					break;
			}
		}

		++it;
	}

	return true;
}

bool Parser::parse(std::istream& in)
{
	std::string extracted;
	in >> extracted;
	// First group has structure IIIii,YYYY,MM,DD,HH,mm,AAXX
	// IIIii: Identifier (5 characters)
	// YYYY,MM,DD,HH,mm Date and time (16 characters)
	// AAXX: Type of message (4 characters)
	// + 2 commas
	// = 27 characters
	if (extracted.size() != 27)
		return false;

	_groups.emplace_back(extracted.substr(0, 5));
	_groups.emplace_back(extracted.substr(6, 16));
	_groups.emplace_back(extracted.substr(23, 4));

	std::istream_iterator<std::string> begin{in}, end;
	std::copy(begin, end, std::back_inserter(_groups));

	// The last group may be padded with one or more "=" signs, remove those
	std::string& lastGroup = _groups.back();
	if (lastGroup.size() > 5)
		lastGroup.erase(5);

	auto it = _groups.begin();

	// ### Section 0 ### //
	_message._sections[0] = true;
	bool r = parseSection0(it);
	if (!r)
		return r;

	// ### Section 1 ### //
	_message._sections[1] = true;
	r = parseSection1(it);

	// ### Possibly section 2 ### //
	if (it != _groups.end() && (*it)[0] == 2 && (*it)[1] == 2 && (*it)[2] == 2) {
		_message._sections[2] = true;
		r = parseSection2(it);
	}
	if (!r)
		return r;

	// ### Possibly section 3 ### //
	if (it != _groups.end() && *it == "333") {
		_message._sections[3] = true;
		r = parseSection3(it);
	}
	if (!r)
		return r;

	// ### Possibly section 4 ### //
	if (it != _groups.end() && *it == "444") {
		_message._sections[4] = true;
		r = parseSection4(it);
	}
	if (!r)
		return r;

	// ### Possibly section 5 ### //
	if (it != _groups.end() && *it == "555") {
		_message._sections[5] = true;
		r = parseSection5(it);
	}
	if (!r)
		return r;

	return r;
}
