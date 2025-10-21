
# Development environment container
FROM ubuntu:24.04 as encviz-dev

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libcairo2-dev \
    libgdal-dev \
    libgtest-dev \
    libmicrohttpd-dev \
    libtinyxml2-dev \
    librsvg2-dev \
    # Clean up apt caches to reduce image size
    && rm -rf /var/lib/apt/lists/*


# Encviz Server Container
FROM ubuntu:24.04 as encviz

# Install dependencies
RUN apt-get update && apt-get install -y \
    libtinyxml2-10 \
    libgdal34t64 \
    libcairo2 \
    librsvg2-2 \
    libmicrohttpd12t64 \
    && mkdir encviz \
    # Clean up apt caches
    && rm -rf /var/lib/apt/lists/*

WORKDIR encviz

# Copy the executable into the container
COPY build-docker/bin/enc_tile_server .
COPY config ./config

# Create an empty charts volume mount point
RUN rm -rf config/charts && mkdir -p config/charts

ENTRYPOINT ["./enc_tile_server", "-c", "./config/config.xml"]