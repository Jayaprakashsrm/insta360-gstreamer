// Standard CPP headers
#include <iostream>
#include <thread>

// For string formatting
#include <fmt/core.h>

// Insta 360 SDK
#include <camera/camera.h>
#include <camera/device_discovery.h>
#include <camera/photography_settings.h>

// OpenCV headers
// TODO: Can we do everything on GStreamer instead?
#include <opencv2/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>

// GStreamer and LibAVCodec headers
extern "C"
{
#include <gst/app/gstappsrc.h>
#include <gst/gst.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

// RECEIVER_IP and IMG_DOWNSCALE are written here
#include "config.h"

class CameraStreamDelegate : public ins_camera::StreamDelegate
{

public:
  CameraStreamDelegate() : receiver_ip_(RECEIVER_IP), fps_(5), img_downscale_(IMG_DOWNSCALE)
  {
    // TODO: Read it from camera instead
    const int width_org = 2304;
    const int height_org = 1152;

    width_ = width_org / img_downscale_;
    height_ = height_org / img_downscale_;

    std::string pipeline = fmt::format(
        "appsrc name=mysource ! queue ! "
        "video/x-raw,format=BGR,width={},height={} ! queue ! "
        "rtpvrawpay ! udpsink auto-multicast=0 host={} port=5004 ",
        width_, height_, receiver_ip_);

    std::cout << "receiver_ip_: " << receiver_ip_ << " img_downscale_: " << img_downscale_ << std::endl;

    // GStreamer pipeline using appsink and udpsink
    pipeline_ = gst_parse_launch(pipeline.c_str(), nullptr);

    if (!pipeline_)
    {
      throw std::runtime_error("Failed to create GStreamer pipeline.");
    }

    // Get the appsrc element
    appsrc_ = gst_bin_get_by_name(GST_BIN(pipeline_), "mysource");
    if (!appsrc_)
    {
      throw std::runtime_error("Failed to get appsrc element.");
    }

    // Configure appsrc
    g_object_set(G_OBJECT(appsrc_), "caps",
                 gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING,
                                     "BGR", "width", G_TYPE_INT, width_,
                                     "height", G_TYPE_INT, height_, "framerate",
                                     GST_TYPE_FRACTION, fps_, 1, nullptr),
                 nullptr);
    g_object_set(G_OBJECT(appsrc_), "block", TRUE, nullptr);
    gst_element_set_state(pipeline_, GST_STATE_PLAYING);

    // Find the decoder for the h264
    codec_ = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec_)
    {
      throw std::runtime_error("Codec not found.");
    }

    codec_ctx_ = avcodec_alloc_context3(codec_);
    codec_ctx_->flags2 |= AV_CODEC_FLAG2_FAST;
    if (!codec_ctx_)
    {
      throw std::runtime_error("Could not allocate video codec context.");
    }

    // Open codec
    if (avcodec_open2(codec_ctx_, codec_, nullptr) < 0)
    {
      throw std::runtime_error("Could not open codec.");
    }

    frame_ = av_frame_alloc();
    packet_ = av_packet_alloc();

    // Precompute undistortion transformation maps
    cv::initUndistortRectifyMap(camera_matrix_, dist_coeffs_, rotation_matrix_,
                                camera_matrix_, cv::Size(width_, height_org),
                                CV_32FC1, map1_, map2_);
  }

  ~CameraStreamDelegate()
  {
    av_frame_free(&frame_);
    av_packet_free(&packet_);
    avcodec_free_context(&codec_ctx_);
    // std::cout << "Successfully closed stream delegate." << std::endl;

    if (pipeline_)
    {
      gst_element_set_state(pipeline_, GST_STATE_NULL);
      gst_object_unref(pipeline_);
      gst_deinit();
    }
  }

  void OnVideoData(const uint8_t *data, size_t size, int64_t timestamp,
                   uint8_t streamType, int stream_index = 0) override
  {
    if (!pipeline_ || !appsrc_)
    {
      std::cerr << "Skipping as pipeline or appsrc not initialized."
                << std::endl;
      return;
    }

    if (stream_index == 0)
    {
      // Feed data into packet
      packet_->data = const_cast<uint8_t *>(data);
      packet_->size = size;

      // Send the packet to the decoder
      if (avcodec_send_packet(codec_ctx_, packet_) == 0)
      {

        // Receive frame from decoder
        while (avcodec_receive_frame(codec_ctx_, frame_) == 0)
        {
          int width = frame_->width;
          int height = frame_->height;
          int chroma_height = height / 2;
          int chroma_width = width / 2;

          // Create a single Mat to hold all three planes
          cv::Mat yuv(height + chroma_height, width, CV_8UC1);

          // Copy the Y plane
          std::memcpy(yuv.data, frame_->data[0], width * height);

          // Copy the U plane
          std::memcpy(yuv.data + width * height, frame_->data[1],
                      chroma_width * chroma_height);

          // Copy the V plane
          std::memcpy(yuv.data + width * height + chroma_width * chroma_height,
                      frame_->data[2], chroma_width * chroma_height);

          // Somehow RGB works instead of BGR
          cv::Mat bgr;
          cv::cvtColor(yuv, bgr, cv::COLOR_YUV420p2RGB);
          // cv::cvtColor(yuv, bgr, cv::COLOR_YUV420p2BGR);

          // cv::Mat rgb;
          // cv::cvtColor(bgr,rgb, cv::COLOR_BGR2RGB);

          cv::Mat img = bgr;
          cv::Mat resized_img;

          int half_width = img.cols / 2;
          cv::Rect roi_first(0, 0, half_width, img.rows);
          cv::Rect roi_second(half_width, 0, half_width, img.rows);
          cv::Mat img_first = UndistortImage(img(roi_first));
          cv::Mat img_second = UndistortImage(img(roi_second));

          cv::Mat undistorted_img;
          cv::hconcat(img_first, img_second, undistorted_img);

          // std::cout << "[size] img: " << img.size() << " undistorted_img: "
          // << undistorted_img.size() << std::endl;
          cv::resize(undistorted_img, resized_img, cv::Size(),
                     1.0 / img_downscale_, 1.0 / img_downscale_);

          cv::Mat send_img = resized_img;

          // Create a new buffer
          GstBuffer *buffer = gst_buffer_new_allocate(
              nullptr, send_img.total() * send_img.elemSize(), nullptr);
          GstMapInfo map;
          gst_buffer_map(buffer, &map, GST_MAP_WRITE);

          // Copy frame data to buffer
          std::memcpy(map.data, send_img.data,
                      send_img.total() * send_img.elemSize());

          // Unmap buffer
          gst_buffer_unmap(buffer, &map);

          // Push buffer to appsrc
          GstFlowReturn ret =
              gst_app_src_push_buffer(GST_APP_SRC(appsrc_), buffer);
          if (ret != GST_FLOW_OK)
          {
            std::cerr << "Failed to push buffer to appsrc: " << ret
                      << std::endl;
          }

          // Display the frame using OpenCV
          // cv::imshow("Live Video", send_img);
          // cv::waitKey(1);
        }
      }
    }
  }

  cv::Mat UndistortImage(const cv::Mat &img)
  {
    cv::Mat undistorted_img;
    cv::remap(img, undistorted_img, map1_, map2_, cv::INTER_LINEAR);
    return undistorted_img;
  }

  void OnAudioData(const uint8_t *data, size_t size,
                   int64_t timestamp) override {}
  void OnGyroData(const std::vector<ins_camera::GyroData> &data) override {}
  void OnExposureData(const ins_camera::ExposureData &data) override {}

