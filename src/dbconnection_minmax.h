/**
 * @file dbconnection_minmax.h
 * @brief Definition of the DbConnectionMinmax class
 * @author Laurent Georget
 * @date 2017-10-30
 */
/*
 * Copyright (C) 2017  SAS Météo Concept <contact@meteo-concept.fr>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef DBCONNECTION_MINMAX_H
#define DBCONNECTION_MINMAX_H

#include <cassandra.h>

#include <ctime>
#include <functional>
#include <tuple>
#include <memory>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <utility>

#include <date/date.h>

#include "dbconnection_common.h"

namespace meteodata {

template<typename T, typename Op>
inline void compute(std::pair<bool, T>& result, const std::pair<bool, T>& op1, const std::pair<bool, T>& op2, Op op)
{
	if (op1.first && op2.first) {
		result.second = op(op1.second, op2.second);
		result.first = true;
	} else {
		result.first = false;
	}
}

template<typename T>
inline void computeMin(std::pair<bool, T>& result, const std::pair<bool, T>& op1, const std::pair<bool, T>& op2)
{
	compute(result, op1, op2, [](const T& t1, const T& t2){ return t1 < t2 ? t1 : t2; });
}

template<typename T>
inline void computeMax(std::pair<bool, T>& result, const std::pair<bool, T>& op1, const std::pair<bool, T>& op2)
{
	compute(result, op1, op2, [](const T& t1, const T& t2){ return t1 >= t2 ? t1 : t2; });
}

template<typename T>
inline void computeMean(std::pair<bool, T>& result, const std::pair<bool, T>& op1, const std::pair<bool, T>& op2)
{
	compute(result, op1, op2, [](const T& t1, const T& t2){ return (t1 + t2) / 2; });
}

	/**
	 * @brief A handle to the database to insert meteorological measures
	 *
	 * An instance of this class is to be used by each meteo station
	 * connector to query details about the station and insert measures in
	 * the database periodically.
	 */
	class DbConnectionMinmax : public DbConnectionCommon
	{
	public:
		/**
		 * @brief Construct a connection to the database
		 *
		 * @param user the username to use
		 * @param password the password corresponding to the username
		 */
		DbConnectionMinmax(const std::string& address = "127.0.0.1", const std::string& user = "", const std::string& password = "");
		/**
		 * @brief Close the connection and destroy the database handle
		 */
		virtual ~DbConnectionMinmax() = default;

		struct Values
		{
			// Values from 6h to 6h
			std::pair<bool, float> insideTemp_max;
			std::pair<bool, float> leafTemp_max[2];
			std::pair<bool, float> outsideTemp_max;
			std::pair<bool, float> soilTemp_max[4];
			std::pair<bool, float> extraTemp_max[3];
			std::pair<bool, float> rainfall;

			// Values from 18h to 18h
			std::pair<bool, float> insideTemp_min;
			std::pair<bool, float> leafTemp_min[2];
			std::pair<bool, float> outsideTemp_min;
			std::pair<bool, float> soilTemp_min[4];
			std::pair<bool, float> extraTemp_min[3];

			// Values from 0h to 0h
			std::pair<bool, float> barometer_min;
			std::pair<bool, float> barometer_max;
			std::pair<bool, float> barometer_avg;
			std::pair<bool, int> leafWetnesses_min[2];
			std::pair<bool, int> leafWetnesses_max[2];
			std::pair<bool, int> leafWetnesses_avg[2];
			std::pair<bool, int> soilMoistures_min[4];
			std::pair<bool, int> soilMoistures_max[4];
			std::pair<bool, int> soilMoistures_avg[4];
			std::pair<bool, int> insideHum_min;
			std::pair<bool, int> insideHum_max;
			std::pair<bool, int> insideHum_avg;
			std::pair<bool, int> outsideHum_min;
			std::pair<bool, int> outsideHum_max;
			std::pair<bool, int> outsideHum_avg;
			std::pair<bool, int> extraHum_min[2];
			std::pair<bool, int> extraHum_max[2];
			std::pair<bool, int> extraHum_avg[2];
			std::pair<bool, int> solarRad_max;
			std::pair<bool, int> solarRad_avg;
			std::pair<bool, int> uv_max;
			std::pair<bool, int> uv_avg;
			std::pair<bool, std::vector<int>> winddir;
			std::pair<bool, float> windgust_max;
			std::pair<bool, float> windgust_avg;
			std::pair<bool, float> windspeed_max;
			std::pair<bool, float> windspeed_avg;
			std::pair<bool, float> rainrate_max;
			std::pair<bool, float> dewpoint_min;
			std::pair<bool, float> dewpoint_max;
			std::pair<bool, float> dewpoint_avg;
			std::pair<bool, float> et;

			// Computed values
			std::pair<bool, float> dayRain;
			std::pair<bool, float> monthRain;
			std::pair<bool, float> yearRain;
			std::pair<bool, float> dayEt;
			std::pair<bool, float> monthEt;
			std::pair<bool, float> yearEt;
			std::pair<bool, float> insideTemp_avg;
			std::pair<bool, float> leafTemp_avg[4];
			std::pair<bool, float> outsideTemp_avg;
			std::pair<bool, float> soilTemp_avg[4];
			std::pair<bool, float> extraTemp_avg[3];
		};

		bool insertDataPoint(const CassUuid& station, const date::sys_days& date, const Values& values);

		bool getValues6hTo6h(const CassUuid& station, const date::sys_days& date, Values& values);
		bool getValues18hTo18h(const CassUuid& station, const date::sys_days& date, Values& values);
		bool getValues0hTo0h(const CassUuid& station, const date::sys_days& date, Values& values);
		bool getYearlyValues(const CassUuid& station, const date::sys_days& date, std::pair<bool, float>& rain, std::pair<bool, float>& et);

	private:
		static constexpr char SELECT_VALUES_AFTER_6H_STMT[] =
			"SELECT "
				"MAX(insidetemp)     AS insideTemp_max,"
				"MAX(leaftemp1)      AS leafTemp1_max,"
				"MAX(leaftemp2)      AS leafTemp2_max,"
				"MAX(outsidetemp)    AS outsideTemp_max,"
				"MAX(soiltemp1)      AS soilTemp1_max,"
				"MAX(soiltemp2)      AS soilTemp2_max,"
				"MAX(soiltemp3)      AS soilTemp3_max,"
				"MAX(soiltemp4)      AS soilTemp4_max,"
				"MAX(extratemp1)     AS extraTemp1_max,"
				"MAX(extratemp2)     AS extraTemp2_max,"
				"MAX(extratemp3)     AS extraTemp3_max,"
				"SUM(rainfall)       AS rainfall "
			//	" FROM meteodata.meteo WHERE station = ? AND time >= ? AND time < ?";
				" FROM meteodata_v2.meteo WHERE station = ? AND day = ? AND time >= ?";

		static constexpr char SELECT_VALUES_BEFORE_6H_STMT[] =
			"SELECT "
				"MAX(insidetemp)     AS insideTemp_max,"
				"MAX(leaftemp1)      AS leafTemp1_max,"
				"MAX(leaftemp2)      AS leafTemp2_max,"
				"MAX(outsidetemp)    AS outsideTemp_max,"
				"MAX(soiltemp1)      AS soilTemp1_max,"
				"MAX(soiltemp2)      AS soilTemp2_max,"
				"MAX(soiltemp3)      AS soilTemp3_max,"
				"MAX(soiltemp4)      AS soilTemp4_max,"
				"MAX(extratemp1)     AS extraTemp1_max,"
				"MAX(extratemp2)     AS extraTemp2_max,"
				"MAX(extratemp3)     AS extraTemp3_max,"
				"SUM(rainfall)       AS rainfall "
			//	" FROM meteodata.meteo WHERE station = ? AND time >= ? AND time < ?";
				" FROM meteodata_v2.meteo WHERE station = ? AND day = ? AND time < ?";
		/**
		 * @brief The first prepared statement for the getValues()
		 * method
		 */
		std::unique_ptr<const CassPrepared, std::function<void(const CassPrepared*)>> _selectValuesAfter6h;
		std::unique_ptr<const CassPrepared, std::function<void(const CassPrepared*)>> _selectValuesAfter18h;

		static constexpr char SELECT_VALUES_ALL_DAY_STMT[] =
			"SELECT "
				"MIN(barometer)               AS barometer_min,"
				"MAX(barometer)               AS barometer_max,"
				"AVG(barometer)               AS barometer_avg,"
				"MIN(leafwetnesses1)          AS leafWetnesses1_min,"
				"MAX(leafwetnesses1)          AS leafWetnesses1_max,"
				"AVG(leafwetnesses1)          AS leafWetnesses1_avg,"
				"MIN(leafwetnesses2)          AS leafWetnesses2_min,"
				"MAX(leafwetnesses2)          AS leafWetnesses2_max,"
				"AVG(leafwetnesses2)          AS leafWetnesses2_avg,"
				"MIN(soilmoistures1)          AS soilMoistures1_min,"
				"MAX(soilmoistures1)          AS soilMoistures1_max,"
				"AVG(soilmoistures1)          AS soilMoistures1_avg,"
				"MIN(soilmoistures2)          AS soilMoistures2_min,"
				"MAX(soilmoistures2)          AS soilMoistures2_max,"
				"AVG(soilmoistures2)          AS soilMoistures2_avg,"
				"MIN(soilmoistures3)          AS soilMoistures3_min,"
				"MAX(soilmoistures3)          AS soilMoistures3_max,"
				"AVG(soilmoistures3)          AS soilMoistures3_avg,"
				"MIN(soilmoistures4)          AS soilMoistures4_min,"
				"MAX(soilmoistures4)          AS soilMoistures4_max,"
				"AVG(soilmoistures4)          AS soilMoistures4_avg,"
				"MIN(insidehum)               AS insideHum_min,"
				"MAX(insidehum)               AS insideHum_max,"
				"AVG(insidehum)               AS insideHum_avg,"
				"MIN(outsidehum)              AS outsideHum_min,"
				"MAX(outsidehum)              AS outsideHum_max,"
				"AVG(outsidehum)              AS outsideHum_avg,"
				"MIN(extrahum1)               AS extraHum1_min,"
				"MAX(extrahum1)               AS extraHum1_max,"
				"AVG(extrahum1)               AS extraHum1_avg,"
				"MIN(extrahum2)               AS extraHum2_min,"
				"MAX(extrahum2)               AS extraHum2_max,"
				"AVG(extrahum2)               AS extraHum2_avg,"
				"MAX(solarrad)                AS solarRad_max,"
				"AVG(solarrad)                AS solarRad_avg,"
				"MAX(uv)                      AS uv_max,"
				"AVG(uv)                      AS uv_avg,"
				"MAX(windgust)                AS windgust_max,"
				"AVG(windgust)                AS windgust_avg,"
				"MAX(windspeed)               AS windspeed_max,"
				"AVG(windspeed)               AS windspeed_avg,"
				"MAX(rainrate)                AS rainrate_max,"
				"MIN(dewpoint)                AS dewpoint_min,"
				"MAX(dewpoint)                AS dewpoint_max,"
				"AVG(dewpoint)                AS dewpoint_avg,"
				"SUM(et)                      AS et"
			//	" FROM meteodata.meteo WHERE station = ? AND time >= ? AND time < ?";
				" FROM meteodata_v2.meteo WHERE station = ? AND day = ?";
		/**
		 * @brief The first prepared statement for the getValues()
		 * method
		 */
		std::unique_ptr<const CassPrepared, std::function<void(const CassPrepared*)>> _selectValuesAllDay;

		static constexpr char SELECT_VALUES_AFTER_18H_STMT[] =
			"SELECT "
				"MIN(insidetemp)     AS insideTemp_min,"
				"MIN(leaftemp1)      AS leafTemp1_min,"
				"MIN(leaftemp2)      AS leafTemp2_min,"
				"MIN(outsidetemp)    AS outsideTemp_min,"
				"MIN(soiltemp1)      AS soilTemp1_min,"
				"MIN(soiltemp2)      AS soilTemp2_min,"
				"MIN(soiltemp3)      AS soilTemp3_min,"
				"MIN(soiltemp4)      AS soilTemp4_min,"
				"MIN(extratemp1)     AS extraTemp1_min,"
				"MIN(extratemp2)     AS extraTemp2_min,"
				"MIN(extratemp3)     AS extraTemp3_min "
			//	" FROM meteodata.meteo WHERE station = ? AND time >= ? AND time < ?";
				" FROM meteodata_v2.meteo WHERE station = ? AND day = ? AND time >= ?";

		static constexpr char SELECT_VALUES_BEFORE_18H_STMT[] =
			"SELECT "
				"MIN(insidetemp)     AS insideTemp_min,"
				"MIN(leaftemp1)      AS leafTemp1_min,"
				"MIN(leaftemp2)      AS leafTemp2_min,"
				"MIN(outsidetemp)    AS outsideTemp_min,"
				"MIN(soiltemp1)      AS soilTemp1_min,"
				"MIN(soiltemp2)      AS soilTemp2_min,"
				"MIN(soiltemp3)      AS soilTemp3_min,"
				"MIN(soiltemp4)      AS soilTemp4_min,"
				"MIN(extratemp1)     AS extraTemp1_min,"
				"MIN(extratemp2)     AS extraTemp2_min,"
				"MIN(extratemp3)     AS extraTemp3_min "
			//	" FROM meteodata.meteo WHERE station = ? AND time >= ? AND time < ?";
				" FROM meteodata_v2.meteo WHERE station = ? AND day = ? AND time < ?";
		/**
		 * @brief The first prepared statement for the getValues()
		 * method
		 */
		std::unique_ptr<const CassPrepared, std::function<void(const CassPrepared*)>> _selectValuesBefore6h;
		std::unique_ptr<const CassPrepared, std::function<void(const CassPrepared*)>> _selectValuesBefore18h;

		static constexpr char SELECT_YEARLY_VALUES_STMT[] =
			"SELECT yearrain,yearet FROM meteodata_v2.minmax WHERE station = ? AND monthyear = ? AND day = ?";
		std::unique_ptr<const CassPrepared, std::function<void(const CassPrepared*)>> _selectYearlyValues;

		static constexpr char INSERT_DATAPOINT_STMT[] =
			"INSERT INTO meteodata_v2.minmax ("
			"station,"
			"monthyear, day,"
			"barometer_min, barometer_max, barometer_avg,"
			"dayet, monthet, yearet,"
			"dayrain, monthrain, yearrain,"
			"dewpoint_max, dewpoint_avg,"
			"insidehum_min, insidehum_max, insidehum_avg,"
			"insidetemp_min, insidetemp_max, insidetemp_avg,"
			"leaftemp1_min, leaftemp1_max, leaftemp1_avg,"
			"leaftemp2_min, leaftemp2_max, leaftemp2_avg,"
			"leafwetnesses1_min, leafwetnesses1_max, leafwetnesses1_avg,"
			"leafwetnesses2_min, leafwetnesses2_max, leafwetnesses2_avg,"
			"outsidehum_min, outsidehum_max, outsidehum_avg,"
			"outsidetemp_min, outsidetemp_max, outsidetemp_avg,"
			"rainrate_max,"
			"soilmoistures1_min, soilmoistures1_max, soilmoistures1_avg,"
			"soilmoistures2_min, soilmoistures2_max, soilmoistures2_avg,"
			"soilmoistures3_min, soilmoistures3_max, soilmoistures3_avg,"
			"soilmoistures4_min, soilmoistures4_max, soilmoistures4_avg,"
			"soiltemp1_min, soiltemp1_max, soiltemp1_avg,"
			"soiltemp2_min, soiltemp2_max, soiltemp2_avg,"
			"soiltemp3_min, soiltemp3_max, soiltemp3_avg,"
			"soiltemp4_min, soiltemp4_max, soiltemp4_avg,"
			"extratemp1_min, extratemp1_max, extratemp1_avg,"
			"extratemp2_min, extratemp2_max, extratemp2_avg,"
			"extratemp3_min, extratemp3_max, extratemp3_avg,"
			"extrahum1_min, extrahum1_max, extrahum1_avg,"
			"extrahum2_min, extrahum2_max, extrahum2_avg,"
			"solarrad_max, solarrad_avg,"
			"uv_max, uv_avg,"
			"winddir,"
			"windgust_max, windgust_avg,"
			"windspeed_max, windspeed_avg)"
			" VALUES ("
			"?,"
			"?, ?,"
			"?, ?, ?,"
			"?, ?, ?,"
			"?, ?, ?,"
			"?, ?,"
			"?, ?, ?,"
			"?, ?, ?,"
			"?, ?, ?,"
			"?, ?, ?,"
			"?, ?, ?,"
			"?, ?, ?,"
			"?, ?, ?,"
			"?, ?, ?,"
			"?,"
			"?, ?, ?,"
			"?, ?, ?,"
			"?, ?, ?,"
			"?, ?, ?,"
			"?, ?, ?,"
			"?, ?, ?,"
			"?, ?, ?,"
			"?, ?, ?,"
			"?, ?, ?,"
			"?, ?, ?,"
			"?, ?, ?,"
			"?, ?, ?,"
			"?, ?, ?,"
			"?, ?,"
			"?, ?,"
			"?,"
			"?, ?,"
			"?, ?)";
		/**
		 * @brief The prepared statement for the insetDataPoint() method
		 */
		std::unique_ptr<const CassPrepared, std::function<void(const CassPrepared*)>> _insertDataPoint;
		/**
		 * @brief Prepare the Cassandra query/insert statements
		 */
		void prepareStatements();
	};
}

#endif
