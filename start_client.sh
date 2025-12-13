#!/bin/bash
# author: ravi joshi
# date: 13 dec 2025

# sudo sysctl -w net.core.rmem_max=10000000

gst-launch-1.0 udpsrc address=127.0.0.1 port=5004 buffer-size=1000000 caps="application/x-rtp, media=(string)video, clock-rate=90000, encoding-name=(string)RAW, sampling=(string)BGR, width=(string)1152, height=(string)576, framerate=(string)5/1, depth=(string)8, payload=(int)96" ! rtpvrawdepay ! videoconvert ! autovideosink

