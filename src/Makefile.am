bin_PROGRAMS = \
	       meteodata\
	       meteodata-minmax \
	       meteodata-minmax-worker \
	       meteodata-month-minmax \
	       meteodata-month-minmax-worker \
	       meteodata-records \
	       meteodata-synop-standalone \
	       meteodata-weatherlink-oldxml-standalone \
	       meteodata-weatherlink-apiv2-standalone \
	       meteodata-weatherlink-apiv2-ingester-standalone \
	       meteodata-weatherlink-apiv2-offload \
	       meteodata-mbdata-standalone \
	       meteodata-mbdata-offload \
	       meteodata-static-standalone \
	       meteodata-static-offload \
	       meteodata-fieldclimate-api-standalone\
	       meteodata-csv-standalone\
	       meteodata-liveobjects-api-standalone\
	       meteodata-cimel-standalone\
	       meteodata-mqtt-vp2-standalone\
	       meteodata-mqtt-chirpstack-receiver-standalone\
	       meteodata-mqtt-payload-ingester-standalone\
	       meteodata-meteofrance-api-standalone\
	       meteodata-meteofrance-all-stations-api-standalone\
	       meteodata-virtual-standalone\
	       meteodata-nbiot-standalone\
	       meteodatactl

meteodata_SOURCES = \
		    connector.cpp\
		    connector.h\
		    connector_group.cpp\
		    connector_group.h\
		    daemon.cpp\
		    http_connection.cpp\
		    http_connection.h\
		    udp_connection.cpp\
		    udp_connection.h\
		    http_utils.h\
		    curl_wrapper.cpp\
		    curl_wrapper.h\
		    meteo_server.cpp\
		    meteo_server.h\
		    rest_web_server.cpp\
		    rest_web_server.h\
		    time_offseter.cpp\
		    time_offseter.h\
		    hex_parser.h\
		    abstract_download_scheduler.cpp\
		    abstract_download_scheduler.h\
		    async_job_publisher.cpp\
		    async_job_publisher.h\
		    cimel/cimel_importer.cpp\
		    cimel/cimel_importer.h\
		    cimel/cimel4A_importer.cpp\
		    cimel/cimel4A_importer.h\
		    cimel/cimel440204_importer.cpp\
		    cimel/cimel440204_importer.h\
		    cimel/cimel_http_request_handler.cpp\
		    cimel/cimel_http_request_handler.h\
		    davis/abstract_weatherlink_api_message.cpp\
		    davis/abstract_weatherlink_api_message.h\
		    davis/abstract_weatherlink_downloader.h\
		    davis/vantagepro2_archive_message.cpp\
		    davis/vantagepro2_archive_message.h\
		    davis/vantagepro2_archive_page.cpp\
		    davis/vantagepro2_archive_page.h\
		    davis/vantagepro2_connector.cpp\
		    davis/vantagepro2_connector.h\
		    davis/vantagepro2_http_request_handler.cpp\
		    davis/vantagepro2_http_request_handler.h\
		    davis/vantagepro2_message.cpp\
		    davis/vantagepro2_message.h\
		    davis/monitorII_http_request_handler.cpp\
		    davis/monitorII_http_request_handler.h\
		    davis/monitorII_archive_entry.cpp\
		    davis/monitorII_archive_entry.h\
		    davis/weatherlink_apiv1_realtime_message.cpp\
		    davis/weatherlink_apiv1_realtime_message.h\
		    davis/weatherlink_apiv2_realtime_message.cpp\
		    davis/weatherlink_apiv2_realtime_message.h\
		    davis/weatherlink_apiv2_realtime_page.cpp\
		    davis/weatherlink_apiv2_realtime_page.h\
		    davis/weatherlink_apiv2_archive_message.cpp\
		    davis/weatherlink_apiv2_archive_message.h\
		    davis/weatherlink_apiv2_archive_page.cpp\
		    davis/weatherlink_apiv2_archive_page.h\
		    davis/weatherlink_apiv2_downloader.cpp\
		    davis/weatherlink_apiv2_downloader.h\
		    davis/weatherlink_apiv2_parser_trait.h\
		    davis/weatherlink_apiv2_download_scheduler.cpp\
		    davis/weatherlink_apiv2_download_scheduler.h\
		    davis/weatherlink_download_scheduler.cpp\
		    davis/weatherlink_download_scheduler.h\
		    davis/weatherlink_downloader.cpp\
		    davis/weatherlink_downloader.h\
		    davis/weatherlink_apiv2_data_structures_parsers/abstract_parser.h\
		    davis/weatherlink_apiv2_data_structures_parsers/parser_factory.cpp\
		    davis/weatherlink_apiv2_data_structures_parsers/parser_factory.h\
		    davis/weatherlink_apiv2_data_structures_parsers/thermohygro_probe_100_parser.cpp\
		    davis/weatherlink_apiv2_data_structures_parsers/thermohygro_probe_100_parser.h\
		    davis/weatherlink_apiv2_data_structures_parsers/soil_probe_108_parser.cpp\
		    davis/weatherlink_apiv2_data_structures_parsers/soil_probe_108_parser.h\
		    davis/weatherlink_apiv2_data_structures_parsers/sentek_probe_116_parser.cpp\
		    davis/weatherlink_apiv2_data_structures_parsers/sentek_probe_116_parser.h\
		    davis/weatherlink_apiv2_data_structures_parsers/davis_transmitter_55_parser.cpp\
		    davis/weatherlink_apiv2_data_structures_parsers/davis_transmitter_55_parser.h\
		    dragino/lsn50v2_thermohygrometer_message.cpp\
		    dragino/lsn50v2_thermohygrometer_message.h\
		    dragino/lsn50v2_probe6470_message.cpp\
		    dragino/lsn50v2_probe6470_message.h\
		    dragino/sn50v3_probe6470_message.cpp\
		    dragino/sn50v3_probe6470_message.h\
		    dragino/llms01_leaf_sensor_message.cpp\
		    dragino/llms01_leaf_sensor_message.h\
		    dragino/lse01_soil_sensor_message.cpp\
		    dragino/lse01_soil_sensor_message.h\
		    dragino/cpl01_pluviometer_message.cpp\
		    dragino/cpl01_pluviometer_message.h\
		    dragino/thpllora_message.cpp\
		    dragino/thpllora_message.h\
		    dragino/thplnbiot_message.cpp\
		    dragino/thplnbiot_message.h\
		    dragino/concept500_message.cpp\
		    dragino/concept500_message.h\
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
		    mbdata/mbdata_messages/mbdata_meteobridge_message.cpp\
		    mbdata/mbdata_messages/mbdata_meteobridge_message.h\
		    mbdata/mbdata_txt_downloader.cpp\
		    mbdata/mbdata_txt_downloader.h\
		    mbdata/mbdata_download_scheduler.cpp\
		    mbdata/mbdata_download_scheduler.h\
		    liveobjects/liveobjects_message.cpp\
		    liveobjects/liveobjects_message.h\
		    liveobjects/liveobjects_http_decoding_request_handler.cpp\
		    liveobjects/liveobjects_http_decoding_request_handler.h\
		    meteo_france/meteo_france_api_download_scheduler.cpp\
		    meteo_france/meteo_france_api_download_scheduler.h\
		    meteo_france/meteo_france_api_downloader.cpp\
		    meteo_france/meteo_france_api_downloader.h\
		    meteo_france/meteo_france_api_bulk_downloader.cpp\
		    meteo_france/meteo_france_api_bulk_downloader.h\
		    meteo_france/meteo_france_api_6m_downloader.cpp\
		    meteo_france/meteo_france_api_6m_downloader.h\
		    meteo_france/mf_radome_message.cpp\
		    meteo_france/mf_radome_message.h\
		    mqtt/generic_message.cpp\
		    mqtt/generic_message.h\
		    mqtt/mqtt_subscriber.cpp\
		    mqtt/mqtt_subscriber.h\
		    mqtt/vp2_mqtt_subscriber.cpp\
		    mqtt/vp2_mqtt_subscriber.h\
		    mqtt/liveobjects_mqtt_subscriber.cpp\
		    mqtt/liveobjects_mqtt_subscriber.h\
		    mqtt/liveobjects_external_mqtt_subscriber.cpp\
		    mqtt/liveobjects_external_mqtt_subscriber.h\
		    mqtt/generic_mqtt_subscriber.cpp\
		    mqtt/generic_mqtt_subscriber.h\
		    mqtt/chirpstack_mqtt_subscriber.cpp\
		    mqtt/chirpstack_mqtt_subscriber.h\
		    nbiot/nbiot_udp_request_handler.cpp\
		    nbiot/nbiot_udp_request_handler.h\
		    ship_and_buoy/meteo_france_ship_and_buoy.cpp\
		    ship_and_buoy/meteo_france_ship_and_buoy.h\
		    ship_and_buoy/ship_and_buoy_downloader.cpp\
		    ship_and_buoy/ship_and_buoy_downloader.h\
		    static/static_message.cpp\
		    static/static_message.h\
		    static/static_txt_downloader.cpp\
		    static/static_txt_downloader.h\
		    static/static_download_scheduler.cpp\
		    static/static_download_scheduler.h\
		    synop/synop_download_scheduler.cpp\
		    synop/synop_download_scheduler.h\
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
		    barani/barani_anemometer_message.cpp\
		    barani/barani_anemometer_message.h\
		    barani/barani_anemometer_2023_message.cpp\
		    barani/barani_anemometer_2023_message.h\
		    barani/barani_meteoag_2022_message.cpp\
		    barani/barani_meteoag_2022_message.h\
		    barani/barani_rain_gauge_message.cpp\
		    barani/barani_rain_gauge_message.h\
		    barani/barani_thermohygro_message.cpp\
		    barani/barani_thermohygro_message.h\
		    custom/thlora_thermohygrometer_message.cpp\
		    custom/thlora_thermohygrometer_message.h\
		    custom/oseren_soil_station_message.cpp\
		    custom/oseren_soil_station_message.h\
		    talkpool/oy1110_thermohygrometer_message.cpp\
		    talkpool/oy1110_thermohygrometer_message.h\
		    pessl/fieldclimate_api_downloader.cpp\
		    pessl/fieldclimate_api_downloader.h\
		    pessl/fieldclimate_api_download_scheduler.cpp\
		    pessl/fieldclimate_api_download_scheduler.h\
		    pessl/fieldclimate_archive_message_collection.cpp\
		    pessl/fieldclimate_archive_message_collection.h\
		    pessl/fieldclimate_archive_message.cpp\
		    pessl/fieldclimate_archive_message.h\
		    pessl/lorain_message.cpp\
		    pessl/lorain_message.h\
		    virtual/virtual_computation_scheduler.cpp\
		    virtual/virtual_computation_scheduler.h\
		    virtual/virtual_obs_computer.cpp\
		    virtual/virtual_obs_computer.h\
		    monitoring/watchdog.h\
		    monitoring/watchdog.cpp\
		    control/control_connector.h\
		    control/control_connector.cpp\
		    control/query_handler.h\
		    control/query_handler.cpp\
		    control/connectors_query_handler.h\
		    control/connectors_query_handler.cpp\
		    control/general_query_handler.h\
		    control/general_query_handler.cpp

