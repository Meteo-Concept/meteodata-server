#ifndef STATION_H
#define STATION_H

#include <memory>

class Station
{
public:
	Station();

private:
	std::string _id;
	std::string _address;
	int _port;
	std::unique_ptr<Connector> _handle;
};

#endif // STATION_H
