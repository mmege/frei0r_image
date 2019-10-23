/**
 * Copyright (c) 2019 Lucas Walter
 * Load frei0r plugins following these guidelines:
 * https://frei0r.dyne.org/codedoc/html/group__pluglocations.html
 */

#include <ddynamic_reconfigure/ddynamic_reconfigure.h>
#include <experimental/filesystem>
#include <frei0r_image/frei0r_image.hpp>
// TODO(lucasw) there is a C++ header in the latest frei0r sources,
// but it isn't in Ubuntu 18.04 released version currently
// #define _UNIX03_SOURCE
#include <dlfcn.h>
#include <frei0r.h>
#include <iostream>
#include <map>
#include <nodelet/nodelet.h>
#include <vector>
#include <ros/ros.h>
#include <sensor_msgs/Image.h>
#include <string>


int main(int argc, char** argv)
{
  std::vector<std::string> plugin_dirs = {
    "/usr/lib/frei0r-1/",  // ubuntu frei0r-plugins puts them here
    "/usr/local/lib/frei0r-1/",
    "/.frei0r-1/lib"  // TODO(lucasw) need to prefix $HOME to this
  };

  std::vector<std::string> plugin_names;
  for (const auto& dir : plugin_dirs) {
    if (!std::experimental::filesystem::exists(dir)) {
      continue;
    }
    try {
      for (const auto& entry : std::experimental::filesystem::directory_iterator(dir)) {
        plugin_names.push_back(entry.path());
        // std::cout << entry.path() << "\n";
      }
    } catch (std::experimental::filesystem::v1::__cxx11::filesystem_error& ex) {
      std::cout << dir << " " << ex.what() << "\n";
    }
  }

  if (plugin_names.size() == 0) {
    return 1;
  }

  std::map<int, std::map<std::string, std::shared_ptr<frei0r_image::Frei0rImage>>> plugins;

  for (const auto& plugin_name : plugin_names) {
    auto plugin = std::make_shared<frei0r_image::Frei0rImage>();
    plugin->loadLibrary(plugin_name);
    const int plugin_type = plugin->fi_.plugin_type;
    plugins[plugin_type][plugin_name] = plugin;
    std::cout << plugin_type << " " << plugin_name << "\n";
  }

  for (const auto& plugin_pair : plugins[1]) {
    plugin_pair.second->print();
  }

  std::cout << std::endl;
  return 0;
}

