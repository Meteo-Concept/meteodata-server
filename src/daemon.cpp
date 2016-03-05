#include <iostream>
#include <unistd.h>
#include <syslog.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>

#include <boost/asio.hpp>

#include <tuple>

#include "dbconnection.h"
#include "station.h"
#include "meteo_server.h"

using namespace meteodata;

static void checkUsage(int argc, char** argv)
{
	if (argc > 1) {
		std::cerr << "Usage: " << argv[0] << std::endl;
		std::exit(1);
	}
}

int main(int /* argc */, char** /* argv */)
{
	openlog("meteodata", LOG_PID, LOG_DAEMON);
	//DbConnection db;
	//auto s = db.getStationById("00000000-0000-0000-0000-222222222222");
	//std::cerr << "Found station " << std::get<0>(s) << " " << std::get<1>(s) << std::endl;

	try {
		boost::asio::io_service ioService;
		MeteoServer server(ioService);
		ioService.run();
	} catch (std::exception& e) {
		std::cerr << e.what() << std::endl;
	}

	closelog();
}

#if 0
int main(int argc, char** argv)
{
	checkUsage(argc,argv);

	int daemonization = daemon(0,0);

	if (daemonization) {
		int errsv = errno;
		perror("Could not start Meteodata");
		return errsv;
	}

	openlog("meteodata", LOG_PID, LOG_DAEMON);
	syslog(LOG_NOTICE, "Meteodata has started succesfully");

	closelog();
}
#endif