meteodata_minmax_SOURCES = \
		    minmax/minmax.cpp\
		    minmax/minmax_computer.cpp\
		    minmax/minmax_computer.h

meteodata_minmax_worker_SOURCES = \
            date_utils.h\
		    minmax/minmax_daemon.cpp\
		    minmax/minmax_worker.cpp\
		    minmax/minmax_worker.h\
		    minmax/minmax_computer.cpp\
		    minmax/minmax_computer.h

meteodata_month_minmax_SOURCES = \
            date_utils.h\
		    month_minmax/month_minmax.cpp\
		    month_minmax/month_minmax_computer.cpp\
		    month_minmax/month_minmax_computer.h

meteodata_month_minmax_worker_SOURCES = \
            date_utils.h\
		    month_minmax/month_minmax_daemon.cpp\
		    month_minmax/month_minmax_worker.cpp\
		    month_minmax/month_minmax_worker.h\
		    month_minmax/month_minmax_computer.cpp\
		    month_minmax/month_minmax_computer.h

meteodata_records_SOURCES = \
		    records.cpp

meteodata_synop_standalone_SOURCES = \
		    time_offseter.cpp\
		    time_offseter.h\
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
		    curl_wrapper.cpp\
		    curl_wrapper.h\
		    async_job_publisher.cpp\
		    async_job_publisher.h\
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
		    davis/weatherlink_apiv2_realtime_page.cpp\
		    davis/weatherlink_apiv2_realtime_page.h\
		    davis/weatherlink_apiv2_archive_message.cpp\
		    davis/weatherlink_apiv2_archive_message.h\
		    davis/weatherlink_apiv2_archive_page.cpp\
		    davis/weatherlink_apiv2_archive_page.h\
		    davis/weatherlink_downloader.cpp\
		    davis/weatherlink_downloader.h\
		    davis/weatherlink_apiv2_data_structures_parsers/abstract_parser.h\
		    davis/weatherlink_apiv2_data_structures_parsers/parser_factory.cpp\
		    davis/weatherlink_apiv2_data_structures_parsers/parser_factory.h\
		    davis/weatherlink_apiv2_data_structures_parsers/thermohygro_probe_100_parser.cpp\
		    davis/weatherlink_apiv2_data_structures_parsers/thermohygro_probe_100_parser.h\
		    davis/weatherlink_apiv2_data_structures_parsers/soil_probe_108_parser.cpp\
		    davis/weatherlink_apiv2_data_structures_parsers/soil_probe_108_parser.h\
		    davis/weatherlink_apiv2_data_structures_parsers/sentek_probe_116_parser.cpp\
		    davis/weatherlink_apiv2_data_structures_parsers/sentek_probe_116_parser.h\
		    davis/weatherlink_apiv2_data_structures_parsers/davis_transmitter_55_parser.cpp\
		    davis/weatherlink_apiv2_data_structures_parsers/davis_transmitter_55_parser.h\
		    davis/weatherlink_downloader_standalone.cpp

