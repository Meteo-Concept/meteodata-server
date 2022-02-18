#ifndef EVAPO_MAYBE_TRANSPI_RATION_H
#define EVAPO_MAYBE_TRANSPI_RATION_H

#include <iostream>

struct EvapoMaybeTranspiRation
{
	/**
	 * Indicator of type of instrumentation for evaporation measurement or
	 * type of crop for which evapotranspiration is reported
	 * @see table 1806, A-294
	 */
	enum class Instrumentation
	{
		USA_EVAPORIMETER_WITHOUT_COVER = '0',
		USA_EVAPORIMETER_MESH_COVERED,
		GGI_3000_EVAPORIMENTER_SUNKEN,
		TANK_20_M3,
		OTHER_EVAPORIMETER,
		RICE,
		WHEAT,
		MAIZE,
		SORGHUM,
		OTHER_CROP,
		UNKNOWN = '/'
	};

	Instrumentation _instrumentation;

	/**
	 * Amount of evapotranspiration or evaporation; EEE, in tenths of mm
	 */
	int _amount;
};

#endif /* EVAPO_MAYBE_TRANSPI_RATION_H */
