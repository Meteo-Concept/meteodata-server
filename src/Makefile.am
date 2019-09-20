bin_PROGRAMS = meteodata meteodata-minmax meteodata-month-minmax meteodata-synop-standalone meteodata-weatherlink-oldxml-standalone meteodata-weatherlink-apiv2-standalone
meteodata_SOURCES = \
		    connector.cpp\
		    connector.h\
		    daemon.cpp\
		    http_utils.h\
		    meteo_server.cpp\
		    meteo_server.h\
		    time_offseter.cpp\
		    time_offseter.h\
		    davis/abstract_weatherlink_api_message.cpp\
		    davis/abstract_weatherlink_api_message.h\
		    davis/abstract_weatherlink_downloader.h\
		    davis/vantagepro2_archive_message.cpp\
		    davis/vantagepro2_archive_message.h\
		    davis/vantagepro2_archive_page.cpp\
		    davis/vantagepro2_archive_page.h\
		    davis/vantagepro2_connector.cpp\
		    davis/vantagepro2_connector.h\
		    davis/vantagepro2_message.cpp\
		    davis/vantagepro2_message.h\
		    davis/weatherlink_apiv1_realtime_message.cpp\
		    davis/weatherlink_apiv1_realtime_message.h\
		    davis/weatherlink_apiv2_realtime_message.cpp\
		    davis/weatherlink_apiv2_realtime_message.h\
		    davis/weatherlink_apiv2_archive_message.cpp\
		    davis/weatherlink_apiv2_archive_message.h\
		    davis/weatherlink_apiv2_archive_page.cpp\
		    davis/weatherlink_apiv2_archive_page.h\
		    davis/weatherlink_apiv2_downloader.cpp\
		    davis/weatherlink_apiv2_downloader.h\
		    davis/weatherlink_download_scheduler.cpp\
		    davis/weatherlink_download_scheduler.h\
		    davis/weatherlink_downloader.cpp\
		    davis/weatherlink_downloader.h\
		    mbdata/mbdata_messages/abstract_mbdata_message.cpp\
		    mbdata/mbdata_messages/abstract_mbdata_message.h\
		    mbdata/mbdata_messages/mbdata_message_factory.h\
		    mbdata/mbdata_messages/mbdata_meteohub_message.cpp\
		    mbdata/mbdata_messages/mbdata_meteohub_message.h\
		    mbdata/mbdata_messages/mbdata_weathercat_message.cpp\
		    mbdata/mbdata_messages/mbdata_weathercat_message.h\
		    mbdata/mbdata_messages/mbdata_weatherdisplay_message.cpp\
		    mbdata/mbdata_messages/mbdata_weatherdisplay_message.h\
		    mbdata/mbdata_messages/mbdata_weatherlink_message.cpp\
		    mbdata/mbdata_messages/mbdata_weatherlink_message.h\
		    mbdata/mbdata_messages/mbdata_wswin_message.cpp\
		    mbdata/mbdata_messages/mbdata_wswin_message.h\
		    mbdata/mbdata_txt_downloader.cpp\
		    mbdata/mbdata_txt_downloader.h\
		    mqtt/mqtt_subscriber.cpp\
		    mqtt/mqtt_subscriber.h\
		    ship_and_buoy/meteo_france_ship_and_buoy.cpp\
		    ship_and_buoy/meteo_france_ship_and_buoy.h\
		    ship_and_buoy/ship_and_buoy_downloader.cpp\
		    ship_and_buoy/ship_and_buoy_downloader.h\
		    static/static_message.cpp\
		    static/static_message.h\
		    static/static_txt_downloader.cpp\
		    static/static_txt_downloader.h\
		    synop/abstract_synop_downloader.cpp\
		    synop/abstract_synop_downloader.h\
		    synop/deferred_synop_downloader.cpp\
		    synop/deferred_synop_downloader.h\
		    synop/ogimet_synop.cpp\
		    synop/ogimet_synop.h\
		    synop/synop_decoder/cloud_observation.h\
		    synop/synop_decoder/direction.h\
		    synop/synop_decoder/evapo_maybe_transpi_ration.h\
		    synop/synop_decoder/ground_state.h\
		    synop/synop_decoder/gust_observation.h\
		    synop/synop_decoder/isobaric_surface_potential.h\
		    synop/synop_decoder/nebulosity.h\
		    synop/synop_decoder/parser.cpp\
		    synop/synop_decoder/parser.h\
		    synop/synop_decoder/phenomenon.h\
		    synop/synop_decoder/precipitation_amount.h\
		    synop/synop_decoder/pressure_tendency.h\
		    synop/synop_decoder/range.h\
		    synop/synop_decoder/synop_message.h\
		    synop/synop_downloader.cpp\
		    synop/synop_downloader.h

