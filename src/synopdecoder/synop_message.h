#ifndef SYNOP_MESSAGE_H
#define SYNOP_MESSAGE_H

#include <experimental/optional>
#include <vector>

#include "range.h"
#include "nebulosity.h"
#include "isobaric_surface_potential.h"
#include "pressure_tendency.h"
#include "precipitation_amount.h"
#include "evapo_maybe_transpi_ration.h"
#include "ground_state.h"
#include "cloud_observation.h"

struct SynopMessage
{
	/**
	 * Indicator for units of wind speed
	 * @see table 1855, A-296
	 */
	enum class WindSpeedUnit {
		METERS_PER_SECOND,
		KNOTS
	};

	/**
	 * Indicator for inclusion or omission of precipitation data
	 * @see table 1819, A-295
	 */
	enum class PrecipitationAvailability {
		SECTION_1 = '1',
		SECTION_3 = '2',
		SECTION_1_AND_3 = '0',
		NO_PRECIPITATION = '3',
		NOT_MEASURED = '4',
		NOT_AVAILABLE = '/'
	};

	/**
	 * Indicator for present and past weather data
	 * @see table 1860, A-297
	 */
	enum class PhenomenaObservationsAvailable {
		BASIC_OBSERVATIONS,
		ADVANCED_OBSERVATIONS,
		NO_PHENOMENON,
		NOT_OBSERVED,
		NOT_AVAILABLE
	};

	using TimePoint = std::chrono::time_point<std::chrono::system_clock, std::chrono::minutes>;

	std::string _stationIcao;
	/**
	 * Indicator for source of wind speed; i_w
	 * @see table 1855, A-296
	 */
	bool _withAnemometer;
	/**
	 * Indicator for unit of wind speed; i_w
	 * @see table 1855, A-296
	 */
	WindSpeedUnit _windSpeedUnit;
	TimePoint _observationTime;
	/**
	 * Indicator for type of station operation (manned or automatic); i_x
	 * @see table 1860, A-297
	 */
	bool _manned;
	/**
	 * Indicator for present and past weather data; i_x
	 * @see table 1860, A-297
	 */
	PhenomenaObservationsAvailable _phenomena;

	bool _sections[6];

	//! Height above surface of the base of the lowest cloud seen; h, table 1600, A-288
	std::optional<Range<int>> _hBaseLowestCloud;

	//! Horizontal visibility at surface; VV, table 4377, A-351
	std::optional<Range<float>> _horizVisibility;

	//! Nebulosity; N, table 2700, A-313
	std::optional<Nebulosity> _cloudCover;

	//! Wind direction; dd, in tens of degrees
	std::optional<int> _meanWindDirection;

	//! Wind speed; ff or fff, in the unit given by i_R (see metadata)
	std::optional<int> _meanWindSpeed;

	//! Mean temperature; TTT, in tenths of °C
	std::optional<int> _meanTemperature;

	//! Dew point; T_dT_dT_d, in tenths of °C
	std::optional<int> _dewPoint;

	//! Relative humidity; UUU
	std::optional<int> _relativeHumidity;

	//! Barometric pressure at the station; P_0P_0P_0P_0, in hPa
	std::optional<int> _pressureAtStation;

	//! Barometric pressure at mean sea level; PPPP, in hPa
	std::optional<int> _pressureAtSeaLevel;

	//! Geopotential of an agreed standard isobaric surface; a_3hhh
	std::optional<IsobaricSurfacePotential> _isobaricSurfacePotential;

	/*
	 * Pressure tendency at station level during the three hours
	 * preceding the time of observation; appp
	 */
	std::optional<PressureTendency> _pressureTendency;

	/*
	 * Amount of precipitation which has fallen during some extent
	 * of time preceding the time of observation; RRRt_R
	 *
	 * This group may be present in section 1, 3, and 5.
	 */
	std::vector<PrecipitationAmount> _precipitation;

	// Observations are not coded for now

	std::optional<Nebulosity> _lowOrMediumCloudCover;

	/**
	 * Presence of clouds of the genera stratocumulus, stratus,cumulus and cumulonimbus; C_L
	 * @see table 0513, A-264
	 */
	std::optional<LowClouds> _lowClouds;

