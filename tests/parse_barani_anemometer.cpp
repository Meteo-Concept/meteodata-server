#include <iostream>
#include <chrono>
#include <date.h>
#include <observation.h>
#include "../src/barani/barani_anemometer_message.h"
#include "../src/cassandra_utils.h"

using namespace meteodata;
using namespace date;
using namespace std::chrono;

int main()
{
	BaraniAnemometerMessage m;
	m.ingest("c582a1087050904b3114", sys_days{2022_y/April/29});

	CassUuid station;
	cass_uuid_from_string("04d2d1f7-4bd7-4cb3-806b-e8154a590a7b", &station);
	auto obs = m.getObservation(station);
	std::cout <<
		obs.day << " | " << obs.time << "\n" <<
		"wind speed: " << obs.windspeed.second << "m/s" << "\n" <<
		"wind direction: " << obs.winddir.second << "°" << "\n" <<
		"wind gust speed: " << obs.windgust.second << "m/s" << "\n";
}