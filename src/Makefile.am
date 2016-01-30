bin_PROGRAMS = meteodata
meteodata_SOURCES = \
		    config.h\
		    daemon.cpp\
		    dbconnection.cpp\
		    dbconnection.h\
		    message.h\
		    vantagepro2connector.cpp\
		    vantagepro2connector.h\
		    station.h\
		    station.cpp\
		    connector.h\
		    connector.cpp

meteodata_CPPFLAGS = $(PTHREAD_CFLAGS) $(BOOST_ASIO_CPPFLAGS) $(BOOST_SYSTEM_CPPFLAGS)
meteodata_CXXFLAGS = -std=c++11
meteodata_LDFLAGS = $(BOOST_ASIO_LDFLAGS)
meteodata_LDADD = -lcassandra $(PTHREAD_CFLAGS) $(BOOST_ASIO_LIBS) $(BOOST_SYSTEM_LIBS)
