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

#include <date/date.h>

namespace meteodata {
	/**
	 * @brief A handle to the database to insert meteorological measures
	 *
	 * An instance of this class is to be used by each meteo station
	 * connector to query details about the station and insert measures in
	 * the database periodically.
	 */
	class DbConnectionMinmax
	{
	public:
		/**
		 * @brief Construct a connection to the database
		 *
		 * @param user the username to use
		 * @param password the password corresponding to the username
		 */
		DbConnectionMinmax(const std::string& user = "", const std::string& password = "");
		/**
		 * @brief Close the connection and destroy the database handle
		 */
		virtual ~DbConnectionMinmax();

		struct Values
		{
			// Values from 0h to 0h
			float insideTemp_max;
			float leafTemp_max[4];
			float outsideTemp_max;
			float soilTemp_max[4];
			float extraTemp_max[7];

			// Values from 18h to 18h
			float insideTemp_min;
			float leafTemp_min[4];
			float outsideTemp_min;
			float soilTemp_min[4];
			float extraTemp_min[7];

			// Values from 0h to 0h
			float barometer_min;
			float barometer_max;
			float barometer_avg;
			int leafWetnesses_min[4];
			int leafWetnesses_max[4];
			int leafWetnesses_avg[4];
			int soilMoistures_min[4];
			int soilMoistures_max[4];
			int soilMoistures_avg[4];
			int insideHum_min;
			int insideHum_max;
			int insideHum_avg;
			int outsideHum_min;
			int outsideHum_max;
			int outsideHum_avg;
			int extraHum_min[7];
			int extraHum_max[7];
			int extraHum_avg[7];
			int solarRad_max;
			int solarRad_avg;
			int uv_max;
			int uv_avg;
			int winddir;
			float windgust_max;
			float windgust_avg;
			float windspeed_max;
			float windspeed_avg;
			float rainrate_max;
			float dewpoint_min;
			float dewpoint_max;
			float dewpoint_avg;

			// Computed values
			float dayRain;
			float monthRain;
			float yearRain;
			float dayEt;
			float monthEt;
			float yearEt;
			float insideTemp_avg;
			float leafTemp_avg[4];
			float outsideTemp_avg;
			float soilTemp_avg[4];
			float extraTemp_avg[7];
		};

		bool getAllStations(std::vector<CassUuid>& stations);

		bool insertDataPoint(const CassUuid& station, const date::sys_days& date, const Values& values);

		bool getValues6hTo6h(const CassUuid& station, const date::sys_days& date, Values& values);
		bool getValues18hTo18h(const CassUuid& station, const date::sys_days& date, Values& values);
		bool getValues0hTo0h(const CassUuid& station, const date::sys_days& date, Values& values);
		bool getYearlyValuesNow(const CassUuid& station, float& rain, float& et);
		bool getYearlyValues(const CassUuid& station, const date::sys_days& date, float& rain, float& et);

	private:
		/**
		 * @brief The Cassandra connection handle
		 */
		CassFuture* _futureConn;
		/**
		 * @brief The Cassandra cluster
		 */
		CassCluster* _cluster;
		/**
		 * @brief The Cassandra session data
		 */
		CassSession* _session;

		static constexpr char SELECT_ALL_STATIONS_STMT[] = "SELECT id FROM meteodata.stations";
		/**
		 * @brief The first prepared statement for the getAllStations()
		 * method
		 */
		std::unique_ptr<const CassPrepared, std::function<void(const CassPrepared*)>> _selectAllStations;
		static constexpr char SELECT_VALUES_6H_TO_6H_STMT[] =
			"SELECT "
				"MAX(insideTemp)     AS insideTemp_max,"
				"MAX(leafTemp1)      AS leafTemp1_max,"
				"MAX(leafTemp2)      AS leafTemp2_max,"
				"MAX(leafTemp3)      AS leafTemp3_max,"
				"MAX(leafTemp4)      AS leafTemp4_max,"
				"MAX(outsideTemp)    AS outsideTemp_max,"
				"MAX(soilTemp1)      AS soilTemp1_max,"
				"MAX(soilTemp2)      AS soilTemp2_max,"
				"MAX(soilTemp3)      AS soilTemp3_max,"
				"MAX(soilTemp4)      AS soilTemp4_max,"
				"MAX(extraTemp1)     AS extraTemp1_max,"
				"MAX(extraTemp2)     AS extraTemp2_max,"
				"MAX(extraTemp3)     AS extraTemp3_max,"
				"MAX(extraTemp4)     AS extraTemp4_max,"
				"MAX(extraTemp5)     AS extraTemp5_max,"
				"MAX(extraTemp6)     AS extraTemp6_max,"
				"MAX(extraTemp7)     AS extraTemp7_max "
				" FROM meteodata.meteo WHERE station = ? AND time >= ? AND time < ?";
			//	" FROM meteodata.meteo WHERE id = ? AND date IN (?,?) AND time >= ? AND time < ?";
		/**
		 * @brief The first prepared statement for the getValues()
		 * method
		 */
		std::unique_ptr<const CassPrepared, std::function<void(const CassPrepared*)>> _selectValues6hTo6h;

