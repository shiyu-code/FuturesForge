#pragma once
#include <unordered_map>
#include <functional>
#include <chrono>
#include <string>
#include "Event.h"

namespace ts {
class BarAggregator {
 public:
  using BarHandler = std::function<void(const BarEvent&)>;
  explicit BarAggregator(int interval_seconds);
  void set_bar_handler(BarHandler h);
  void on_tick(const MarketDataEvent& md);
  void flush_all();
 private:
  struct Accum {
    BarEvent bar;
    std::chrono::steady_clock::time_point start;
  };
  int interval_{1};
  BarHandler handler_;
  std::unordered_map<std::string, Accum> acc_;
  static std::string now_string();
};
}