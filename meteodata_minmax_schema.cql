CREATE TABLE meteodata_v2.minmax (
    station uuid,
    monthyear int,
    day date,
    barometer_avg float,
    barometer_max float,
    barometer_min float,
    dayet float,
    dayrain float,
    dewpoint_avg float,
    dewpoint_max float,
    extrahum1_avg int,
    extrahum1_max int,
    extrahum1_min int,
    extrahum2_avg int,
    extrahum2_max int,
    extrahum2_min int,
    extratemp1_avg float,
    extratemp1_max float,
    extratemp1_min float,
    extratemp2_avg float,
    extratemp2_max float,
    extratemp2_min float,
    extratemp3_avg float,
    extratemp3_max float,
    extratemp3_min float,
    insidehum_avg int,
    insidehum_max int,
    insidehum_min int,
    insidetemp_avg float,
    insidetemp_max float,
    insidetemp_min float,
    leaftemp1_avg float,
    leaftemp1_max float,
    leaftemp1_min float,
    leaftemp2_avg float,
    leaftemp2_max float,
    leaftemp2_min float,
    leafwetnesses1_avg int,
    leafwetnesses1_max int,
    leafwetnesses1_min int,
    leafwetnesses2_avg int,
    leafwetnesses2_max int,
    leafwetnesses2_min int,
    monthet float,
    monthrain float,
    outsidehum_avg int,
    outsidehum_max int,
    outsidehum_min int,
    outsidetemp_avg float,
    outsidetemp_max float,
    outsidetemp_min float,
    rainrate_max float,
    soilmoistures1_avg int,
    soilmoistures1_max int,
    soilmoistures1_min int,
    soilmoistures2_avg int,
    soilmoistures2_max int,
    soilmoistures2_min int,
    soilmoistures3_avg int,
    soilmoistures3_max int,
    soilmoistures3_min int,
    soilmoistures4_avg int,
    soilmoistures4_max int,
    soilmoistures4_min int,
    soiltemp1_avg float,
    soiltemp1_max float,
    soiltemp1_min float,
    soiltemp2_avg float,
    soiltemp2_max float,
    soiltemp2_min float,
    soiltemp3_avg float,
    soiltemp3_max float,
    soiltemp3_min float,
    soiltemp4_avg float,
    soiltemp4_max float,
    soiltemp4_min float,
    solarrad_avg int,
    solarrad_max int,
    uv_avg int,
    uv_max int,
    winddir list<int>,
    windgust_avg float,
    windgust_max float,
    windspeed_avg float,
    windspeed_max float,
    yearet float,
    yearrain float,
    PRIMARY KEY ((station, monthyear), day)
) WITH CLUSTERING ORDER BY (day ASC);
