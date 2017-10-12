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
		    vantagepro2message.cpp\
		    vantagepro2archivepage.h\
		    vantagepro2archivepage.cpp\
		    vantagepro2archivemessage.h\
		    vantagepro2archivemessage.cpp\
		    date/src/tz.cpp\
                    date/include/date/date.h\
                    date/include/date/tz.h\
                    date/include/date/tz_private.h\
                    date/include/date/ios.h

meteodata_CPPFLAGS = $(PTHREAD_CFLAGS) $(BOOST_ASIO_CPPFLAGS) $(BOOST_SYSTEM_CPPFLAGS) $(BOOST_PROGRAM_OPTIONS_CPPFLAGS) $(CASSANDRA_CPPFLAGS) -I$(srcdir)/date/include
meteodata_CXXFLAGS = -std=c++14 -Wall -Wextra -pedantic -DUSE_OS_TZDB=1
meteodata_LDFLAGS = $(BOOST_ASIO_LDFLAGS)
meteodata_LDADD = $(PTHREAD_CFLAGS) $(BOOST_ASIO_LIBS) $(BOOST_SYSTEM_LIBS) $(BOOST_PROGRAM_OPTIONS_LIBS)