meteodata_weatherlink_apiv2_ingester_standalone_SOURCES = \
		    time_offseter.cpp\
		    time_offseter.h\
		    cassandra_utils.h\
		    http_utils.h\
		    curl_wrapper.cpp\
		    curl_wrapper.h\
		    async_job_publisher.cpp\
		    async_job_publisher.h\
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
		    davis/weatherlink_apiv2_realtime_page.cpp\
		    davis/weatherlink_apiv2_realtime_page.h\
		    davis/weatherlink_apiv2_archive_message.cpp\
		    davis/weatherlink_apiv2_archive_message.h\
		    davis/weatherlink_apiv2_archive_page.cpp\
		    davis/weatherlink_apiv2_archive_page.h\
		    davis/weatherlink_apiv2_downloader.cpp\
		    davis/weatherlink_apiv2_downloader.h\
		    davis/weatherlink_apiv2_data_structures_parsers/abstract_parser.h\
		    davis/weatherlink_apiv2_data_structures_parsers/parser_factory.cpp\
		    davis/weatherlink_apiv2_data_structures_parsers/parser_factory.h\
		    davis/weatherlink_apiv2_data_structures_parsers/thermohygro_probe_100_parser.cpp\
		    davis/weatherlink_apiv2_data_structures_parsers/thermohygro_probe_100_parser.h\
		    davis/weatherlink_apiv2_data_structures_parsers/soil_probe_108_parser.cpp\
		    davis/weatherlink_apiv2_data_structures_parsers/soil_probe_108_parser.h\
		    davis/weatherlink_apiv2_data_structures_parsers/sentek_probe_116_parser.cpp\
		    davis/weatherlink_apiv2_data_structures_parsers/sentek_probe_116_parser.h\
		    davis/weatherlink_apiv2_data_structures_parsers/davis_transmitter_55_parser.cpp\
		    davis/weatherlink_apiv2_data_structures_parsers/davis_transmitter_55_parser.h\
		    davis/weatherlink_apiv2_ingester_standalone.cpp

