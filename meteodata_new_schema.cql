CREATE TABLE meteodata.meteo_new (
    station uuid,
    date date,
    time timestamp,
    barometer float,
    dewpoint float,
    extrahum1 int,
    extrahum2 int,
    extratemp1 float,
    extratemp2 float,
    extratemp3 float,
    heatindex float,
    icepoint float,
    insidehum int,
    insidetemp float,
    leaftemp1 float,
    leaftemp2 float,
    leafwetnesses1 int,
    leafwetnesses2 int,
    outsidehum int,
    outsidetemp float,
    rainrate float,
    rainfall float,
    et float,
    soilmoistures1 int,
    soilmoistures2 int,
    soilmoistures3 int,
    soilmoistures4 int,
    soiltemp1 float,
    soiltemp2 float,
    soiltemp3 float,
    soiltemp4 float,
    solarrad int,
    thswindex float,
    uv int,
    windchill float,
    winddir int,
    windgust float,
    windgustdir float,
    windspeed float,
    PRIMARY KEY ((station, date), time)
) WITH CLUSTERING ORDER BY (time DESC)
    AND bloom_filter_fp_chance = 0.01
    AND caching = '{"keys":"ALL", "rows_per_partition":"NONE"}'
    AND comment = ''
    AND compaction = {'class': 'org.apache.cassandra.db.compaction.SizeTieredCompactionStrategy'}
    AND compression = {'sstable_compression': 'org.apache.cassandra.io.compress.LZ4Compressor'}
    AND dclocal_read_repair_chance = 0.1
    AND default_time_to_live = 0
    AND gc_grace_seconds = 0
    AND max_index_interval = 2048
    AND memtable_flush_period_in_ms = 0
    AND min_index_interval = 128
    AND read_repair_chance = 0.0
    AND speculative_retry = '99.0PERCENTILE';

CREATE TABLE meteodata.minmax_new (
    station uuid,
    date date,
    barometer_min float,
    barometer_max float,
    barometer_avg float,
    dewpoint float,
    extrahum1_min int,
    extrahum1_max int,
    extrahum1_avg int,
    extrahum2_min int,
    extrahum2_max int,
    extrahum2_avg int,
    extratemp1_min float,
    extratemp1_max float,
    extratemp1_avg float,
    extratemp2_min float,
    extratemp2_max float,
    extratemp2_avg float,
    extratemp3_min float,
    extratemp3_max float,
    extratemp3_avg float,
    insidehum_min int,
    insidehum_max int,
    insidehum_avg int,
    insidetemp_min float,
    insidetemp_max float,
    insidetemp_avg float,
    leaftemp1_min float,
    leaftemp1_max float,
    leaftemp1_avg float,
    leaftemp2_min float,
    leaftemp2_max float,
    leaftemp2_avg float,
    leafwetnesses1_min int,
    leafwetnesses1_max int,
    leafwetnesses1_avg int,
    leafwetnesses2_min int,
    leafwetnesses2_max int,
    leafwetnesses2_avg int,
    outsidehum_min int,
    outsidehum_max int,
    outsidehum_avg int,
    outsidetemp_min float,
    outsidetemp_max float,
    outsidetemp_avg float,
    rainrate_max float,
    soilmoistures1_min int,
    soilmoistures1_max int,
    soilmoistures1_avg int,
    soilmoistures2_min int,
    soilmoistures2_max int,
    soilmoistures2_avg int,
    soilmoistures3_min int,
    soilmoistures3_max int,
    soilmoistures3_avg int,
    soilmoistures4_min int,
    soilmoistures4_max int,
    soilmoistures4_avg int,
    soiltemp1_min float,
    soiltemp1_max float,
    soiltemp1_avg float,
    soiltemp2_min float,
    soiltemp2_max float,
    soiltemp2_avg float,
    soiltemp3_min float,
    soiltemp3_max float,
    soiltemp3_avg float,
    soiltemp4_min float,
    soiltemp4_max float,
    soiltemp4_avg float,
    solarrad_max int,
    solarrad_mean int,
    uv_max int,
    uv_avg int,
    winddir int,
    windgust_max float,
    windgust_avg float,
    windgustdir float,
    windspeed_max float,
    windspeed_avg float,
    dayet float,
    monthet float,
    yearet float,
    dayrain float,
    monthrain float,
    yearrain float,
    PRIMARY KEY ((station, monthyear), date)
) WITH CLUSTERING ORDER BY (date DESC)
    AND bloom_filter_fp_chance = 0.01
    AND caching = '{"keys":"ALL", "rows_per_partition":"NONE"}'
    AND comment = ''
    AND compaction = {'class': 'org.apache.cassandra.db.compaction.SizeTieredCompactionStrategy'}
    AND compression = {'sstable_compression': 'org.apache.cassandra.io.compress.LZ4Compressor'}
    AND dclocal_read_repair_chance = 0.1
    AND default_time_to_live = 0
    AND gc_grace_seconds = 0
    AND max_index_interval = 2048
    AND memtable_flush_period_in_ms = 0
    AND min_index_interval = 128
    AND read_repair_chance = 0.0
    AND speculative_retry = '99.0PERCENTILE';
