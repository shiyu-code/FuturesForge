#include "TradingSystem/BarAggregator.h"
#include <iomanip>
#include <sstream>

namespace ts {

BarAggregator::BarAggregator(int interval_seconds) : interval_(interval_seconds > 0 ? interval_seconds : 1) {}

void BarAggregator::set_bar_handler(BarHandler h) { handler_ = std::move(h); }

void BarAggregator::on_tick(const MarketDataEvent& md) {
  auto now = std::chrono::steady_clock::now();
  auto it = acc_.find(md.instrument);
  if (it == acc_.end()) {
    Accum a;
    a.start = now;
    a.bar.instrument = md.instrument;
    a.bar.open = a.bar.high = a.bar.low = a.bar.close = md.last_price;
    a.bar.volume = md.volume;
    a.bar.ts = now_string();
    acc_.emplace(md.instrument, a);
    return;
  }
  auto& a = it->second;
  auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - a.start).count();
  if (elapsed >= interval_) {
    if (handler_) handler_(a.bar);
    a.start = now;
    a.bar.instrument = md.instrument;
    a.bar.open = a.bar.high = a.bar.low = a.bar.close = md.last_price;
    a.bar.volume = md.volume;
    a.bar.ts = now_string();
  } else {
    a.bar.close = md.last_price;
    if (md.last_price > a.bar.high) a.bar.high = md.last_price;
    if (md.last_price < a.bar.low) a.bar.low = md.last_price;
    a.bar.volume += md.volume;
  }
}

void BarAggregator::flush_all() {
  if (!handler_) return;
  for (auto& kv : acc_) handler_(kv.second.bar);
}

std::string BarAggregator::now_string() {
  using namespace std::chrono;
  auto tp = system_clock::now();
  auto t = system_clock::to_time_t(tp);
  std::tm tm{};
#ifdef _WIN32
  localtime_s(&tm, &t);
#else
  tm = *std::localtime(&t);
#endif
  std::stringstream ss;
  ss << std::put_time(&tm, "%F %T");
  return ss.str();
}

} // namespace ts