meteodata_weatherlink_apiv2_standalone_SOURCES = \
		    time_offseter.cpp\
		    time_offseter.h\
		    cassandra_utils.h\
		    http_utils.h\
		    curl_wrapper.cpp\
		    curl_wrapper.h\
		    async_job_publisher.cpp\
		    async_job_publisher.h\
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
		    davis/weatherlink_apiv2_realtime_page.cpp\
		    davis/weatherlink_apiv2_realtime_page.h\
		    davis/weatherlink_apiv2_archive_message.cpp\
		    davis/weatherlink_apiv2_archive_message.h\
		    davis/weatherlink_apiv2_archive_page.cpp\
		    davis/weatherlink_apiv2_archive_page.h\
		    davis/weatherlink_apiv2_downloader.cpp\
		    davis/weatherlink_apiv2_downloader.h\
		    davis/weatherlink_apiv2_data_structures_parsers/abstract_parser.h\
		    davis/weatherlink_apiv2_data_structures_parsers/parser_factory.cpp\
		    davis/weatherlink_apiv2_data_structures_parsers/parser_factory.h\
		    davis/weatherlink_apiv2_data_structures_parsers/thermohygro_probe_100_parser.cpp\
		    davis/weatherlink_apiv2_data_structures_parsers/thermohygro_probe_100_parser.h\
		    davis/weatherlink_apiv2_data_structures_parsers/soil_probe_108_parser.cpp\
		    davis/weatherlink_apiv2_data_structures_parsers/soil_probe_108_parser.h\
		    davis/weatherlink_apiv2_data_structures_parsers/sentek_probe_116_parser.cpp\
		    davis/weatherlink_apiv2_data_structures_parsers/sentek_probe_116_parser.h\
		    davis/weatherlink_apiv2_data_structures_parsers/davis_transmitter_55_parser.cpp\
		    davis/weatherlink_apiv2_data_structures_parsers/davis_transmitter_55_parser.h\
		    davis/weatherlink_apiv2_downloader_standalone.cpp

meteodata_weatherlink_apiv2_offload_SOURCES = \
		    time_offseter.cpp\
		    time_offseter.h\
		    cassandra_utils.h\
		    http_utils.h\
		    curl_wrapper.cpp\
		    curl_wrapper.h\
		    async_job_publisher.cpp\
		    async_job_publisher.h\
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
		    davis/weatherlink_apiv2_realtime_page.cpp\
		    davis/weatherlink_apiv2_realtime_page.h\
		    davis/weatherlink_apiv2_archive_message.cpp\
		    davis/weatherlink_apiv2_archive_message.h\
		    davis/weatherlink_apiv2_archive_page.cpp\
		    davis/weatherlink_apiv2_archive_page.h\
		    davis/weatherlink_apiv2_downloader.cpp\
		    davis/weatherlink_apiv2_downloader.h\
		    davis/weatherlink_apiv2_data_structures_parsers/abstract_parser.h\
		    davis/weatherlink_apiv2_data_structures_parsers/parser_factory.cpp\
		    davis/weatherlink_apiv2_data_structures_parsers/parser_factory.h\
		    davis/weatherlink_apiv2_data_structures_parsers/thermohygro_probe_100_parser.cpp\
		    davis/weatherlink_apiv2_data_structures_parsers/thermohygro_probe_100_parser.h\
		    davis/weatherlink_apiv2_data_structures_parsers/soil_probe_108_parser.cpp\
		    davis/weatherlink_apiv2_data_structures_parsers/soil_probe_108_parser.h\
		    davis/weatherlink_apiv2_data_structures_parsers/sentek_probe_116_parser.cpp\
		    davis/weatherlink_apiv2_data_structures_parsers/sentek_probe_116_parser.h\
		    davis/weatherlink_apiv2_data_structures_parsers/davis_transmitter_55_parser.cpp\
		    davis/weatherlink_apiv2_data_structures_parsers/davis_transmitter_55_parser.h\
		    davis/weatherlink_apiv2_downloader_offload.cpp

meteodata_mbdata_standalone_SOURCES = \
		    time_offseter.cpp\
		    time_offseter.h\
		    cassandra_utils.h\
		    curl_wrapper.cpp\
		    curl_wrapper.h\
		    mbdata/mbdata_standalone.cpp\
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
		    mbdata/mbdata_messages/mbdata_meteobridge_message.cpp\
		    mbdata/mbdata_messages/mbdata_meteobridge_message.h\
		    mbdata/mbdata_txt_downloader.cpp\
		    mbdata/mbdata_txt_downloader.h

meteodata_mbdata_offload_SOURCES = \
		    time_offseter.cpp\
		    time_offseter.h\
		    cassandra_utils.h\
		    curl_wrapper.cpp\
		    curl_wrapper.h\
		    mbdata/mbdata_downloader_offload.cpp\
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
		    mbdata/mbdata_messages/mbdata_meteobridge_message.cpp\
		    mbdata/mbdata_messages/mbdata_meteobridge_message.h\
		    mbdata/mbdata_txt_downloader.cpp\
		    mbdata/mbdata_txt_downloader.h

meteodata_static_standalone_SOURCES = \
		    time_offseter.cpp\
		    time_offseter.h\
		    cassandra_utils.h\
		    curl_wrapper.cpp\
		    curl_wrapper.h\
		    static/static_message.cpp\
		    static/static_message.h\
		    static/static_txt_downloader.cpp\
		    static/static_txt_downloader.h\
		    static/static_standalone.cpp

meteodata_static_offload_SOURCES = \
		    time_offseter.cpp\
		    time_offseter.h\
		    cassandra_utils.h\
		    curl_wrapper.cpp\
		    curl_wrapper.h\
		    static/static_message.cpp\
		    static/static_message.h\
		    static/static_txt_downloader.cpp\
		    static/static_txt_downloader.h\
		    static/static_downloader_offload.cpp

