#ifndef DBCONNECTION_H
#define DBCONNECTION_H

#include <cassandra.h>

#include <functional>
#include <tuple>
#include <memory>

namespace meteodata {
	struct DataPoint {
		std::string station;
		int64_t time;
		int32_t bartrend;
		int32_t barometer;
		int32_t barometer_abs;
		int32_t barometer_raw;
		int32_t insidetemp;
		int32_t outsidetemp;
		int32_t insidehum;
		int32_t outsidehum;
		int32_t extratemp[7];
		int32_t soiltemp[4];
		int32_t leaftemp[4];
		int32_t extrahum[7];
		int32_t soilmoistures[4];
		int32_t leafwetnesses[4];
		int32_t windspeed;
		int32_t winddir;
		int32_t avgwindspeed_10min;
		int32_t avgwindspeed_2min;
		int32_t windgust_10min;
		int32_t windgustdir;
		int32_t rainrate;
		int32_t rain_15min;
		int32_t rain_1h;
		int32_t rain_24h;
		int32_t dayrain;
		int32_t monthrain;
		int32_t yearrain;
		int32_t stormrain;
		uint32_t stormstartdate;
		int32_t UV;
		int32_t solarrad;
		int32_t dewpoint;
		int32_t heatindex;
		int32_t windchill;
		int32_t thswindex;
		int32_t dayET;
		int32_t monthET;
		int32_t yearET;
		std::string forecast;
		int32_t forecast_icons;
		int64_t sunrise;
		int64_t sunset;
	};

	class DbConnection
	{
	public:
		DbConnection();
		virtual ~DbConnection();
		std::tuple<std::string,int,int> getStationById(const std::string& id);
		bool insertDataPoint(const DataPoint& p);

	private:
		CassFuture* _futureConn;
		CassCluster* _cluster;
		CassSession* _session;
		std::unique_ptr<const CassPrepared, std::function<void(const CassPrepared*)>> _selectStationById;
		std::unique_ptr<const CassPrepared, std::function<void(const CassPrepared*)>> _insertDataPoint;
		void prepareStatements();
	};
}

#endif
