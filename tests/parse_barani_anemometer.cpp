#include <iostream>
#include <chrono>
#include <date.h>
#include <observation.h>
#include "../src/barani/barani_anemometer_message.h"
#include "../src/barani/barani_rain_gauge_message.h"
#include "../src/cassandra_utils.h"

using namespace meteodata;
using namespace date;
using namespace std::chrono;

int main()
{
	BaraniAnemometerMessage m;
	m.ingest("c582a1087050904b3114", sys_days{2022_y/April/29});

	CassUuid station;
	cass_uuid_from_string("00000000-0000-0000-0000000000000000", &station);
	auto obs = m.getObservation(station);
	std::cout <<
		obs.day << " | " << obs.time << "\n" <<
		"wind speed: " << obs.windspeed.second << "m/s" << "\n" <<
		"wind direction: " << obs.winddir.second << "°" << "\n" <<
		"wind gust speed: " << obs.windgust.second << "m/s" << "\n";


	BaraniRainGaugeMessage m2;
	m2.ingest("047807FFC028", sys_days{2022_y/April/29}, 0.2, 0, 0);
	obs = m2.getObservation(station);
	std::cout <<
		obs.day << " | " << obs.time << "\n" <<
		"rainfall: " << obs.rainfall.second << "mm" << "\n" <<
		"rainrate: " << obs.rainrate.second << "mm/h" << "\n";
}