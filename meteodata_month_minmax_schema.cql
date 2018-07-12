DROP TABLE meteodata_v2.month_minmax;

CREATE TABLE meteodata_v2.month_minmax (
    station uuid,
    year int,
    month int,
    outsidetemp float,
    outsidetemp_max_max float,
    outsidetemp_max_min float,
    outsidetemp_min_max float,
    outsidetemp_min_min float,
    barometer_min float,
    barometer_max float,
    barometer_avg float,
    windgust_speed_max float,
    winddir list<int>,
    rainfall float,
    rainrate_max float,
    etp float,
    outsidehum_min int,
    outsidehum_max int,
    solarrad_min float,
    solarrad float,
    uv_max int,
    PRIMARY KEY ((station, year), month)
) WITH  bloom_filter_fp_chance = 0.01
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
