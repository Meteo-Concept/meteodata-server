#include <iostream>
#include <date.h>
#include "../src/dragino/lsn50v2_thermohygrometer_message.h"
#include "../src/cassandra_utils.h"

using namespace meteodata;
using namespace date;
using namespace std::chrono;

int main()
{
	Lsn50v2ThermohygrometerMessage m;
	m.ingest("0cf70000010900010c0197", sys_days{2023_y/January/27});

	CassUuid station;
	cass_uuid_from_string("00000000-0000-0000-0000000000000000", &station);
	auto obs = m.getObservation(station);
	std::cout <<
		obs.day << " | " << obs.time << "\n" <<
		"temperature: " << obs.outsidetemp.second << "°C\n" <<
		"humidity: " << obs.outsidehum.second << "%\n";
}