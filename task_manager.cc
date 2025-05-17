#include "task_manager.h"

using sherpa_onnx::cxx::OfflineRecognizerResult;
using std::future;
using std::mutex;

future<OfflineRecognizerResult> RecognitionTaskManager::submitTask(AudioData input, int priority) {
  RecognitionTask task;
  task.input = std::move(input);
  task.priority = priority;
  auto future = task.promise.get_future();

  {
    std::lock_guard<mutex> lock(mutex_);
    taskQueue_.push(std::move(task));
  }
  cv_.notify_one();

  return future;
}

size_t RecognitionTaskManager::getQueueSize() const {
  std::lock_guard<mutex> lock(mutex_);
  return taskQueue_.size();
}

void RecognitionTaskManager::processTasks() {
  while (true) {
    RecognitionTask task;
    {
      std::unique_lock<mutex> lock(mutex_);
      cv_.wait(lock, [&] { return !taskQueue_.empty() || !running_; });

      if (!running_ && taskQueue_.empty()) break;

      task = std::move(const_cast<RecognitionTask &>(taskQueue_.top()));
      taskQueue_.pop();
    }

    auto result = recognitionTaskProcessor_(task.input);

    task.promise.set_value(result);
  }
}