	/**
	 * Presence of clouds of the genera altocumulus, altostratus and nimbostratus; C_M
	 * @see table 0515, A-265
	 */
	std::optional<MediumClouds> _mediumClouds;

	/**
	 * Presence of clouds of the genera cirrus, cirrocumulusand cirrostratus; C_H
	 * @see table 0509, A-263
	 */
	std::optional<HighClouds> _highClouds;

	/**
	 * Maximum air temperature (over the last 24h in Europe); T_xT_xT_x, in tenths of degrees Celsius
	 */
	std::optional<int> _maxTemperature;

	/**
	 * Minimum air temperature (over the last 24h in Europe); T_nT_nT_n, in tenths of degrees Celsius
	 */
	std::optional<int> _minTemperature;

	/**
	 * State of the ground without snow or measurable ice cover; E
	 * @see table 0901, A-274
	 */
	std::optional<GroundStateWithoutSnowOrIce> _groundStateWithoutSnowOrIce;

	/**
	 * Minimum soil temperature over the last night; T_gT_g, in degrees Celsius
	 */
	std::optional<int> _minSoilTemperature;

	/**
	 * State of the ground with snow or measurable ice cover; E'
	 * @see table 0975, A-276
	 */
	std::optional<GroundStateWithSnowOrIce> _groundStateWithSnowOrIce;

	/**
	 * Total depth of snow; sss
	 * @see table 3889, A-341
	 */
	std::optional<SnowDepth> _snowDepth;

	/**
	 * Daily amount of evaporation or evapotranspiration; EEEi_E, in tenths of mm
	 */
	std::optional<EvapoMaybeTranspiRation> _evapoMaybeTranspiRation;
	/**
	 * Daily hours of sunshine; SSS
	 */
	std::optional<float> _hoursOfSunshineLastDay;
	/**
	 * Duration of sunshine in the last hour; SS
	 */
	std::optional<int> _minutesOfSunshineLastHour;
	/**
	 * Net short-wave radiation during the previous hour; FFFF, in kJ.m^{-2}
	 */
	std::optional<int> _netShortWaveRadiationLastHour;
	/**
	 * Direct solar radiation during the previous hour; FFFF, in kJ.m^{-2}
	 */
	std::optional<int> _directSolarRadiationLastHour;

	std::optional<int> _netRadiationLastHour;
	std::optional<int> _globalSolarRadiationLastHour;
	std::optional<int> _diffusedSolarRadiationLastHour;
	std::optional<int> _downwardLongWaveRadiationLastHour;
	std::optional<int> _upwardLongWaveRadiationLastHour;
	std::optional<int> _shortWaveRadiationLastHour;
	/**
	 * Net short-wave radiation over the last 24 hours; F_{24}F_{24}F_{24}F_{24}, in J.cm^{-2}
	 */
	std::optional<int> _netShortWaveRadiationLast24Hours;
	/**
	 * Direct solar radiation over the last 24 hours; F_{24}F_{24}F_{24}F_{24}, in J.cm^{-2}
	 */
	std::optional<int> _directSolarRadiationLast24Hours;

	std::optional<int> _netRadiationLast24Hours;
	std::optional<int> _globalSolarRadiationLast24Hours;
	std::optional<int> _diffusedSolarRadiationLast24Hours;
	std::optional<int> _downwardLongWaveRadiationLast24Hours;
	std::optional<int> _upwardLongWaveRadiationLast24Hours;
	std::optional<int> _shortWaveRadiationLast24Hours;
	/**
	 * Direction and elevation of clouds; CD_ae_c
	 * @see table 1004, A-279 pour e_c
	 */
	std::vector<CloudElevation> _clouds;
	/**
	 * Direction of cloud drift for low clouds; D_L
	 */
	std::optional<Direction> _lowCloudsDrift;
	/**
	 * Direction of cloud drift for medium clouds; D_M
	 */
	std::optional<Direction> _mediumCloudsDrift;
	/**
	 * Direction of cloud drift for high clouds; D_H
	 */
	std::optional<Direction> _highCloudsDrift;

	/**
	 * Height of base of clouds; h_sh_s, in m
	 * @see table 1677, A-289
	 */
	std::vector<CloudObservation> _heightOfBaseOfClouds;
};

#endif /* SYNOP_MESSAGE_H */
