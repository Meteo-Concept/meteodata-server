bin_PROGRAMS = meteodata meteodata-minmax meteodata-month-minmax meteodata-synop-standalone
meteodata_SOURCES = \
		    daemon.cpp\
		    vantagepro2connector.h\
		    vantagepro2connector.cpp\
		    connector.h\
		    connector.cpp\
		    meteo_server.h\
		    meteo_server.cpp\
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
		    mqttsubscriber.h\
		    mqttsubscriber.cpp\
		    synopdownloader.h\
		    synopdownloader.cpp\
		    ogimetsynop.h\
		    ogimetsynop.cpp\
		    synopdecoder/cloud_observation.h\
		    synopdecoder/direction.h\
		    synopdecoder/evapo_maybe_transpi_ration.h\
		    synopdecoder/ground_state.h\
		    synopdecoder/isobaric_surface_potential.h\
		    synopdecoder/nebulosity.h\
		    synopdecoder/parser.cpp\
		    synopdecoder/parser.h\
		    synopdecoder/phenomenon.h\
		    synopdecoder/precipitation_amount.h\
		    synopdecoder/pressure_tendency.h\
		    synopdecoder/range.h\
		    synopdecoder/gust_observation.h\
		    synopdecoder/synop_message.h

meteodata_minmax_SOURCES = \
			   minmax.cpp

meteodata_month_minmax_SOURCES = \
			   month_minmax.cpp

meteodata_synop_standalone_SOURCES = \
				     synop_standalone.h\
				     synop_standalone.cpp\
				     ogimetsynop.cpp\
				     ogimetsynop.h\
				     synopdecoder/cloud_observation.h\
				     synopdecoder/direction.h\
				     synopdecoder/evapo_maybe_transpi_ration.h\
				     synopdecoder/ground_state.h\
				     synopdecoder/isobaric_surface_potential.h\
				     synopdecoder/nebulosity.h\
				     synopdecoder/parser.cpp\
				     synopdecoder/parser.h\
				     synopdecoder/phenomenon.h\
				     synopdecoder/precipitation_amount.h\
				     synopdecoder/pressure_tendency.h\
				     synopdecoder/range.h\
				     synopdecoder/gust_observation.h\
				     synopdecoder/synop_message.h

meteodata_CPPFLAGS = $(PTHREAD_CFLAGS) $(BOOST_ASIO_CPPFLAGS) $(BOOST_SYSTEM_CPPFLAGS) $(BOOST_PROGRAM_OPTIONS_CPPFLAGS) $(BOOST_PROPERTY_TREE_CPPFLAGS) $(CASSANDRA_CPPFLAGS) $(OPENSSL_INCLUDES)
meteodata_CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic
meteodata_LDFLAGS = $(BOOST_ASIO_LDFLAGS) $(OPENSSL_LDFLAGS)
meteodata_LDADD = $(PTHREAD_CFLAGS) $(BOOST_ASIO_LIBS) $(BOOST_SYSTEM_LIBS) $(BOOST_PROGRAM_OPTIONS_LIBS) $(BOOST_PROPERTY_TREE_LIBS) $(OPENSSL_LIBS)

meteodata_minmax_CPPFLAGS = $(PTHREAD_CFLAGS) $(BOOST_PROGRAM_OPTIONS_CPPFLAGS) $(CASSANDRA_CPPFLAGS)
meteodata_minmax_CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic
meteodata_minmax_LDFLAGS = 
meteodata_minmax_LDADD = $(PTHREAD_CFLAGS) $(BOOST_PROGRAM_OPTIONS_LIBS)

meteodata_month_minmax_CPPFLAGS = $(PTHREAD_CFLAGS) $(BOOST_PROGRAM_OPTIONS_CPPFLAGS) $(CASSANDRA_CPPFLAGS)
meteodata_month_minmax_CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic
meteodata_month_minmax_LDFLAGS = 
meteodata_month_minmax_LDADD = $(PTHREAD_CFLAGS) $(BOOST_PROGRAM_OPTIONS_LIBS)

meteodata_synop_standalone_CPPFLAGS = $(PTHREAD_CFLAGS) $(BOOST_SYSTEM_CPPFLAGS) $(BOOST_PROGRAM_OPTIONS_CPPFLAGS) $(CASSANDRA_CPPFLAGS)
meteodata_synop_standalone_CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic
meteodata_synop_standalone_LDFLAGS = 
meteodata_synop_standalone_LDADD = $(PTHREAD_CFLAGS) $(BOOST_SYSTEM_LIBS) $(BOOST_PROGRAM_OPTIONS_LIBS)
