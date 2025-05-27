#include <crow.h>
#include <yaml-cpp/yaml.h>

#include <chrono>
#include <iostream>
#include <string>
#include <memory>

#include "audio.h"
#include "recongizer.h"
#include "task_manager.h"
#include "sherpa-onnx/c-api/cxx-api.h"

using sherpa_onnx::cxx::OfflineRecognizer;
using sherpa_onnx::cxx::OfflineRecognizerConfig;
using sherpa_onnx::cxx::OfflineRecognizerResult;
using sherpa_onnx::cxx::OfflineStream;
using std::cerr;
using std::cout;
using std::string;

struct WebConfig {
  string host;
  int32_t port;
  int32_t resample_rate;
  int32_t max_processing_time;
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
  config.max_processing_time = node["max_processing_time"].as<int32_t>();
  return config;
}

AppConfig LoadConfig(const string &filename) {
  YAML::Node node = YAML::LoadFile(filename);
  AppConfig config;
  config.model_config = LoadModelConfig(node["model"]);
  config.web_config = LoadWebConfig(node["web"]);
  return config;
}

crow::SimpleApp SetupCrow(const std::shared_ptr<RecognitionTaskManager> task_manager, const WebConfig &web_config) {
  crow::SimpleApp app;

  CROW_ROUTE(app, "/health")
  ([task_manager]() {
    crow::json::wvalue res;
    res["status"] = "ok";
    res["queue_size"] = task_manager->getQueueSize();
    return res;
  });

  const auto &config = web_config;

  CROW_ROUTE(app, "/asr").methods("POST"_method)([task_manager, &config](const crow::request &req) {
    const auto begin = std::chrono::steady_clock::now();

    crow::multipart::mp_map part_map;
    try {
      crow::multipart::message multipart_req(req);
      part_map = multipart_req.part_map;
    } catch (const std::exception &e) {
      return crow::response(400, std::string("Multipart parse error: ") + e.what());
    }

    std::string language = "auto";
    std::vector<uint8_t> file_data;

    for (auto &[key, part] : part_map) {
      if (key == "language" && !part.body.empty()) {
        language = part.body;
      } else if (key == "file") {
        file_data = std::vector<uint8_t>(part.body.begin(), part.body.end());
      }
    }

    if (file_data.empty()) {
      return crow::response(400, "Missing 'file' field.");
    }

    auto wave = ReadAudio(file_data, config.resample_rate);
    if (!wave.isValid()) {
      return crow::response(400, "Failed to read audio file.");
    }

    auto future = task_manager->submitTask(wave);

    if (future.wait_for(std::chrono::seconds(config.max_processing_time)) != std::future_status::ready) {
      return crow::response(504, "Timeout while processing");
    }

    auto asr_result = future.get();
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

  auto recongizer = std::make_shared<Recongizer>(model_config);
  if (!recongizer) {
    cerr << "Failed to create recongizer with config: " << config_filename << "\n";
    return -1;
  }
  recongizer->Init();

  auto task_manager = std::make_shared<RecognitionTaskManager>(
    [recongizer](const AudioData &wave) { return recongizer->Recognize(wave); });

  auto app = SetupCrow(task_manager, web_config);
  app.bindaddr(web_config.host).port(web_config.port).multithreaded().run();

  return 0;
}
