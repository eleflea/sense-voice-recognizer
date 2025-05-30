#include <cstdlib>
#include <string>
#include <stdexcept>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <unordered_map>
#include <mutex>
#include <optional>

#include "include/dotenv.h"

class Config {
 public:
  Config() { dotenv::init(".env.local"); }

  template <typename T>
  T get(const std::string &key) const {
    auto raw = getRawValue(key);
    if (!raw) {
      throw std::runtime_error("Environment variable not found: " + key);
    }
    return convert<T>(*raw);
  }

  template <typename T>
  T get(const std::string &key, const T &defaultValue) const {
    if (auto raw = getRawValue(key)) {
      return convert<T>(*raw);
    }
    return defaultValue;
  }

 private:
  std::optional<std::string> getRawValue(const std::string &key) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = cache_.find(key);
    if (it != cache_.end()) {
      return it->second;
    }

    const char *env = std::getenv(key.c_str());
    if (!env) {
      return std::nullopt;  // Environment variable not found
    }

    std::string value = env;
    cache_[key] = value;
    return value;
  }

  template <typename T>
  T convert(const std::string &value) const;

  mutable std::unordered_map<std::string, std::string> cache_;
  mutable std::mutex mutex_;
};

template <>
std::string Config::convert<std::string>(const std::string &value) const {
  return value;
}

template <>
int32_t Config::convert<int32_t>(const std::string &value) const {
  return static_cast<int32_t>(std::stol(value));
}

template <>
bool Config::convert<bool>(const std::string &value) const {
  std::string val = value;
  std::transform(val.begin(), val.end(), val.begin(), ::tolower);

  if (val == "1" || val == "true" || val == "yes" || val == "on") return true;
  if (val == "0" || val == "false" || val == "no" || val == "off") return false;

  throw std::invalid_argument("Invalid boolean value: " + value);
}

template <>
int64_t Config::convert<int64_t>(const std::string &value) const {
  return std::stoll(value);
}

template <>
float Config::convert<float>(const std::string &value) const {
  return std::stof(value);
}

template <>
double Config::convert<double>(const std::string &value) const {
  return std::stod(value);
}
