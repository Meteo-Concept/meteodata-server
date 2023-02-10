#include <iostream>
#include <date.h>
#include "../src/talkpool/oy1110_thermohygrometer_message.h"
#include "../src/cassandra_utils.h"

using namespace meteodata;
using namespace date;
using namespace std::chrono;

int main()
{
	CassUuid station;
	cass_uuid_from_string("00000000-0000-0000-0000000000000000", &station);

	Oy1110ThermohygrometerMessage m{station};
	m.ingest("3e441d", sys_days{2023_y/January/27});

	auto obs = m.getObservation(station);
	std::cout <<
		obs.day << " | " << obs.time << "\n" <<
		"temperature: " << obs.outsidetemp.second << "°C\n" <<
		"humidity: " << obs.outsidehum.second << "%\n";


	Oy1110ThermohygrometerMessage m2{station};
	m2.ingest("304039", sys_days{2023_y/January/27});
	obs = m2.getObservation(station);
	std::cout <<
		obs.day << " | " << obs.time << "\n" <<
		"temperature: " << obs.outsidetemp.second << "°C\n" <<
		"humidity: " << obs.outsidehum.second << "%\n";
}
