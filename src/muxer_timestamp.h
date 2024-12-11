#pragma once

#include <chrono>

class MuxerTimestamp {
public:
  // Initialize and record the start time
  void start() {
    startTime = Clock::now();
  }

  // Get the current timestamp in 90 kHz units
  uint64_t getCurrentTimestamp() const {
    auto now = Clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - startTime).count();
    return static_cast<uint64_t>(elapsed * 90 / 1000); // Convert microseconds to 90 kHz units
  }

protected:
  using Clock = std::chrono::steady_clock;
  Clock::time_point startTime;
};
