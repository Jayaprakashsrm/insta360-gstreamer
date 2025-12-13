# insta360-gstreamer
GStreamer-based streaming tool for Insta360 cameras

## Dependencies
1. [Docker](https://www.docker.com)
2. [Insta360 SDK](https://www.insta360.com/developer/home): Get the SDK by applying in their website. Extract it and move the files to this project as shown below:
    ```
    .
    ├── CMakeLists.txt
    ├── Dockerfile
    ├── include
    │   ├── camera
    │   │   ├── camera.h # <-- Make sure to have this file in place
    │   │   ├── device_discovery.h # <-- Make sure to have this file in place
    │   │   ├── ins_types.h # <-- Make sure to have this file in place
    │   │   └── photography_settings.h # <-- Make sure to have this file in place
    │   └── stream
    │       ├── stream_delegate.h # <-- Make sure to have this file in place
    │       └── stream_types.h # <-- Make sure to have this file in place
    ├── lib
    │   └── libCameraSDK.so # <-- Make sure to have this file in place
    ├── LICENSE
    ├── README.md
    ├── src
    │   ├── config.h
    │   └── streamer.cpp
    ├── start_client.sh
    └── start_server.sh
    ```

## Installation
```
$ cd ~
$ git clone https://github.com/ravijo/insta360-gstreamer.git
$ cd ~/insta360-gstreamer
$ docker build -t insta360-gstreamer .

$ docker run -it --privileged -v ${HOME}/Desktop/insta360-gstreamer:/insta360-gstreamer -v /dev:/dev -p 5004:5004 insta360-gstreamer
$ cmake .. # inside the container
$ make # inside the container
$ ./streamer # inside the container
```

## Receiver side
```
$ gst-launch-1.0 udpsrc address=192.168.0.10 port=5004 caps="application/x-rtp, media=(string)video, encoding-name=(string)RAW, sampling=(string)BGR, width=(string)1152, height=(string)576, framerate=(string)5/1, depth=(string)8, payload=(int)96" ! queue! rtpvrawdepay ! videoconvert ! autovideosink

$ gst-launch-1.0 -v souphttpsrc location=http://172.17.0.2:8080/ ! multipartdemux ! jpegdec ! autovideosink

$ gst-launch-1.0 -v tcpclientsrc host=127.0.0.1 port=5000 ! multipartdemux ! jpegdec ! videoconvert ! autovideosink
gst-launch-1.0 -v tcpclientsrc host=127.0.0.1 port=5000 ! jpegdec ! videoconvert ! identity silent=false ! fakesink sync=true

$ gst-launch-1.0 -v udpsrc port=5000 ! application/x-rtp,encoding-name=H264 ! rtph264depay ! avdec_h264 ! videoconvert ! autovideosink
```