meteodata_fieldclimate_api_standalone_SOURCES = \
		    time_offseter.cpp\
		    time_offseter.h\
		    cassandra_utils.h\
		    http_utils.h\
		    curl_wrapper.cpp\
		    curl_wrapper.h\
		    connector.cpp\
		    connector.h\
		    async_job_publisher.cpp\
		    async_job_publisher.h\
		    pessl/fieldclimate_api_downloader_standalone.cpp \
		    pessl/fieldclimate_api_downloader.cpp\
		    pessl/fieldclimate_api_downloader.h\
		    pessl/fieldclimate_archive_message_collection.cpp\
		    pessl/fieldclimate_archive_message_collection.h\
		    pessl/fieldclimate_archive_message.cpp\
		    pessl/fieldclimate_archive_message.h

meteodata_csv_standalone_SOURCES = \
		    time_offseter.cpp\
		    time_offseter.h\
		    davis/csv_importer.h\
		    davis/wlk_message.cpp\
		    davis/wlk_message.h\
		    davis/mileos_message.cpp\
		    davis/mileos_message.h\
		    davis/csv_import_standalone.cpp

meteodata_liveobjects_api_standalone_SOURCES = \
		    time_offseter.cpp\
		    time_offseter.h\
		    cassandra_utils.h\
		    http_utils.h\
		    curl_wrapper.cpp\
		    curl_wrapper.h\
		    async_job_publisher.cpp\
		    async_job_publisher.h\
		    liveobjects/liveobjects_api_downloader_standalone.cpp\
		    liveobjects/liveobjects_api_downloader.cpp\
		    liveobjects/liveobjects_api_downloader.h\
		    liveobjects/liveobjects_message.cpp\
		    liveobjects/liveobjects_message.h\
		    barani/barani_anemometer_message.cpp\
		    barani/barani_anemometer_message.h\
		    barani/barani_anemometer_2023_message.cpp\
		    barani/barani_anemometer_2023_message.h\
		    barani/barani_meteoag_2022_message.cpp\
		    barani/barani_meteoag_2022_message.h\
		    barani/barani_rain_gauge_message.cpp\
		    barani/barani_rain_gauge_message.h\
		    barani/barani_thermohygro_message.cpp\
		    barani/barani_thermohygro_message.h\
		    dragino/lsn50v2_thermohygrometer_message.cpp\
		    dragino/lsn50v2_thermohygrometer_message.h\
		    dragino/lsn50v2_probe6470_message.cpp\
		    dragino/lsn50v2_probe6470_message.h\
		    dragino/sn50v3_probe6470_message.cpp\
		    dragino/sn50v3_probe6470_message.h\
		    dragino/llms01_leaf_sensor_message.cpp\
		    dragino/llms01_leaf_sensor_message.h\
		    dragino/lse01_soil_sensor_message.cpp\
		    dragino/lse01_soil_sensor_message.h\
		    dragino/cpl01_pluviometer_message.cpp\
		    dragino/cpl01_pluviometer_message.h\
		    dragino/thpllora_message.cpp\
		    dragino/thpllora_message.h\
		    dragino/concept500_message.cpp\
		    dragino/concept500_message.h\
		    custom/thlora_thermohygrometer_message.cpp\
		    custom/thlora_thermohygrometer_message.h\
		    custom/oseren_soil_station_message.cpp\
		    custom/oseren_soil_station_message.h\
		    talkpool/oy1110_thermohygrometer_message.cpp\
		    talkpool/oy1110_thermohygrometer_message.h\
		    pessl/lorain_message.cpp\
		    pessl/lorain_message.h

meteodata_cimel_standalone_SOURCES = \
		    time_offseter.cpp\
		    time_offseter.h\
		    hex_parser.h\
		    async_job_publisher.cpp\
		    async_job_publisher.h\
		    cimel/cimel_importer.cpp\
		    cimel/cimel_importer.h\
		    cimel/cimel4A_importer.cpp\
		    cimel/cimel4A_importer.h\
		    cimel/cimel440204_importer.cpp\
		    cimel/cimel440204_importer.h\
		    cimel/cimel_import_standalone.cpp

meteodata_mqtt_vp2_standalone_SOURCES = \
		    cassandra_utils.h\
		    async_job_publisher.cpp\
		    async_job_publisher.h\
		    mqtt/vp2_mqtt_subscriber_standalone.cpp


