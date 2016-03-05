#ifndef DBCONNECTION_H
#define DBCONNECTION_H

#include <cassandra.h>

#include <functional>
#include <tuple>
#include <memory>

namespace meteodata {
	struct DataPoint {
		std::string station;
		std::string time;
		int bartrend;
		int barometer;
		int barometer_abs;
		int barometer_raw;
		int insidetemp;
		int outsidetemp;
		int insidehum;
		int outsidehum;
		int extratemp[7];
		int soiltemp[4];
		int leaftemp[4];
		int extrahum[7];
		int soilmoistures[4];
		int leafwetnesses[4];
		int windspeed;
		int winddir;
		int avgwindspeed_10min;
		int avgwindspeed_2min;
		int windgust_10min;
		int windgustdir;
		int rainrate;
		int rain_15min;
		int rain_1h;
		int rain_24h;
		int dayrain;
		int monthrain;
		int yearrain;
		int stormrain;
		int stormstartdate;
		int UV;
		int solarrad;
		int dewpoint;
		int heatindex;
		int windchill;
		int thswindex;
		int dayET;
		int monthET;
		int yearET;
		int forecast;
		int forecast_icons;
		int sunrise;
		int sunset;
	};

	class DbConnection
	{
	public:
		DbConnection();
		virtual ~DbConnection();
		std::tuple<std::string,int,int> getStationById(std::string id);
		bool insertDataPoint();

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