meteodata_minmax_SOURCES = \
		    minmax.cpp

meteodata_month_minmax_SOURCES = \
		    month_minmax.cpp

meteodata_synop_standalone_SOURCES = \
		    synop/synop_standalone.h\
		    synop/synop_standalone.cpp\
		    synop/ogimet_synop.cpp\
		    synop/ogimet_synop.h\
		    synop/synop_decoder/cloud_observation.h\
		    synop/synop_decoder/direction.h\
		    synop/synop_decoder/evapo_maybe_transpi_ration.h\
		    synop/synop_decoder/ground_state.h\
		    synop/synop_decoder/isobaric_surface_potential.h\
		    synop/synop_decoder/nebulosity.h\
		    synop/synop_decoder/parser.cpp\
		    synop/synop_decoder/parser.h\
		    synop/synop_decoder/phenomenon.h\
		    synop/synop_decoder/precipitation_amount.h\
		    synop/synop_decoder/pressure_tendency.h\
		    synop/synop_decoder/range.h\
		    synop/synop_decoder/gust_observation.h\
		    synop/synop_decoder/synop_message.h

meteodata_weatherlink_oldxml_standalone_SOURCES = \
		    time_offseter.cpp\
		    time_offseter.h\
		    cassandra_utils.h\
		    http_utils.h\
		    davis/abstract_weatherlink_api_message.cpp\
		    davis/abstract_weatherlink_api_message.h\
		    davis/abstract_weatherlink_downloader.h\
		    davis/vantagepro2_archive_message.cpp\
		    davis/vantagepro2_archive_message.h\
		    davis/vantagepro2_archive_page.cpp\
		    davis/vantagepro2_archive_page.h\
		    davis/vantagepro2_message.cpp\
		    davis/vantagepro2_message.h\
		    davis/weatherlink_apiv1_realtime_message.cpp\
		    davis/weatherlink_apiv1_realtime_message.h\
		    davis/weatherlink_apiv2_realtime_message.cpp\
		    davis/weatherlink_apiv2_realtime_message.h\
		    davis/weatherlink_apiv2_archive_message.cpp\
		    davis/weatherlink_apiv2_archive_message.h\
		    davis/weatherlink_apiv2_archive_page.cpp\
		    davis/weatherlink_apiv2_archive_page.h\
		    davis/weatherlink_downloader.cpp\
		    davis/weatherlink_downloader.h\
		    davis/weatherlink_downloader_standalone.cpp

meteodata_weatherlink_apiv2_standalone_SOURCES = \
		    time_offseter.cpp\
		    time_offseter.h\
		    cassandra_utils.h\
		    http_utils.h\
		    davis/abstract_weatherlink_api_message.cpp\
		    davis/abstract_weatherlink_api_message.h\
		    davis/abstract_weatherlink_downloader.h\
		    davis/vantagepro2_archive_message.cpp\
		    davis/vantagepro2_archive_message.h\
		    davis/vantagepro2_archive_page.cpp\
		    davis/vantagepro2_archive_page.h\
		    davis/vantagepro2_message.cpp\
		    davis/vantagepro2_message.h\
		    davis/weatherlink_apiv1_realtime_message.cpp\
		    davis/weatherlink_apiv1_realtime_message.h\
		    davis/weatherlink_apiv2_realtime_message.cpp\
		    davis/weatherlink_apiv2_realtime_message.h\
		    davis/weatherlink_apiv2_archive_message.cpp\
		    davis/weatherlink_apiv2_archive_message.h\
		    davis/weatherlink_apiv2_archive_page.cpp\
		    davis/weatherlink_apiv2_archive_page.h\
		    davis/weatherlink_apiv2_downloader.cpp\
		    davis/weatherlink_apiv2_downloader.h\
		    davis/weatherlink_apiv2_downloader_standalone.cpp