meteodata_mqtt_chirpstack_receiver_standalone_SOURCES = \
		    async_job_publisher.cpp\
		    async_job_publisher.h\
		    connector.cpp\
		    connector.h\
		    cassandra_utils.h\
		    time_offseter.cpp\
		    time_offseter.h\
		    liveobjects/liveobjects_message.cpp\
		    liveobjects/liveobjects_message.h\
		    barani/barani_anemometer_message.cpp\
		    barani/barani_anemometer_message.h\
		    barani/barani_anemometer_2023_message.cpp\
		    barani/barani_anemometer_2023_message.h\
		    barani/barani_meteoag_2022_message.cpp\
		    barani/barani_meteoag_2022_message.h\
		    barani/barani_rain_gauge_message.cpp\
		    barani/barani_rain_gauge_message.h\
		    barani/barani_thermohygro_message.cpp\
		    barani/barani_thermohygro_message.h\
		    dragino/lsn50v2_thermohygrometer_message.cpp\
		    dragino/lsn50v2_thermohygrometer_message.h\
		    dragino/lsn50v2_probe6470_message.cpp\
		    dragino/lsn50v2_probe6470_message.h\
		    dragino/sn50v3_probe6470_message.cpp\
		    dragino/sn50v3_probe6470_message.h\
		    dragino/llms01_leaf_sensor_message.cpp\
		    dragino/llms01_leaf_sensor_message.h\
		    dragino/lse01_soil_sensor_message.cpp\
		    dragino/lse01_soil_sensor_message.h\
		    dragino/cpl01_pluviometer_message.cpp\
		    dragino/cpl01_pluviometer_message.h\
		    dragino/thpllora_message.cpp\
		    dragino/thpllora_message.h\
		    dragino/concept500_message.cpp\
		    dragino/concept500_message.h\
		    custom/thlora_thermohygrometer_message.cpp\
		    custom/thlora_thermohygrometer_message.h\
		    custom/oseren_soil_station_message.cpp\
		    custom/oseren_soil_station_message.h\
		    talkpool/oy1110_thermohygrometer_message.cpp\
		    talkpool/oy1110_thermohygrometer_message.h\
		    pessl/lorain_message.cpp\
		    pessl/lorain_message.h\
		    mqtt/generic_message.cpp\
		    mqtt/generic_message.h\
		    mqtt/mqtt_subscriber.cpp\
		    mqtt/mqtt_subscriber.h\
		    mqtt/chirpstack_mqtt_subscriber.cpp\
		    mqtt/chirpstack_mqtt_subscriber.h\
		    mqtt/chirpstack_mqtt_subscriber_standalone.cpp

meteodata_mqtt_payload_ingester_standalone_SOURCES = \
		    async_job_publisher.cpp\
		    async_job_publisher.h\
		    connector.cpp\
		    connector.h\
		    cassandra_utils.h\
		    time_offseter.cpp\
		    time_offseter.h\
		    liveobjects/liveobjects_message.cpp\
		    liveobjects/liveobjects_message.h\
		    barani/barani_anemometer_message.cpp\
		    barani/barani_anemometer_message.h\
		    barani/barani_anemometer_2023_message.cpp\
		    barani/barani_anemometer_2023_message.h\
		    barani/barani_meteoag_2022_message.cpp\
		    barani/barani_meteoag_2022_message.h\
		    barani/barani_rain_gauge_message.cpp\
		    barani/barani_rain_gauge_message.h\
		    barani/barani_thermohygro_message.cpp\
		    barani/barani_thermohygro_message.h\
		    dragino/lsn50v2_thermohygrometer_message.cpp\
		    dragino/lsn50v2_thermohygrometer_message.h\
		    dragino/lsn50v2_probe6470_message.cpp\
		    dragino/lsn50v2_probe6470_message.h\
		    dragino/sn50v3_probe6470_message.cpp\
		    dragino/sn50v3_probe6470_message.h\
		    dragino/llms01_leaf_sensor_message.cpp\
		    dragino/llms01_leaf_sensor_message.h\
		    dragino/lse01_soil_sensor_message.cpp\
		    dragino/lse01_soil_sensor_message.h\
		    dragino/cpl01_pluviometer_message.cpp\
		    dragino/cpl01_pluviometer_message.h\
		    dragino/thpllora_message.cpp\
		    dragino/thpllora_message.h\
		    dragino/concept500_message.cpp\
		    dragino/concept500_message.h\
		    custom/thlora_thermohygrometer_message.cpp\
		    custom/thlora_thermohygrometer_message.h\
		    custom/oseren_soil_station_message.cpp\
		    custom/oseren_soil_station_message.h\
		    talkpool/oy1110_thermohygrometer_message.cpp\
		    talkpool/oy1110_thermohygrometer_message.h\
		    pessl/lorain_message.cpp\
		    pessl/lorain_message.h\
		    mqtt/generic_message.cpp\
		    mqtt/generic_message.h\
		    mqtt/mqtt_payload_ingester_standalone.cpp

meteodata_meteofrance_api_standalone_SOURCES = \
		    time_offseter.cpp\
		    time_offseter.h\
		    cassandra_utils.h\
		    http_utils.h\
		    curl_wrapper.cpp\
		    curl_wrapper.h\
		    async_job_publisher.cpp\
		    async_job_publisher.h\
		    meteo_france/meteo_france_api_downloader.cpp\
		    meteo_france/meteo_france_api_downloader.h\
		    meteo_france/mf_radome_message.cpp\
		    meteo_france/mf_radome_message.h\
		    meteo_france/meteo_france_api_downloader_standalone.cpp

meteodata_meteofrance_all_stations_api_standalone_SOURCES = \
		    time_offseter.cpp\
		    time_offseter.h\
		    cassandra_utils.h\
		    http_utils.h\
		    curl_wrapper.cpp\
		    curl_wrapper.h\
		    async_job_publisher.cpp\
		    async_job_publisher.h\
		    meteo_france/meteo_france_api_6m_downloader.cpp\
		    meteo_france/meteo_france_api_6m_downloader.h\
		    meteo_france/meteo_france_api_downloader.h\
		    meteo_france/mf_radome_message.cpp\
		    meteo_france/mf_radome_message.h\
		    meteo_france/meteo_france_api_all_stations_downloader_standalone.cpp

meteodata_virtual_standalone_SOURCES = \
		    async_job_publisher.cpp\
		    async_job_publisher.h\
		    virtual/virtual_obs_computer.cpp\
		    virtual/virtual_obs_computer.h\
		    virtual/virtual_obs_computer_standalone.cpp

