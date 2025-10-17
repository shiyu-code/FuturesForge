#pragma once
#include "TradingSystem/IMarketData.h"
#include <atomic>
#include <thread>
#include <string>
#include <vector>
#include <unordered_set>

namespace ts {
class BacktestMarketData : public IMarketData {
 public:
  explicit BacktestMarketData(int speed_ms = 5);
  ~BacktestMarketData() override;
  bool connect(const std::string& front) override; // front作为CSV文件路径
  bool login(const std::string& broker_id, const std::string& user_id, const std::string& password) override;
  bool subscribe(const std::vector<std::string>& instruments) override;
  void set_market_data_handler(MarketDataHandler handler) override;
 private:
  void run_loop();
  std::string file_;
  int speed_ms_{5};
  MarketDataHandler handler_;
  std::unordered_set<std::string> sub_set_;
  std::atomic<bool> running_{false};
  std::thread worker_;
};
} // namespace ts