meteodata_CPPFLAGS = $(PTHREAD_CFLAGS) $(BOOST_ASIO_CPPFLAGS) $(BOOST_SYSTEM_CPPFLAGS) $(BOOST_PROGRAM_OPTIONS_CPPFLAGS) $(BOOST_PROPERTY_TREE_CPPFLAGS) $(CASSANDRA_CPPFLAGS) $(OPENSSL_INCLUDES) $(LIBCURL_CPPFLAGS)
meteodata_CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic
meteodata_LDFLAGS = $(BOOST_ASIO_LDFLAGS) $(OPENSSL_LDFLAGS)
meteodata_LDADD = $(PTHREAD_CFLAGS) $(BOOST_ASIO_LIBS) $(BOOST_SYSTEM_LIBS) $(BOOST_PROGRAM_OPTIONS_LIBS) $(BOOST_PROPERTY_TREE_LIBS) $(OPENSSL_LIBS) $(LIBCURL) -lcassobs

meteodata_minmax_CPPFLAGS = $(PTHREAD_CFLAGS) $(BOOST_PROGRAM_OPTIONS_CPPFLAGS) $(CASSANDRA_CPPFLAGS)
meteodata_minmax_CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic
meteodata_minmax_LDFLAGS = 
meteodata_minmax_LDADD = $(PTHREAD_CFLAGS) $(BOOST_PROGRAM_OPTIONS_LIBS) -lcassobs

meteodata_month_minmax_CPPFLAGS = $(PTHREAD_CFLAGS) $(BOOST_PROGRAM_OPTIONS_CPPFLAGS) $(CASSANDRA_CPPFLAGS)
meteodata_month_minmax_CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic
meteodata_month_minmax_LDFLAGS = 
meteodata_month_minmax_LDADD = $(PTHREAD_CFLAGS) $(BOOST_PROGRAM_OPTIONS_LIBS) -lcassobs

meteodata_synop_standalone_CPPFLAGS = $(PTHREAD_CFLAGS) $(BOOST_SYSTEM_CPPFLAGS) $(BOOST_PROGRAM_OPTIONS_CPPFLAGS) $(CASSANDRA_CPPFLAGS)
meteodata_synop_standalone_CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic
meteodata_synop_standalone_LDFLAGS = 
meteodata_synop_standalone_LDADD = $(PTHREAD_CFLAGS) $(BOOST_SYSTEM_LIBS) $(BOOST_PROGRAM_OPTIONS_LIBS) -lcassobs

meteodata_weatherlink_oldxml_standalone_CPPFLAGS = $(PTHREAD_CFLAGS) $(BOOST_ASIO_CPPFLAGS) $(BOOST_SYSTEM_CPPFLAGS) $(BOOST_PROGRAM_OPTIONS_CPPFLAGS) $(CASSANDRA_CPPFLAGS) $(OPENSSL_INCLUDES) $(LIBCURL_CPPFLAGS)
meteodata_weatherlink_oldxml_standalone_CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic
meteodata_weatherlink_oldxml_standalone_LDFLAGS = $(BOOST_ASIO_LDFLAGS) $(OPENSSL_LDFLAGS)
meteodata_weatherlink_oldxml_standalone_LDADD = $(PTHREAD_CFLAGS) $(BOOST_ASIO_LIBS) $(BOOST_SYSTEM_LIBS) $(BOOST_PROGRAM_OPTIONS_LIBS) $(OPENSSL_LIBS) $(LIBCURL) -lcassobs

meteodata_weatherlink_apiv2_standalone_CPPFLAGS = $(PTHREAD_CFLAGS) $(BOOST_ASIO_CPPFLAGS) $(BOOST_SYSTEM_CPPFLAGS) $(BOOST_PROGRAM_OPTIONS_CPPFLAGS) $(CASSANDRA_CPPFLAGS) $(OPENSSL_INCLUDES) $(LIBCURL_CPPFLAGS)
meteodata_weatherlink_apiv2_standalone_CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic
meteodata_weatherlink_apiv2_standalone_LDFLAGS = $(BOOST_ASIO_LDFLAGS) $(OPENSSL_LDFLAGS)
meteodata_weatherlink_apiv2_standalone_LDADD = $(PTHREAD_CFLAGS) $(BOOST_ASIO_LIBS) $(BOOST_SYSTEM_LIBS) $(BOOST_PROGRAM_OPTIONS_LIBS) $(OPENSSL_LIBS) $(LIBCURL) -lcassobs
