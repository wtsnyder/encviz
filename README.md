# ENCVIZ

## Overview

ENCVIZ is a simple tool for visualization of ENC(S-57) chart data. It includes a
basic WTMS tile server compatible with various GIS and web mapping tools such as
Leaflet.

The application is currently Linux native, and targets an Ubuntu 24.04 Operating
System. Other environments may be added as time and interest permits.

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
