#include <crow.h>
#include <yaml-cpp/yaml.h>

#include <chrono>
#include <iostream>
#include <optional>
#include <string>

#include "audio.h"
#include "sherpa-onnx/c-api/cxx-api.h"

using sherpa_onnx::cxx::OfflineRecognizer;
using sherpa_onnx::cxx::OfflineRecognizerConfig;
using sherpa_onnx::cxx::OfflineRecognizerResult;
using sherpa_onnx::cxx::OfflineStream;
using std::cerr;
using std::cout;
using std::nullopt;
using std::optional;
using std::string;

struct WebConfig {
  string host;
  int32_t port;
  int32_t resample_rate;
};

struct AppConfig {
  OfflineRecognizerConfig model_config;
  WebConfig web_config;
};

OfflineRecognizerConfig LoadModelConfig(const YAML::Node &node) {
  OfflineRecognizerConfig config;
  config.model_config.sense_voice.model = node["weights"].as<string>();
  config.model_config.sense_voice.use_itn = node["use_itn"].as<bool>();
  config.model_config.sense_voice.language = node["language"].as<string>();
  config.model_config.tokens = node["tokens"].as<string>();
  config.model_config.num_threads = node["num_threads"].as<int32_t>();

  return config;
}

WebConfig LoadWebConfig(const YAML::Node &node) {
  WebConfig config;
  config.host = node["host"].as<string>();
  config.port = node["port"].as<int32_t>();
  config.resample_rate = node["resample_rate"].as<int32_t>();
  return config;
}

AppConfig LoadConfig(const string &filename) {
  YAML::Node node = YAML::LoadFile(filename);
  AppConfig config;
  config.model_config = LoadModelConfig(node["model"]);
  config.web_config = LoadWebConfig(node["web"]);
  return config;
}

optional<OfflineRecognizer> CreateRecongizer(const OfflineRecognizerConfig &config) {
  cout << "Loading model\n";
  OfflineRecognizer recongizer = OfflineRecognizer::Create(config);
  if (!recongizer.Get()) {
    cerr << "Please check your config\n";
    return nullopt;
  }
  cout << "Loading model done\n";
  return recongizer;
}

OfflineRecognizerResult Recognize(const OfflineRecognizer &recongizer, const AudioData &wave) {
  OfflineStream stream = recongizer.CreateStream();
  stream.AcceptWaveform(wave.sample_rate, wave.samples.data(), wave.samples.size());
  recongizer.Decode(&stream);
  return recongizer.GetResult(&stream);
}

crow::SimpleApp SetupCrow(const OfflineRecognizer &recongizer, int32_t resample_rate) {
  crow::SimpleApp app;

  CROW_ROUTE(app, "/health")
  ([]() {
    crow::json::wvalue res;
    res["status"] = "ok";
    return res;
  });

  CROW_ROUTE(app, "/asr").methods("POST"_method)([&recongizer, resample_rate](const crow::request &req) {
    const auto begin = std::chrono::steady_clock::now();

    crow::multipart::message multipart_req(req);

    std::string language = "auto";
    std::vector<uint8_t> file_data;

    for (auto &[key, part] : multipart_req.part_map) {
      if (key == "language" && !part.body.empty()) {
        language = part.body;
      } else if (key == "file") {
        file_data = std::vector<uint8_t>(part.body.begin(), part.body.end());
      }
    }

    if (file_data.empty()) {
      return crow::response(400, "Missing 'file' field.");
    }

    AudioData wave;
    auto result = ReadAudio(file_data, resample_rate, wave);
    if (!result) {
      return crow::response(400, "Failed to read audio file.");
    }

    OfflineRecognizerResult asr_result = Recognize(recongizer, wave);

    const auto end = std::chrono::steady_clock::now();
    const float elapsed_seconds = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() / 1000.;
    float duration = wave.samples.size() / static_cast<float>(wave.sample_rate);
    float rtf = duration / elapsed_seconds;
    cout << "RTF = " << duration << "s / " << elapsed_seconds << "s = " << rtf << "\n";

    return crow::response(200, asr_result.json);
  });
  return app;
}

int32_t main() {
  string config_filename = "config.yaml";
  auto [model_config, web_config] = LoadConfig(config_filename);

  auto recongizer = CreateRecongizer(model_config);
  if (!recongizer) {
    cerr << "Failed to create recongizer with config: " << config_filename << "\n";
    return -1;
  }

  auto app = SetupCrow(*recongizer, web_config.resample_rate);
  app.bindaddr(web_config.host).port(web_config.port).multithreaded().run();

  return 0;
}
