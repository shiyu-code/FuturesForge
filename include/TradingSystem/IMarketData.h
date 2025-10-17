#pragma once
#include <string>
#include <vector>
#include <functional>
#include "Event.h"

namespace ts {
class IMarketData {
 public:
  using MarketDataHandler = std::function<void(const MarketDataEvent&)>;
  virtual ~IMarketData() = default;
  virtual bool connect(const std::string& front) = 0;
  virtual bool login(const std::string& broker_id, const std::string& user_id, const std::string& password) = 0;
  virtual bool subscribe(const std::vector<std::string>& instruments) = 0;
  virtual void set_market_data_handler(MarketDataHandler handler) = 0;
};
}