Meteodata
=========

Meteodata is a network daemon that regularly collects data from meteorological
stations to store them in a database.

It currently only supports direct connections from Vantage Pro 2® stations, by
Davis Instruments® (http://www.davisnet.com/solution/vantage-pro2/), and uses
Cassandra as storage (http://cassandra.apache.org/).
Stations must connect on the port on which meteodata listens (5886 by default)
and then meteodata starts querying data from them at regular intervals. Since
Vantage Pro 2 stations are not natively equipped to connect to the internet,
serial-to-IP converters can be useful.

Meteodata also downloads data from weatherlink.com (both archive data packets
and realtime data with the v1 Weatherlink API), SYNOP and BUOY data from
ogimet.com, data provided in InfoClimat StatIC files and Météo Bretagne MBData
files.

Alternatively, you may take this program as an example and reuse some headers
and source files. The data structures representing the messages from the Vantage
Pro 2 station as well as the unit conversions can be useful to implement a C++
driver for the station.


Installing
----------

Meteodata is packaged with the autotools so installing can be as simple as

    ./configure; make; make install

The dependencies of this project are :
- a C++14 compiler
- some Boost libraries: Asio, System and Program options, at least version 1.52
- the Cassandra cpp driver version 2
- the Howard Hinnant date library.


Running
-------

Meteodata supports some options:
- `-u | --username`: The username to use to log in to the database
- `-p | --password`: The corresponding password for the database

These first two options can also be set in the configuration file (by default,
`/etc/meteodata/db_credentials`.

- `--help`: Display an help message presenting the available options and exits
- `--version`: Display the version of Meteodata and exit
- `--config-file`: Gives the path to the configuration file
- `--no-daemon | -D`: Normally, Meteodata runs as a daemon and outputs error
messages and warnings to syslog but using this option makes it skip the
daemonization process and outputs debug messages on the standard error output.