private:
  std::string receiver_ip_;

  size_t width_;
  size_t height_;
  size_t fps_;
  size_t img_downscale_;

  AVCodec *codec_;
  AVCodecContext *codec_ctx_;
  AVFrame *frame_;
  AVPacket *packet_;
  struct SwsContext *sws_ctx_;

  GstElement *pipeline_;
  GstElement *appsrc_;

  cv::Mat map1_, map2_;
  const cv::Mat camera_matrix_ =
      (cv::Mat1d(3, 3) << 305.8511660909695, 0.0, 571.9976412068438, 0.0,
       304.8739784192282, 578.9734717897188, 0.0, 0.0, 1.0);
  const cv::Mat dist_coeffs_ =
      (cv::Mat1d(4, 1) << 0.0829955798117611, -0.027906274475464777,
       0.0076202648985968895, -0.0010836351255689319);
  const cv::Mat rotation_matrix_ = cv::Mat::eye(3, 3, CV_64F);
};

int main(int argc, char *argv[])
{
  gst_init(&argc, &argv);

  std::cout << "Begin open camera" << std::endl;
  ins_camera::DeviceDiscovery discovery;
  auto list = discovery.GetAvailableDevices();
  for (int i = 0; i < list.size(); ++i)
  {
    auto desc = list[i];
  }

  if (list.size() <= 0)
  {
    std::cerr << "No device found" << std::endl;
    return -1;
  }

  std::shared_ptr<ins_camera::Camera> cam =
      std::make_shared<ins_camera::Camera>(list[0].info);
  if (!cam->Open())
  {
    std::cerr << "Failed to open camera" << std::endl;
    return -1;
  }

  std::shared_ptr<ins_camera::StreamDelegate> delegate =
      std::make_shared<CameraStreamDelegate>();
  cam->SetStreamDelegate(delegate);

  discovery.FreeDeviceDescriptors(list);
  std::cout << "Successfully opened Insta 360 camera." << std::endl;

  auto camera_type = cam->GetCameraType();

  auto start = time(NULL);
  cam->SyncLocalTimeToCamera(start);

  // Params for live steaming
  ins_camera::LiveStreamParam param;
  param.video_resolution = ins_camera::VideoResolution::RES_1152_1152P30;
  param.video_bitrate = 1024 * 1024 / 100;
  param.enable_audio = false;
  param.using_lrv = false;

  if (cam->StartLiveStreaming(param))
  {
    std::cout << "Successfully started live stream." << std::endl;
  }

  char key;
  std::cout << "Press 'enter' key to quit. ";
  std::cin.get(key);

  if (cam->StopLiveStreaming())
  {
    std::cout << "Successfully stoped streaming." << std::endl;
  }
  else
  {
    std::cerr << "Failed to stop streaming." << std::endl;
  }

  cam->Close();
  return 0;
}
