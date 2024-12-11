#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>

template<typename T>
class ThreadSafeQueue {
  std::queue<T> queue;
  std::mutex mutex;
  std::condition_variable cv;

public:
  void push(T value) {
    std::lock_guard<std::mutex> lock(mutex);
    queue.push(value);
    cv.notify_one();
  }

  T pop() {
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait(lock, [&] { return !queue.empty(); });
    T value = queue.front();
    queue.pop();
    return value;
  }

  bool empty() {
    std::lock_guard<std::mutex> lock(mutex);
    return queue.empty();
  }
};

