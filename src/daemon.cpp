#include <iostream>
#include <unistd.h>
#include <syslog.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>

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

