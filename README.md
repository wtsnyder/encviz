# ENCVIZ

![build](https://github.com/wtsnyder/encviz/actions/workflows/cmake-build.yml/badge.svg)

## Overview

ENCVIZ is a simple tool for visualization of ENC(S-57) chart data. It includes a
basic WTMS tile server compatible with various GIS and web mapping tools such as
Leaflet.

The application is currently Linux native, and targets an Ubuntu 24.04 Operating
System. Other environments may be added as time and interest permits.

## Helpful References
https://www.teledynecaris.com/s-57/frames/S57catalog.htm

https://nauticalcharts.noaa.gov/publications/docs/us-chart-1/ChartNo1.pdf

This appears broken right now but it is usually great

https://nauticalcharts.noaa.gov/enconline/enconline.html

https://devgis.charttools.noaa.gov/pod/

S-52 Standard

https://iho.int/uploads/user/pubs/standards/s-52/S-52%20Edition%206.1.1%20-%20June%202015.pdf

https://iho.int/uploads/user/pubs/standards/s-52/S-52%20PresLib%20Ed%204.0.3%20Part%20I%20Addendum_Clean.pdf

## Quick Start

1. Install dependencies

```
$ sudo apt install cmake libcairo2-dev libgdal-dev libgtest-dev libmicrohttpd-dev  libtinyxml2-dev librsvg2-dev
```

2. Compile the software

```
~ $ cd encviz
encviz $ mkdir build
encviz $ cd build
build $ cmake ..
build $ make -j$(nproc)
build $ make test
```

3. Obtain a set of ENC(S-57) charts, such as from NOAA ENC Chart Downloader

4. Create a local user configuration and point ENCVIZ at them

```
build $ cd ..
encviz $ rm -rf ~/.encviz
encviz $ cp config ~/.encviz
encviz $ gedit ~/.encviz/config.xml
```

```
  <!-- Location of ENC charts -->
  <chart_path>/path/to/your/ENC_ROOT</chart_path>
```

5. Start the tile server (be patient on first start)

```
encviz $ cd build
build $ ./bin/enc_tile_server
Using config directory: /home/user/.encviz ...
 - Reading /home/user/.encviz/config.xml ...
 - Charts: /home/user/Charts
 - Metadata: /home/user/.encviz/meta
 - Styles: /home/user/.encviz/styles
 - Tile Size: 256
 - Scale Base: 2e+08
3562 charts loaded
```

6. Point your GIS or Web Map application at ENCVIZ

```
http://127.0.0.1:8888/default/{z}/{y}/{x}.png
```

7. Scroll around and enjoy.

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
