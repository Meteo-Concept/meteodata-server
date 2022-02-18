#ifndef PRECIPITATION_AMOUNT_H
#define PRECIPITATION_AMOUNT_H

#include <iostream>

struct PrecipitationAmount
{
	/**
	 * Amount of precipitation, in mm
	 * @see table 3590, A-325
	 */
	float _amount;

	/**
	 * Whether trace of precipitation has been observed, even
	 * if the amount is coded 0
	 */
	bool _trace = false;

	/**
	 * Duration of period of reference for amount of precipitation,
	 * ending at the time of the report, in hours
	 * @see table 4019
	 */
	int _duration;
};

struct SnowDepth
{
	/**
	 * Total depth of snow, in cm
	 * @see table 3889, A-341
	 */
	int _depth;

	/**
	 * Special codes for the depth of snow
	 * @see table 3889, A-341
	 */
	enum class SnowCoverageCondition
	{
		NO_SNOW, COVER_MORE_THAN_5_MM, COVER_LESS_THAN_5_MM, DISCONTINUOUS_COVER, NOT_MEASURED
	};

	/**
	 * Special codes for the depth of snow
	 * @see table 3889, A-341
	 */
	SnowCoverageCondition _cover = SnowCoverageCondition::NO_SNOW;
};

#endif /* PRECIPITATION_AMOUNT_H */
