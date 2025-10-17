#pragma once
#include "TradingSystem/IMarketData.h"
#include <atomic>
#include <thread>
#include <random>

namespace ts {
class StubMarketData : public IMarketData {
 public:
  StubMarketData();
  ~StubMarketData() override;
  bool connect(const std::string& front) override;
  bool login(const std::string& broker_id, const std::string& user_id, const std::string& password) override;
  bool subscribe(const std::vector<std::string>& instruments) override;
  void set_market_data_handler(MarketDataHandler handler) override;
 private:
  void run_loop();
  std::vector<std::string> instruments_;
  MarketDataHandler handler_;
  std::atomic<bool> running_{false};
  std::thread worker_;
  std::mt19937 rng_;
};
}