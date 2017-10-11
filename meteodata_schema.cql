
CREATE KEYSPACE meteodata WITH replication = {'class': 'SimpleStrategy', 'replication_factor': '1'}  AND durable_writes = true;

CREATE TABLE meteodata.stationsfr (
    id uuid PRIMARY KEY,
    date_creation int,
    elevation int,
    icao text,
    idstation text,
    latitude float,
    longitude float,
    name text,
    type int
) WITH bloom_filter_fp_chance = 0.01
    AND caching = '{"keys":"ALL", "rows_per_partition":"NONE"}'
    AND comment = ''
    AND compaction = {'min_threshold': '4', 'class': 'org.apache.cassandra.db.compaction.SizeTieredCompactionStrategy', 'max_threshold': '32'}
    AND compression = {'sstable_compression': 'org.apache.cassandra.io.compress.LZ4Compressor'}
    AND dclocal_read_repair_chance = 0.1
    AND default_time_to_live = 0
    AND gc_grace_seconds = 864000
    AND max_index_interval = 2048
    AND memtable_flush_period_in_ms = 0
    AND min_index_interval = 128
    AND read_repair_chance = 0.0
    AND speculative_retry = '99.0PERCENTILE';
CREATE INDEX stationsfr_icao_idx ON meteodata.stationsfr (icao);
CREATE INDEX stationsfr_idstation_idx ON meteodata.stationsfr (idstation);

CREATE TABLE meteodata.meteo (
    station uuid,
    time timestamp,
    altimeter float,
    avgwindspeed_10min float,
    avgwindspeed_2min float,
    barometer float,
    barometer_abs float,
    barometer_raw float,
    bartrend text,
    dayet float,
    dayrain float,
    dewpoint float,
    extrahum1 int,
    extrahum2 int,
    extrahum3 int,
    extrahum4 int,
    extrahum5 int,
    extrahum6 int,
    extrahum7 int,
    extratemp1 float,
    extratemp2 float,
    extratemp3 float,
    extratemp4 float,
    extratemp5 float,
    extratemp6 float,
    extratemp7 float,
    forecast text,
    forecast_icons int,
    heatindex float,
    icepoint float,
    insidehum int,
    insidetemp float,
    leaftemp1 float,
    leaftemp2 float,
    leaftemp3 float,
    leaftemp4 float,
    leafwetnesses1 int,
    leafwetnesses2 int,
    leafwetnesses3 int,
    leafwetnesses4 int,
    monthet float,
    monthrain float,
    outsidehum int,
    outsidetemp float,
    rain_15min float,
    rain_1h float,
    rain_24h float,
    rainrate float,
    soilmoistures1 int,
    soilmoistures2 int,
    soilmoistures3 int,
    soilmoistures4 int,
    soiltemp1 float,
    soiltemp2 float,
    soiltemp3 float,
    soiltemp4 float,
    solarrad int,
    stormrain float,
    stormstartdate float,
    sunrise time,
    sunset time,
    thswindex float,
    uv int,
    windchill float,
    winddir int,
    windgust_10min float,
    windgustdir float,
    windspeed float,
    yearet float,
    yearrain float,
    PRIMARY KEY (station, time)
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

CREATE TABLE meteodata.datafr (
    station uuid,
    time timestamp,
    barometer float,
    barometer_abs float,
    dewpoint float,
    etat_sol int,
    nebulosity int,
    outsidehum float,
    outsidehummax float,
    outsidehummin float,
    outsidetemp float,
    rain_1h float,
    rain_24h float,
    rayglo float,
    snow int,
    sunshine float,
    t100cm float,
    t10cm float,
    t20cm float,
    t50cm float,
    tn float,
    tx float,
    visibility float,
    weather int,
    windchill float,
    winddir int,
    windgust float,
    windspeed float,
    windspeedmax float,
    windspeedmaxdir int,
    PRIMARY KEY (station, time)
) WITH CLUSTERING ORDER BY (time ASC)
    AND bloom_filter_fp_chance = 0.01
    AND caching = '{"keys":"ALL", "rows_per_partition":"NONE"}'
    AND comment = ''
    AND compaction = {'min_threshold': '4', 'class': 'org.apache.cassandra.db.compaction.SizeTieredCompactionStrategy', 'max_threshold': '32'}
    AND compression = {'sstable_compression': 'org.apache.cassandra.io.compress.LZ4Compressor'}
    AND dclocal_read_repair_chance = 0.1
    AND default_time_to_live = 0
    AND gc_grace_seconds = 864000
    AND max_index_interval = 2048
    AND memtable_flush_period_in_ms = 0
    AND min_index_interval = 128
    AND read_repair_chance = 0.0
    AND speculative_retry = '99.0PERCENTILE';

CREATE TABLE meteodata.coordinates (
    elevation int,
    latitude int,
    longitude int,
    station uuid,
    PRIMARY KEY (elevation, latitude, longitude)
) WITH CLUSTERING ORDER BY (latitude ASC, longitude ASC)
    AND bloom_filter_fp_chance = 0.01
    AND caching = '{"keys":"ALL", "rows_per_partition":"NONE"}'
    AND comment = ''
    AND compaction = {'class': 'org.apache.cassandra.db.compaction.SizeTieredCompactionStrategy'}
    AND compression = {'sstable_compression': 'org.apache.cassandra.io.compress.LZ4Compressor'}
    AND dclocal_read_repair_chance = 0.1
    AND default_time_to_live = 0
    AND gc_grace_seconds = 864000
    AND max_index_interval = 2048
    AND memtable_flush_period_in_ms = 0
    AND min_index_interval = 128
    AND read_repair_chance = 0.0
    AND speculative_retry = '99.0PERCENTILE';

CREATE TABLE meteodata.stations (
    id uuid PRIMARY KEY,
    address text,
    altimeter_setting int,
    barometer_reduction_method int,
    barometric_calibration int,
    barometric_offset int,
    console_battery float,
    elevation int,
    last_archive_download timestamp,
    latitude float,
    longitude float,
    name text,
    polling_period int,
    port int,
    transmitter_battery float
) WITH bloom_filter_fp_chance = 0.01
    AND caching = '{"keys":"ALL", "rows_per_partition":"NONE"}'
    AND comment = ''
    AND compaction = {'min_threshold': '4', 'class': 'org.apache.cassandra.db.compaction.SizeTieredCompactionStrategy', 'max_threshold': '32'}
    AND compression = {'sstable_compression': 'org.apache.cassandra.io.compress.LZ4Compressor'}
    AND dclocal_read_repair_chance = 0.1
    AND default_time_to_live = 0
    AND gc_grace_seconds = 864000
    AND max_index_interval = 2048
    AND memtable_flush_period_in_ms = 0
    AND min_index_interval = 128
    AND read_repair_chance = 0.0
    AND speculative_retry = '99.0PERCENTILE';

