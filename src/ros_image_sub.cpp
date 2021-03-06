/* ros_image_sub
 * Copyright 2019 Lucas Walter
 * This file is a Frei0r plugin.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <algorithm>
#include <assert.h>
#include <cv_bridge/cv_bridge.h>
#include <iostream>
#include <memory>
#include <ros/master.h>
#include <ros/ros.h>
#include <sensor_msgs/Image.h>
#include <stdlib.h>
#include <string>

extern "C" {
  #include <frei0r.h>
}

typedef struct ros_image_sub_instance {
  unsigned int width_;
  unsigned int height_;
  std::string topic_ = "image_in";

  std::unique_ptr<ros::NodeHandle> nh_;
  ros::Subscriber sub_;
  cv::Mat image_in_;

  void imageCallback(const sensor_msgs::ImageConstPtr& msg)
  {
    cv_bridge::CvImageConstPtr cv_ptr;
    try {
      cv_ptr = cv_bridge::toCvShare(msg, "bgra8");
    } catch (cv_bridge::Exception& ex) {
      ROS_ERROR_THROTTLE(1.0, "cv bridge exception %s", ex.what());
      return;
    }
    cv::resize(cv_ptr->image, image_in_,
        cv::Size(width_, height_), cv::INTER_NEAREST);
  }
} ros_image_sub_instance_t;

/* Clamps a int32-range int between 0 and 255 inclusive. */
unsigned char CLAMP0255(int32_t a) {
  return (unsigned char)((((-a) >> 31) & a)  // 0 if the number was negative
                         | (255 - a) >>
                               31);  // -1 if the number was greater than 255
}

int f0r_init() {
  int argc = 0;
  ros::init(argc, nullptr, "frei0r", ros::init_options::AnonymousName);
  ROS_INFO_STREAM("ros_image_sub init " << ros::this_node::getName());
  return 1;
}

void f0r_deinit() {
  std::cout << "deinit\n";
}

void f0r_get_plugin_info(f0r_plugin_info_t *inverterInfo) {
  inverterInfo->name = "ros_image_sub";
  inverterInfo->author = "Lucas Walter";
  inverterInfo->plugin_type = F0R_PLUGIN_TYPE_SOURCE;
  inverterInfo->color_model = F0R_COLOR_MODEL_BGRA8888;
  inverterInfo->frei0r_version = FREI0R_MAJOR_VERSION;
  inverterInfo->major_version = 0;
  inverterInfo->minor_version = 1;
  inverterInfo->num_params = 1;
  inverterInfo->explanation = "subscribes to a ros image topic";
}

void f0r_get_param_info(f0r_param_info_t *info, int param_index) {
  switch (param_index) {
  case 0:
    info->name = "ros image topic";
    info->type = F0R_PARAM_STRING;
    info->explanation = "ros image topic";
    break;
  }
}

f0r_instance_t f0r_construct(unsigned int width, unsigned int height) {
  ros_image_sub_instance_t *inst = new ros_image_sub_instance;

  inst->width_ = width;
  inst->height_ = height;

  ROS_INFO_STREAM("ros_image_sub construct " << ros::this_node::getName() << " "
      << width << " x " << height << " " << inst->topic_);
  return (f0r_instance_t)inst;
}

void f0r_destruct(f0r_instance_t instance) {
  std::cout << "destruct\n";
  ros_image_sub_instance_t *inst = reinterpret_cast<ros_image_sub_instance_t*>(instance);
  delete inst;
}

void f0r_set_param_value(f0r_instance_t instance, f0r_param_t param,
                         int param_index) {
  assert(instance);
  ros_image_sub_instance_t *inst = reinterpret_cast<ros_image_sub_instance_t*>(instance);

  switch (param_index) {
  case 0:
    const std::string topic = std::string(*(reinterpret_cast<char**>(param)));
    // inst->topic_ = (*(char**)(param));
    // std::cout << inst->topic_ << "\n";
    if (topic != inst->topic_) {
      inst->topic_ = topic;
      ROS_INFO_STREAM("new topic " << inst->topic_);
      if (inst->nh_) {
        inst->sub_.shutdown();
        if (inst->topic_ != "") {
          // TODO(lucasw) try in case topic is badly formatted?
          inst->sub_ = inst->nh_->subscribe<sensor_msgs::Image>(inst->topic_, 3,
              &ros_image_sub_instance::imageCallback, inst);
        }
      }
    }
    break;
  }
}

void f0r_get_param_value(f0r_instance_t instance, f0r_param_t param,
                         int param_index) {
  assert(instance);
  ros_image_sub_instance_t *inst = reinterpret_cast<ros_image_sub_instance_t*>(instance);

  switch (param_index) {
  case 0:
    ROS_INFO_STREAM("get param start");
    *(reinterpret_cast<f0r_param_string*>(param)) = const_cast<char*>(inst->topic_.data());
    ROS_INFO_STREAM("get param done");
    break;
  }
}

void f0r_update(f0r_instance_t instance, double time, const uint32_t *inframe,
                uint32_t *outframe) {
  assert(instance);
  ros_image_sub_instance_t *inst = reinterpret_cast<ros_image_sub_instance_t*>(instance);

  unsigned char *dst = reinterpret_cast<unsigned char *>(outframe);

  if (!inst->nh_) {
    if (!ros::master::check()) {
      return;
    }
    inst->nh_ = std::make_unique<ros::NodeHandle>();
    inst->sub_ = inst->nh_->subscribe<sensor_msgs::Image>(inst->topic_, 3,
        &ros_image_sub_instance::imageCallback, inst);
    ROS_INFO_STREAM("subscribed to " << inst->topic_);
  }

  ros::spinOnce();

  if (inst->image_in_.empty()) {
    ROS_INFO_STREAM("blank image");
    inst->image_in_ = cv::Mat(cv::Size(inst->width_, inst->height_),
                              CV_8UC4, cv::Scalar(0, 0, 0, 0));
  }

  cv::Mat tmp = inst->image_in_;
  if ((inst->image_in_.cols != inst->width_) ||
      (inst->image_in_.rows != inst->height_)) {
    ROS_WARN_STREAM("size mismatch ");
    cv::resize(inst->image_in_, tmp,
        cv::Size(inst->width_, inst->height_), cv::INTER_NEAREST);
  }

  const size_t num_bytes = inst->width_ * inst->height_ * 4;
  std::copy(&tmp.data[0], &tmp.data[0] + num_bytes, dst);
}
