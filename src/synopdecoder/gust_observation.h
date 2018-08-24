#ifndef GUST_OBSERVATION_H
#define GUST_OBSERVATION_H

#include <iostream>

struct GustObservation
{
	/**
	 * Wind gust speed, in the unit given by the wind unit indicator i_w
	 */
	int _speed;

	/**
	 * Duration of period of observation,
	 * ending at the time of the report, in minutes
	 * @see table 4077
	 */
	int _duration;
};

#endif /* GUST_OBSERVATION_H */