meteodata_nbiot_standalone_SOURCES = \
		    cassandra_utils.h\
		    hex_parser.h\
		    http_utils.h\
		    async_job_publisher.cpp\
		    async_job_publisher.h\
		    udp_connection.h\
		    dragino/thplnbiot_message.cpp\
		    dragino/thplnbiot_message.h\
		    nbiot/nbiot_udp_request_handler.cpp\
		    nbiot/nbiot_udp_request_handler.h\
		    nbiot/nbiot_standalone.cpp

meteodatactl_SOURCES = \
		    control/meteodatactl.cpp

basecppflags = $(PTHREAD_CFLAGS) $(BOOST_ASIO_CPPFLAGS) $(BOOST_PROGRAM_OPTIONS_CPPFLAGS) $(BOOST_JSON_CPPFLAGS) $(OPENSSL_INCLUDES) $(LIBCURL_CPPFLAGS) $(CASSANDRA_CFLAGS) $(DATE_CFLAGS) $(MYSQL_CFLAGS) $(CASSOBS_CFLAGS) $(POSTGRES_CFLAGS)
allcppflags = $(basecppflags) $(BOOST_SYSTEM_CPPFLAGS) $(BOOST_PROPERTY_TREE_CPPFLAGS) $(BOOST_BEAST_CPPFLAGS) $(SYSTEMD_CFLAGS)

baseldflags = $(BOOST_ASIO_LDFLAGS) $(BOOST_SYSTEM_LDFLAGS) $(BOOST_PROGRAM_OPTIONS_LDFLAGS) $(OPENSSL_LDFLAGS) $(CASSOBS_LDFLAGS) $(MYSQL_LDFLAGS) $(CASSANDRA_LDFLAGS) $(DATE_LDFLAGS) $(POSTGRES_LDFLAGS)
allldflags = $(baseldflags) $(BOOST_SYSTEM_LDFLAGS) $(BOOST_PROPERTY_TREE_LDFLAGS) $(SYSTEMD_LDFLAGS)

baselibs = $(PTHREAD_CFLAGS) $(BOOST_ASIO_LIBS) $(BOOST_PROGRAM_OPTIONS_LIBS) $(BOOST_SYSTEM_LIBS) $(OPENSSL_LIBS) $(LIBCURL) $(CASSOBS_LIBS) $(MYSQL_LIBS) $(CASSANDRA_LIBS) $(DATE_LIBS) $(POSTGRES_LIBS)
alllibs = $(baselibs) $(BOOST_SYSTEM_LIBS) $(BOOST_PROPERTY_TREE_LIBS) $(SYSTEMD_LIBS)

meteodata_CPPFLAGS = $(allcppflags)
meteodata_CXXFLAGS =
meteodata_LDFLAGS = $(allldflags)
meteodata_LDADD = $(alllibs)

meteodata_minmax_CPPFLAGS = $(basecppflags)
meteodata_minmax_CXXFLAGS =
meteodata_minmax_LDFLAGS = $(baseldflags)
meteodata_minmax_LDADD = $(baselibs)

meteodata_minmax_worker_CPPFLAGS = $(allcppflags)
meteodata_minmax_worker_CXXFLAGS =
meteodata_minmax_worker_LDFLAGS = $(allldflags)
meteodata_minmax_worker_LDADD = $(alllibs)

meteodata_month_minmax_CPPFLAGS = $(basecppflags)
meteodata_month_minmax_CXXFLAGS =
meteodata_month_minmax_LDFLAGS = $(baseldflags)
meteodata_month_minmax_LDADD = $(baselibs)

meteodata_month_minmax_worker_CPPFLAGS = $(allcppflags)
meteodata_month_minmax_worker_CXXFLAGS =
meteodata_month_minmax_worker_LDFLAGS = $(allldflags)
meteodata_month_minmax_worker_LDADD = $(alllibs)

meteodata_records_CPPFLAGS = $(basecppflags)
meteodata_records_CXXFLAGS =
meteodata_records_LDFLAGS = $(baseldflags)
meteodata_records_LDADD = $(baselibs)

meteodata_synop_standalone_CPPFLAGS = $(basecppflags)
meteodata_synop_standalone_CXXFLAGS =
meteodata_synop_standalone_LDFLAGS = $(baseldflags)
meteodata_synop_standalone_LDADD = $(baselibs)

meteodata_weatherlink_oldxml_standalone_CPPFLAGS = $(basecppflags)
meteodata_weatherlink_oldxml_standalone_CXXFLAGS =
meteodata_weatherlink_oldxml_standalone_LDFLAGS = $(baseldflags)
meteodata_weatherlink_oldxml_standalone_LDADD = $(baselibs)

meteodata_weatherlink_apiv2_ingester_standalone_CPPFLAGS = $(basecppflags)
meteodata_weatherlink_apiv2_ingester_standalone_CXXFLAGS =
meteodata_weatherlink_apiv2_ingester_standalone_LDFLAGS = $(baseldflags)
meteodata_weatherlink_apiv2_ingester_standalone_LDADD = $(baselibs)

meteodata_weatherlink_apiv2_standalone_CPPFLAGS = $(basecppflags)
meteodata_weatherlink_apiv2_standalone_CXXFLAGS =
meteodata_weatherlink_apiv2_standalone_LDFLAGS = $(baseldflags)
meteodata_weatherlink_apiv2_standalone_LDADD = $(baselibs)

meteodata_weatherlink_apiv2_offload_CPPFLAGS = $(basecppflags)
meteodata_weatherlink_apiv2_offload_CXXFLAGS =
meteodata_weatherlink_apiv2_offload_LDFLAGS = $(baseldflags)
meteodata_weatherlink_apiv2_offload_LDADD = $(baselibs)

