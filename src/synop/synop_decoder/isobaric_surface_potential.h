#ifndef ISOBARIC_SURFACE_POTENTIAL_H
#define ISOBARIC_SURFACE_POTENTIAL_H

#include <iostream>

struct IsobaricSurfacePotential
{
	enum class StandardIsobaricSurface
	{
		S_1000 = '1', S_925 = '2', S_500 = '5', S_700 = '7', S_850 = '8'
	};

	/**
	 * Standard isobaric surface for which the geopotential is
	 * reported; a_3
	 * @see table 0264, A-247
	 */
	StandardIsobaricSurface _standardIsobaricSurface;

	/**
	 * Geopotential of an agreed standard isobaric surface given by
	 * a_3; hhh, in standard geopotential metres
	 */
	int _geopotential;
};

#endif /* ISOBARIC_SURFACE_POTENTIAL_H */
