#include <iostream>
#include <chrono>
#include <date.h>
#include <observation.h>
#include "../src/barani/barani_anemometer_message.h"
#include "../src/barani/barani_anemometer_2023_message.h"
#include "../src/cassandra_utils.h"

using namespace meteodata;
using namespace date;
using namespace std::chrono;

int main()
{
	CassUuid station;
	cass_uuid_from_string("00000000-0000-0000-0000000000000000", &station);

	BaraniAnemometerMessage m;
	m.ingest(station, "c582a1087050904b3114", sys_days{2022_y/April/29});

	auto obs = m.getObservation(station);
	std::cout <<
		obs.day << " | " << obs.time << "\n" <<
		"wind speed: " << obs.windspeed.second << "km/h" << "\n" <<
		"wind direction: " << obs.winddir.second << "°" << "\n" <<
		"wind gust speed: " << obs.windgust.second << "km/h" << "\n";

	BaraniAnemometer2023Message m2;
	m2.ingest(station, "068088781c00101d380f5101", sys_days{2023_y/August/10});

	obs = m2.getObservation(station);
	std::cout <<
			  obs.day << " | " << obs.time << "\n" <<
			  "wind speed: " << obs.windspeed.second << "km/h" << "\n" <<
			  "wind direction: " << obs.winddir.second << "°" << "\n" <<
			  "wind gust speed: " << obs.windgust.second << "km/h" << "\n";
}