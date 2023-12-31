#pragma once

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <json.hpp>
#include <string>
#include <utility>

using json = nlohmann::json;

class Config {
public:
  static std::string path;

  Config(const Config &) = delete;
  Config &operator=(const Config &) = delete;
  static Config &get_instance() {
    static Config config(path);
    return config;
  }

  static void set_path(std::string _path) { path = _path; }
  static std::string get_path() { return path; }

  template <typename Valuetype> Valuetype get(const std::string &key) {
    return config_[key].get<Valuetype>();
  }

  std::string dump(bool pretty = true) {
    if (pretty) {
      return config_.dump(4);
    }
    return config_.dump();
  }

private:
  json config_;

  Config(std::string path) {
    if (path == "") {
      path = "../config.json";
    }

    std::ifstream f(path);
    if (!f.good()) {
      std::cerr << "path: " << path << " doesn't exist" << std::endl;
      exit(EXIT_FAILURE);
    }
    config_ = json::parse(f);
  }
};
