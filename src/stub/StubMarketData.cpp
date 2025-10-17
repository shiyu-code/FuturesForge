#include "stub/StubMarketData.h"
#include "TradingSystem/Event.h"
#include <iostream>
#include <chrono>

namespace ts {

StubMarketData::StubMarketData() : rng_(std::random_device{}()) {}

bool StubMarketData::connect(const std::string& front) {
  std::cout << "[StubMD] Connect to " << front << std::endl;
  return true;
}

bool StubMarketData::login(const std::string& broker_id, const std::string& user_id, const std::string& password) {
  std::cout << "[StubMD] Login broker=" << broker_id << " user=" << user_id << std::endl;
  return true;
}

bool StubMarketData::subscribe(const std::vector<std::string>& instruments) {
  instruments_ = instruments;
  std::cout << "[StubMD] Subscribe instruments:";
  for (auto& s : instruments_) std::cout << " " << s;
  std::cout << std::endl;
  running_.store(true);
  worker_ = std::thread(&StubMarketData::run_loop, this);
  return true;
}

void StubMarketData::set_market_data_handler(IMarketData::MarketDataHandler handler) {
  handler_ = std::move(handler);
}

void StubMarketData::run_loop() {
  std::uniform_real_distribution<double> dist(100.0, 500.0);
  int ticks = 0;
  while (running_.load() && ticks < 200) {
    for (const auto& inst : instruments_) {
      MarketDataEvent ev;
      ev.instrument = inst;
      ev.last_price = dist(rng_);
      ev.bid_price = ev.last_price - 0.5;
      ev.ask_price = ev.last_price + 0.5;
      ev.volume = static_cast<int>(dist(rng_));
      ev.update_time = "stub";
      if (handler_) handler_(ev);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ++ticks;
  }
  running_.store(false);
}

StubMarketData::~StubMarketData() {
  running_.store(false);
  if (worker_.joinable()) worker_.join();
}

} // namespace ts