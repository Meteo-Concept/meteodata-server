bin_PROGRAMS = meteodata meteodata-minmax meteodata-month-minmax
meteodata_SOURCES = \
		    daemon.cpp\
		    dbconnection_common.h\
		    dbconnection_common.cpp\
		    dbconnection.cpp\
		    dbconnection.h\
		    vantagepro2connector.h\
		    vantagepro2connector.cpp\
		    connector.h\
		    connector.cpp\
		    meteo_server.h\
		    meteo_server.cpp\
		    message.h\
		    vantagepro2message.h\
		    vantagepro2message.cpp\
		    vantagepro2archivepage.h\
		    vantagepro2archivepage.cpp\
		    vantagepro2archivemessage.h\
		    vantagepro2archivemessage.cpp\
		    timeoffseter.h\
		    timeoffseter.cpp\
		    weatherlinkdownloader.h\
		    weatherlinkdownloader.cpp\
		    weatherlink_api_realtime_message.h\
		    weatherlink_api_realtime_message.cpp\
		    date/src/tz.cpp\
		    date/include/date/date.h\
		    date/include/date/tz.h\
		    date/include/date/tz_private.h\
		    date/include/date/ios.h

meteodata_minmax_SOURCES = \
			   minmax.cpp\
			   dbconnection_common.h\
			   dbconnection_common.cpp\
			   dbconnection_minmax.h\
			   dbconnection_minmax.cpp\
			   date/include/date/date.h

meteodata_month_minmax_SOURCES = \
			   month_minmax.cpp\
			   dbconnection_common.h\
			   dbconnection_common.cpp\
			   dbconnection_month_minmax.h\
			   dbconnection_month_minmax.cpp\
			   date/include/date/date.h

meteodata_CPPFLAGS = $(PTHREAD_CFLAGS) $(BOOST_ASIO_CPPFLAGS) $(BOOST_SYSTEM_CPPFLAGS) $(BOOST_PROGRAM_OPTIONS_CPPFLAGS) $(BOOST_PROPERTY_TREE_CPPFLAGS) $(CASSANDRA_CPPFLAGS) -I$(srcdir)/date/include
meteodata_CXXFLAGS = -std=c++14 -Wall -Wextra -pedantic -DUSE_OS_TZDB=1
meteodata_LDFLAGS = $(BOOST_ASIO_LDFLAGS)
meteodata_LDADD = $(PTHREAD_CFLAGS) $(BOOST_ASIO_LIBS) $(BOOST_SYSTEM_LIBS) $(BOOST_PROGRAM_OPTIONS_LIBS) $(BOOST_PROPERTY_TREE_LIBS)

meteodata_minmax_CPPFLAGS = $(PTHREAD_CFLAGS) $(BOOST_PROGRAM_OPTIONS_CPPFLAGS) $(CASSANDRA_CPPFLAGS) -I$(srcdir)/date/include
meteodata_minmax_CXXFLAGS = -std=c++14 -Wall -Wextra -pedantic
meteodata_minmax_LDFLAGS = 
meteodata_minmax_LDADD = $(PTHREAD_CFLAGS) $(BOOST_PROGRAM_OPTIONS_LIBS)

meteodata_month_minmax_CPPFLAGS = $(PTHREAD_CFLAGS) $(BOOST_PROGRAM_OPTIONS_CPPFLAGS) $(CASSANDRA_CPPFLAGS) -I$(srcdir)/date/include
meteodata_month_minmax_CXXFLAGS = -std=c++14 -Wall -Wextra -pedantic
meteodata_month_minmax_LDFLAGS = 
meteodata_month_minmax_LDADD = $(PTHREAD_CFLAGS) $(BOOST_PROGRAM_OPTIONS_LIBS)