		static constexpr char SELECT_VALUES_0H_TO_0H_STMT[] =
			"SELECT "
				"MIN(barometer)               AS barometer_min,"
				"MAX(barometer)               AS barometer_max,"
				"AVG(barometer)               AS barometer_avg,"
				"MIN(leafWetnesses1)          AS leafWetnesses1_min,"
				"MAX(leafWetnesses1)          AS leafWetnesses1_max,"
				"AVG(leafWetnesses1)          AS leafWetnesses1_avg,"
				"MIN(leafWetnesses2)          AS leafWetnesses2_min,"
				"MAX(leafWetnesses2)          AS leafWetnesses2_max,"
				"AVG(leafWetnesses2)          AS leafWetnesses2_avg,"
				"MIN(leafWetnesses3)          AS leafWetnesses3_min,"
				"MAX(leafWetnesses3)          AS leafWetnesses3_max,"
				"AVG(leafWetnesses3)          AS leafWetnesses3_avg,"
				"MIN(leafWetnesses4)          AS leafWetnesses4_min,"
				"MAX(leafWetnesses4)          AS leafWetnesses4_max,"
				"AVG(leafWetnesses4)          AS leafWetnesses4_avg,"
				"MIN(soilMoistures1)          AS soilMoistures1_min,"
				"MAX(soilMoistures1)          AS soilMoistures1_max,"
				"AVG(soilMoistures1)          AS soilMoistures1_avg,"
				"MIN(soilMoistures2)          AS soilMoistures2_min,"
				"MAX(soilMoistures2)          AS soilMoistures2_max,"
				"AVG(soilMoistures2)          AS soilMoistures2_avg,"
				"MIN(soilMoistures3)          AS soilMoistures3_min,"
				"MAX(soilMoistures3)          AS soilMoistures3_max,"
				"AVG(soilMoistures3)          AS soilMoistures3_avg,"
				"MIN(soilMoistures4)          AS soilMoistures4_min,"
				"MAX(soilMoistures4)          AS soilMoistures4_max,"
				"AVG(soilMoistures4)          AS soilMoistures4_avg,"
				"MIN(insideHum)               AS insideHum_min,"
				"MAX(insideHum)               AS insideHum_max,"
				"AVG(insideHum)               AS insideHum_avg,"
				"MIN(outsideHum)              AS outsideHum_min,"
				"MAX(outsideHum)              AS outsideHum_max,"
				"AVG(outsideHum)              AS outsideHum_avg,"
				"MIN(extraHum1)               AS extraHum1_min,"
				"MAX(extraHum1)               AS extraHum1_max,"
				"AVG(extraHum1)               AS extraHum1_avg,"
				"MIN(extraHum2)               AS extraHum2_min,"
				"MAX(extraHum2)               AS extraHum2_max,"
				"AVG(extraHum2)               AS extraHum2_avg,"
				"MIN(extraHum3)               AS extraHum3_min,"
				"MAX(extraHum3)               AS extraHum3_max,"
				"AVG(extraHum3)               AS extraHum3_avg,"
				"MIN(extraHum4)               AS extraHum4_min,"
				"MAX(extraHum4)               AS extraHum4_max,"
				"AVG(extraHum4)               AS extraHum4_avg,"
				"MIN(extraHum5)               AS extraHum5_min,"
				"MAX(extraHum5)               AS extraHum5_max,"
				"AVG(extraHum5)               AS extraHum5_avg,"
				"MIN(extraHum6)               AS extraHum6_min,"
				"MAX(extraHum6)               AS extraHum6_max,"
				"AVG(extraHum6)               AS extraHum6_avg,"
				"MIN(extraHum7)               AS extraHum7_min,"
				"MAX(extraHum7)               AS extraHum7_max,"
				"AVG(extraHum7)               AS extraHum7_avg,"
				"MAX(solarRad)                AS solarRad_max,"
				"AVG(solarRad)                AS solarRad_avg,"
				"MAX(uv)                      AS uv_max,"
				"AVG(uv)                      AS uv_avg,"
				"COUNTMAXOCCURRENCES(winddir) AS winddir,"
				"MAX(windgust_10min)                AS windgust_max,"
				"AVG(windgust_10min)                AS windgust_avg,"
				"MAX(windspeed)               AS windspeed_max,"
				"AVG(windspeed)               AS windspeed_avg,"
				"MAX(rainrate)                AS rainrate_max,"
				"MIN(dewpoint)                AS dewpoint_min,"
				"MIN(dewpoint)                AS dewpoint_max,"
				"AVG(dewpoint)                AS dewpoint_avg "
				" FROM meteodata.meteo WHERE station = ? AND time >= ? AND time < ?";
			//	" FROM meteodata.meteo WHERE id = ? AND date = ?";
		/**
		 * @brief The first prepared statement for the getValues()
		 * method
		 */
		std::unique_ptr<const CassPrepared, std::function<void(const CassPrepared*)>> _selectValues0hTo0h;