meteodata_mbdata_standalone_CPPFLAGS = $(basecppflags)
meteodata_mbdata_standalone_CXXFLAGS =
meteodata_mbdata_standalone_LDFLAGS = $(baseldflags)
meteodata_mbdata_standalone_LDADD = $(baselibs)

meteodata_mbdata_offload_CPPFLAGS = $(basecppflags)
meteodata_mbdata_offload_CXXFLAGS =
meteodata_mbdata_offload_LDFLAGS = $(baseldflags)
meteodata_mbdata_offload_LDADD = $(baselibs)

meteodata_static_standalone_CPPFLAGS = $(basecppflags)
meteodata_static_standalone_CXXFLAGS =
meteodata_static_standalone_LDFLAGS = $(baseldflags)
meteodata_static_standalone_LDADD = $(baselibs)

meteodata_static_offload_CPPFLAGS = $(basecppflags)
meteodata_static_offload_CXXFLAGS =
meteodata_static_offload_LDFLAGS = $(baseldflags)
meteodata_static_offload_LDADD = $(baselibs)

meteodata_fieldclimate_api_standalone_CPPFLAGS = $(basecppflags)
meteodata_fieldclimate_api_standalone_CXXFLAGS =
meteodata_fieldclimate_api_standalone_LDFLAGS = $(baseldflags)
meteodata_fieldclimate_api_standalone_LDADD = $(baselibs)

meteodata_csv_standalone_CPPFLAGS = $(basecppflags)
meteodata_csv_standalone_CXXFLAGS =
meteodata_csv_standalone_LDFLAGS = $(baseldflags)
meteodata_csv_standalone_LDADD = $(baselibs)

meteodata_liveobjects_api_standalone_CPPFLAGS = $(basecppflags)
meteodata_liveobjects_api_standalone_CXXFLAGS =
meteodata_liveobjects_api_standalone_LDFLAGS = $(baseldflags)
meteodata_liveobjects_api_standalone_LDADD = $(baselibs)

meteodata_cimel_standalone_CPPFLAGS = $(basecppflags)
meteodata_cimel_standalone_CXXFLAGS =
meteodata_cimel_standalone_LDFLAGS = $(baseldflags)
meteodata_cimel_standalone_LDADD = $(baselibs)

meteodata_mqtt_vp2_standalone_CPPFLAGS = $(basecppflags) $(BOOST_SYSTEM_CPPFLAGS)
meteodata_mqtt_vp2_standalone_CXXFLAGS =
meteodata_mqtt_vp2_standalone_LDFLAGS = $(baseldflags) $(BOOST_SYSTEM_LDFLAGS)
meteodata_mqtt_vp2_standalone_LDADD = $(baselibs) $(BOOST_SYSTEM_LIBS)

meteodata_mqtt_chirpstack_receiver_standalone_CPPFLAGS = $(basecppflags)
meteodata_mqtt_chirpstack_receiver_standalone_CXXFLAGS =
meteodata_mqtt_chirpstack_receiver_standalone_LDFLAGS = $(baseldflags)
meteodata_mqtt_chirpstack_receiver_standalone_LDADD = $(baselibs)

meteodata_mqtt_payload_ingester_standalone_CPPFLAGS = $(basecppflags)
meteodata_mqtt_payload_ingester_standalone_CXXFLAGS =
meteodata_mqtt_payload_ingester_standalone_LDFLAGS = $(baseldflags)
meteodata_mqtt_payload_ingester_standalone_LDADD = $(baselibs)

meteodata_meteofrance_api_standalone_CPPFLAGS = $(basecppflags)
meteodata_meteofrance_api_standalone_CXXFLAGS =
meteodata_meteofrance_api_standalone_LDFLAGS = $(baseldflags)
meteodata_meteofrance_api_standalone_LDADD = $(baselibs)

meteodata_meteofrance_all_stations_api_standalone_CPPFLAGS = $(basecppflags)
meteodata_meteofrance_all_stations_api_standalone_CXXFLAGS =
meteodata_meteofrance_all_stations_api_standalone_LDFLAGS = $(baseldflags)
meteodata_meteofrance_all_stations_api_standalone_LDADD = $(baselibs)

meteodata_virtual_standalone_CPPFLAGS = $(basecppflags)
meteodata_virtual_standalone_CXXFLAGS =
meteodata_virtual_standalone_LDFLAGS = $(baseldflags)
meteodata_virtual_standalone_LDADD = $(baselibs)

meteodata_nbiot_standalone_CPPFLAGS = $(basecppflags)
meteodata_nbiot_standalone_CXXFLAGS =
meteodata_nbiot_standalone_LDFLAGS = $(baseldflags)
meteodata_nbiot_standalone_LDADD = $(baselibs)

meteodatactl_CPPFLAGS = $(PTHREAD_CFLAGS) $(BOOST_ASIO_CPPFLAGS) $(BOOST_PROGRAM_OPTIONS_CPPFLAGS) $(DATE_CFLAGS)
meteodatactl_CXXFLAGS =
meteodatactl_LDFLAGS = $(PTHREAD_LDFLAGS) $(BOOST_ASIO_LDFLAGS) $(BOOST_PROGRAM_OPTIONS_LDFLAGS) $(DATE_LDFLAGS)
meteodatactl_LDADD = $(PTHREAD_LIBS) $(BOOST_ASIO_LIBS) $(BOOST_PROGRAM_OPTIONS_LIBS) $(DATE_LIBS)
