#include <iostream>
#include <fstream>
#include <unistd.h>
#include <syslog.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>

#include <boost/asio.hpp>
#include <boost/program_options.hpp>

#include <tuple>

#include "config.h"
#include "dbconnection.h"
#include "meteo_server.h"

#define DEFAULT_CONFIG_FILE "/etc/meteodata/db_credentials"

using namespace meteodata;
namespace po = boost::program_options;

int main(int argc, char** argv)
{
	std::string user;
	std::string password;

	po::options_description config("Configuration");
	config.add_options()
		("user,u", po::value<std::string>(&user), "database username")
		("password,p", po::value<std::string>(&password), "database password")
	;

	po::options_description desc("Allowed options");
	desc.add_options()
		("help", "display the help message and exit")
		("version", "display the version of Meteodata and exit")
		("config-file", po::value<std::string>(), "alternative configuration file")
	;
	desc.add(config);

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	std::string configFileName = vm.count("config-file") ?
		vm["config-file"].as<std::string>() :
		DEFAULT_CONFIG_FILE;
	std::ifstream configFile(DEFAULT_CONFIG_FILE);
	if (configFile) {
		po::store(po::parse_config_file(configFile, config), vm);
		configFile.close();
	}
	po::notify(vm);

	if (vm.count("help")) {
		std::cout << PACKAGE_STRING"\n";
		std::cout << "Usage: " << argv[0] << " [-u user -p password]\n";
		std::cout << desc << "\n";
		std::cout << "You must give either both the username and "
			"password or none of them." << std::endl;

		return 0;
	}

	if (vm.count("version")) {
		std::cout << VERSION << std::endl;
		return 0;
	}

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
