#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <future>
#include <atomic>
#include <chrono>
#include <functional>

#include "audio.h"
#include "sherpa-onnx/c-api/cxx-api.h"

struct RecognitionTask {
  int32_t priority;
  std::promise<sherpa_onnx::cxx::OfflineRecognizerResult> promise;
  AudioData input;

  bool operator<(const RecognitionTask &other) const { return priority < other.priority; }
};

using RecognitionTaskFn = std::function<sherpa_onnx::cxx::OfflineRecognizerResult(const AudioData &)>;

class RecognitionTaskManager {
 public:
  RecognitionTaskManager(const RecognitionTaskFn &fn)
      : running_(true), worker_(&RecognitionTaskManager::processTasks, this), recognitionTaskProcessor_(fn) {}

  ~RecognitionTaskManager() {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      running_ = false;
    }
    cv_.notify_one();
    if (worker_.joinable()) worker_.join();
  }

  std::future<sherpa_onnx::cxx::OfflineRecognizerResult> submitTask(AudioData input, int priority = 0);

  size_t getQueueSize() const;

 private:
  void processTasks();

  mutable std::mutex mutex_;
  std::condition_variable cv_;
  std::priority_queue<RecognitionTask> taskQueue_;
  std::atomic<bool> running_;
  std::thread worker_;
  RecognitionTaskFn recognitionTaskProcessor_;
};
