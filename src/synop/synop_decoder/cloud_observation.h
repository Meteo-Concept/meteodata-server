#ifndef CLOUD_OBSERVATION_H
#define CLOUD_OBSERVATION_H

#include "phenomenon.h"
#include "range.h"
#include "direction.h"

#include <iostream>

#include "nebulosity.h"

enum class CloudGenus
{
	CIRRUS = '0',
	CIRROCUMULUS,
	CIRROSTRATUS,
	ALTOCUMULUS,
	ALTOSTRATUS,
	NIMBOSTRATUS,
	STRATOCUMULUS,
	STRATUS,
	CUMULUS,
	CUMULONIMBUS,
	NOT_IDENTIFIED
};

struct CloudObservation
{
	CloudGenus _genus;
	Direction _direction;
	Range<int> _distance;
	Nebulosity _nebulosity;
};

struct CloudElevation
{
	/**
	 * Elevation angle of the top of clouds or phenomenon
	 * @see table 1004, A-279
	 */
	enum class ElevationAngle
	{
		NOT_VISIBLE = '0',
		MORE_THAN_45_DEG,
		ABOUT_30_DEG,
		ABOUT_20_DEG,
		ABOUT_15_DEG,
		ABOUT_12_DEG,
		ABOUT_9_DEG,
		ABOUT_7_DEG,
		ABOUT_6_DEG,
		LESS_THAN_5_DEG,
		NOT_AVAILABLE = '/'
	};

	CloudGenus _genus;
	Direction _direction;
	ElevationAngle _angle;
};
#endif /* CLOUD_OBSERVATION_H */