		static constexpr char SELECT_VALUES_18H_TO_18H_STMT[] =
			"SELECT "
				"MIN(insideTemp)     AS insideTemp_min,"
				"MIN(leafTemp1)      AS leafTemp1_min,"
				"MIN(leafTemp2)      AS leafTemp2_min,"
				"MIN(leafTemp3)      AS leafTemp3_min,"
				"MIN(leafTemp4)      AS leafTemp4_min,"
				"MIN(outsideTemp)    AS outsideTemp_min,"
				"MIN(soilTemp1)      AS soilTemp1_min,"
				"MIN(soilTemp2)      AS soilTemp2_min,"
				"MIN(soilTemp3)      AS soilTemp3_min,"
				"MIN(soilTemp4)      AS soilTemp4_min,"
				"MIN(extraTemp1)     AS extraTemp1_min,"
				"MIN(extraTemp2)     AS extraTemp2_min,"
				"MIN(extraTemp3)     AS extraTemp3_min,"
				"MIN(extraTemp4)     AS extraTemp4_min,"
				"MIN(extraTemp5)     AS extraTemp5_min,"
				"MIN(extraTemp6)     AS extraTemp6_min,"
				"MIN(extraTemp7)     AS extraTemp7_min "
				" FROM meteodata.meteo WHERE station = ? AND time >= ? AND time < ?";
			//	" FROM meteodata.meteo WHERE id = ? AND date IN (?,?) AND time >= ? AND time <= ?";
		/**
		 * @brief The first prepared statement for the getValues()
		 * method
		 */
		std::unique_ptr<const CassPrepared, std::function<void(const CassPrepared*)>> _selectValues18hTo18h;

		static constexpr char SELECT_YEARLY_VALUES_STMT[] =
			"SELECT yearrain,yearET FROM meteodata.meteo WHERE station = ? AND time >= ? AND time < ? LIMIT 1";
			//"SELECT yearrain,yearET FROM meteodata.meteo WHERE id = ? AND date = ? AND time >= ? AND time < ?";
		std::unique_ptr<const CassPrepared, std::function<void(const CassPrepared*)>> _selectYearlyValues;

		static constexpr char SELECT_YEARLY_VALUES_NOW_STMT[] =
			"SELECT yearrain,yearET FROM meteodata.meteo WHERE station = ? LIMIT 1";
		std::unique_ptr<const CassPrepared, std::function<void(const CassPrepared*)>> _selectYearlyValuesNow;

		static constexpr char INSERT_DATAPOINT_STMT[] =
			"INSERT INTO meteodata.minmax ("
			"station,"
			"date,"
			"barometer_min, barometer_max, barometer_avg,"
			"dayet, monthet, yearet,"
			"dayrain, monthrain, yearrain,"
			"dewpoint_min, dewpoint_max, dewpoint_avg,"
			"insidehum_min, insidehum_max, insidehum_avg,"
			"insidetemp_min, insidetemp_max, insidetemp_avg,"
			"leaftemp1_min, leaftemp1_max, leaftemp1_avg,"
			"leaftemp2_min, leaftemp2_max, leaftemp2_avg,"
			"leaftemp3_min, leaftemp3_max, leaftemp3_avg,"
			"leaftemp4_min, leaftemp4_max, leaftemp4_avg,"
			"leafwetnesses1_min, leafwetnesses1_max, leafwetnesses1_avg,"
			"leafwetnesses2_min, leafwetnesses2_max, leafwetnesses2_avg,"
			"leafwetnesses3_min, leafwetnesses3_max, leafwetnesses3_avg,"
			"leafwetnesses4_min, leafwetnesses4_max, leafwetnesses4_avg,"
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
			"extratemp4_min, extratemp4_max, extratemp4_avg,"
			"extratemp5_min, extratemp5_max, extratemp5_avg,"
			"extratemp6_min, extratemp6_max, extratemp6_avg,"
			"extratemp7_min, extratemp7_max, extratemp7_avg,"
			"extrahum1_min, extrahum1_max, extrahum1_avg,"
			"extrahum2_min, extrahum2_max, extrahum2_avg,"
			"extrahum3_min, extrahum3_max, extrahum3_avg,"
			"extrahum4_min, extrahum4_max, extrahum4_avg,"
			"extrahum5_min, extrahum5_max, extrahum5_avg,"
			"extrahum6_min, extrahum6_max, extrahum6_avg,"
			"extrahum7_min, extrahum7_max, extrahum7_avg,"
			"solarrad_max, solarrad_avg,"
			"uv_max, uv_avg,"
			"winddir,"
			"windgust_max, windgust_avg,"
			"windspeed_max, windspeed_avg)"
			" VALUES ("
			"?,"
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
