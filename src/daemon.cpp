#include <iostream>
#include <unistd.h>
#include <syslog.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>

#include <boost/asio.hpp>

#include <tuple>

#include "dbconnection.h"
#include "meteo_server.h"

using namespace meteodata;

static void checkUsage(int argc, char** argv)
{
	if (argc > 1) {
		std::cerr << "Usage: " << argv[0] << std::endl;
		std::exit(1);
	}
}
int main(int argc, char** argv)
{
	checkUsage(argc,argv);

	std::string user;
	std::string password;
	std::cout << "Please give the username and password to access the database.\n";
	std::cout << "Username: ";
	std::cin >> user;
	std::cout << "Password: ";
	std::cin >> password;

	int daemonization = daemon(0,0);

	if (daemonization) {
		int errsv = errno;
		perror("Could not start Meteodata");
		return errsv;
	}

	openlog("meteodata", LOG_PID, LOG_DAEMON);
	syslog(LOG_NOTICE, "Meteodata has started succesfully");

	try {
		boost::asio::io_service ioService;
		MeteoServer server(ioService, user, password);
		ioService.run();
	} catch (std::exception& e) {
		syslog(LOG_ERR, "%s", e.what());
	}

	closelog();
}
