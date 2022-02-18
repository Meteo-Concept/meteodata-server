#ifndef PRESSURE_TENDENCY_H
#define PRESSURE_TENDENCY_H

#include <iostream>

struct PressureTendency
{
	/**
	 * Characteristic of pressure tendency during the three hours
	 * preceding the time of observation.
	 * @see a, table 0200, A-244
	 */
	enum class Description
	{
		INCREASING_THEN_DECREASING = '0',
		INCREASING_THEN_MORE_SLOWLY,
		INCREASING,
		VARIABLE_THEN_INCREASING_MORE_QUICKLY,
		STEADY,
		DECREASING_THEN_INCREASING,
		DECREASING_THEN_MORE_SLOWLY,
		DECREASING,
		VARIABLE_THEN_DECREASING_MORE_QUICKLY
	};

	Description _description;
	/*
	 *  Amount of pressure tendency at station level during the three
	 *  hours preceding the time of observation; ppp, in tenths of a hectopascal.
	 */
	int _amount;
};

#endif /* PRESSURE_TENDENCY_H */
