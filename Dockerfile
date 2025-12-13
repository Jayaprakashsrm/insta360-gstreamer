# syntax=docker/dockerfile:1

FROM ubuntu:20.04

# Disable interactive prompts during build
ENV DEBIAN_FRONTEND=noninteractive

# Install dependencies
RUN apt-get update && \
    apt-get install -y \
        ca-certificates \
        wget \
        build-essential \
        cmake \
        nano \
        net-tools \
        \
        # USB / udev
        libusb-dev \
        libudev-dev \
        \
        # fmt
        libfmt-dev \
        \
        # OpenCV
        libopencv-dev \
        \
        # FFmpeg / Libav
        libavformat-dev \
        libavcodec-dev \
        libswscale-dev \
        \
        # GStreamer core
        libgstreamer1.0-dev \
        libgstreamer-plugins-base1.0-dev \
        \
        # GStreamer plugins
        gstreamer1.0-plugins-base \
        gstreamer1.0-plugins-good \
        gstreamer1.0-plugins-bad \
        gstreamer1.0-plugins-ugly \
        gstreamer1.0-libav \
        gstreamer1.0-tools \
        gstreamer1.0-x \
        gstreamer1.0-alsa \
        gstreamer1.0-gl \
        gstreamer1.0-gtk3 \
        gstreamer1.0-qt5 \
        gstreamer1.0-pulseaudio \
    && rm -rf /var/lib/apt/lists/*

# Copy project
COPY . /insta360-gstreamer

# Build directory
RUN mkdir -p /insta360-gstreamer/build

# Switch to build directory
WORKDIR /insta360-gstreamer/build

# Compile project
# RUN cmake .. && make -j1

# Run streamer
# CMD ["./streamer"]