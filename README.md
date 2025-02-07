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
Météo France, data provided in InfoClimat StatIC files and Météo Bretagne MBData
files. It has various MQTT connectors for IoT devices, it can query Orange
Liveobjects® and Bouygues Telecom Objenious® platforms (although the latter is
no longer available for LoRa devices). It has a HTTP server to receive data
from Vantage Pro 2 and Weather Monitor II stations, as well as decoding messages
from Liveobjects. Finally, there's also a UDP server in there for Dragino NB-IoT
nodes.

It's unlikely this project is useful to you as-is, but you may reuse some
headers and parts here and there. The data structures representing the messages
from the Vantage Pro 2 station as well as the unit conversions can be useful to
implement a C++ driver for the station.


Installing
----------

Meteodata is packaged with the autotools so installing can be as simple as

    ./configure; make; make install

The dependencies of this project are :
- a C++17 compiler
- some Boost libraries: Asio, System and Program options, at least version 1.81
- the cassobs-lib (https://github.com/Meteo-Concept/cassobs-lib)
- the Howard Hinnant date library.


Running
-------

Meteodata supports some options that can be set in the configuration file (by
default, `/etc/meteodata/db_credentials`.  Use option --help for a description.

Other relevant command-line options:
- `--version`: Display the version of Meteodata and exit
- `--config-file`: Gives the path to the configuration file
- `--no-daemon | -D`: Normally, Meteodata runs as a systemd daemon (with a
  watchdog, among other things) but using this option prevents it from sending
  notifications to systemd.

