# ENCVIZ

![build](https://github.com/wtsnyder/encviz/actions/workflows/cmake-build.yml/badge.svg)

## WARNING
**ENCVIZ IS NOT FOR NAVIGATION**

ENCVIZ is not an S-52 conformant ECDIS or commercial chartplotter.
Rendered chart data may be incomplete or incorrect.
Do not rely on it for navigating a vessel.

## Overview

ENCVIZ is a tool for rendering ENC(S-57) chart data. 
It includes a basic WTMS tile server compatible with various GIS and web mapping tools such as Leaflet.
Tiles are rendered dynamically from the S-57 chart files on request.
This keeps storage requirements low and makes updating charts easy.
The rendering of png tiles is much more CPU intensive than serving static png tiles though so it is best suited for applications with only a few clients.

The order and representation of S-57 objects rendered by ENCVIZ is driven by a config file.
Multiple style config files may be defined to provide charts with different day/dusk/night color schemes or different types of data displayed as seperate tile sets from the same WTMS server.
ENCVIZ can not yet render all S-57 objects properly but many of the basics (Land, Shoreline, Lakes/Rivers, Depth Contours, Soundings, Buoys, Channels, Rocks, Wrecks, Obstructions) do work.

A sample tile rendering in Leaflet of Newport Harbor:

![sample](https://github.com/wtsnyder/encviz/raw/master/doc/sample_images/NewportHarbor.png "Rendering Of Newport Harbor")

ENCVIZ is currently Linux native, and targets an Ubuntu 24.04 Operating System.
Instructions to build the WTMS server as a Docker container are below.
See the [docker compose file](https://github.com/wtsnyder/encviz/tree/master/docker/docker-compose.yml) and [sample leaflet webpage](https://github.com/wtsnyder/encviz/tree/master/web/index.html) to get a sample up and running.

## Helpful References

NOAA's S-57 Chart Download Page: https://charts.noaa.gov/ENCs/ENCs.shtml

An excelent browser for the S-57 data schema:
https://www.teledynecaris.com/s-57/frames/S57catalog.htm

Comparison of NOAA and ECDIS symbology:
https://nauticalcharts.noaa.gov/publications/docs/us-chart-1/ChartNo1.pdf

NOAA Browser tools for exploring rendered charts:
https://nauticalcharts.noaa.gov/enconline/enconline.html ,
https://devgis.charttools.noaa.gov/pod/

S-52 (ECDIS) Chart Representation Standards:

https://iho.int/uploads/user/pubs/standards/s-52/S-52%20Edition%206.1.1%20-%20June%202015.pdf

https://iho.int/uploads/user/pubs/standards/s-52/S-52%20PresLib%20Ed%204.0.3%20Part%20I%20Addendum_Clean.pdf

## Quick Start

1. Install dependencies

```
$ sudo apt install cmake libcairo2-dev libgdal-dev libgtest-dev libmicrohttpd-dev  libtinyxml2-dev librsvg2-dev
```

2. Compile the software

```
$ cd encviz
$ mkdir build
$ cd build
$ cmake ..
$ make -j$(nproc)
$ make test
```

3. Obtain a set of ENC(S-57) charts, such as from NOAA ENC Chart Downloader

4. Point ENCVIZ at your chart files

The easiest method is to create a symlink to the charts in the config directory
```
$ cd encviz/config
$ ln -s path/to/dir/with/ENC_ROOT charts
```

You could also edit the path in `encviz/config/config.xml`
```
  <!-- Location of ENC charts -->
  <chart_path>/path/to/your/ENC_ROOT</chart_path>
```

5. Start the tile server with the config file (be patient on first start)

```
$ cd encviz/build
$ ./bin/enc_tile_server -c ../config/config.xml 
Using config file: ../config/config.xml ...
 - Reading ../config/config.xml ...
 - Charts: ../config/charts
 - Metadata: ../config/meta
 - Styles: ../config/styles
 - SVGs: ../config/icons
 - Tile Size: 256
 - Scale Base: 1e+08
Load Chart bounds from cache: "../config/charts/ENC_ROOT/US4NC20M/US4NC20M.000"
.....
Load Chart bounds from cache: "../config/charts/ENC_ROOT/US4AK6RW/US4AK6RW.000"
6598 charts loaded
```

6. Point your GIS or Web Map application at ENCVIZ

```
http://127.0.0.1:8888/default/{z}/{y}/{x}.png
```

7. For a demo using Leaflet
```
$ cd encviz
$ firefox ./web/index.html
```

8. Scroll around and enjoy.

## Docker workflow

1. Setup docker apt repo
```
# Add Docker's official GPG key:
sudo apt-get update
sudo apt-get install ca-certificates curl
sudo install -m 0755 -d /etc/apt/keyrings
sudo curl -fsSL https://download.docker.com/linux/ubuntu/gpg -o /etc/apt/keyrings/docker.asc
sudo chmod a+r /etc/apt/keyrings/docker.asc

# Add the repository to Apt sources:
echo \
  "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.asc] https://download.docker.com/linux/ubuntu \
  $(. /etc/os-release && echo "${UBUNTU_CODENAME:-$VERSION_CODENAME}") stable" | \
  sudo tee /etc/apt/sources.list.d/docker.list > /dev/null
sudo apt-get update
```

2. Install dependencies

```
sudo apt install docker-ce docker-buildx-plugin docker-compose-plugin
```

3. Build the dev container
```
docker buildx build --no-cache --load --file docker/Dockerfile --target encviz-dev -t encviz-dev:latest .
```
Check that you have an `encviz-dev:latest` listed in `docker images`

4. Build encviz using the container
```
docker run -it -v ./:/encviz encviz-dev /bin/bash -c 'cd encviz && mkdir -p build-docker && cd build-docker && cmake .. && make -j'
```

5. Build an encviz container
```
docker buildx build --no-cache --load --file docker/Dockerfile --target encviz -t encviz:latest .
```

6. Run encviz with some charts
```
docker run -it -p 8888:8888 -v /home/will/Documents/charts/02Region_ENCs:/encviz/config/charts encviz
```
