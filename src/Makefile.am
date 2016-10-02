bin_PROGRAMS = meteodata
meteodata_SOURCES = \
		    daemon.cpp\
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
		    vantagepro2message.cpp

meteodata_CPPFLAGS = $(PTHREAD_CFLAGS) $(BOOST_ASIO_CPPFLAGS) $(BOOST_SYSTEM_CPPFLAGS) $(BOOST_PROGRAM_OPTIONS_CPPFLAGS)
meteodata_CXXFLAGS = -std=c++11
meteodata_LDFLAGS = $(BOOST_ASIO_LDFLAGS)
meteodata_LDADD = -lcassandra $(PTHREAD_CFLAGS) $(BOOST_ASIO_LIBS) $(BOOST_SYSTEM_LIBS) $(BOOST_PROGRAM_OPTIONS_LIBS)
