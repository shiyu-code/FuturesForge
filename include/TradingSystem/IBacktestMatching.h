#pragma once
#include <string>
#include "TradingSystem/Event.h"

namespace ts {


struct IBacktestMatching {
  virtual ~IBacktestMatching() = default;
  virtual void on_market_data(const MarketDataEvent& ev) = 0;
  virtual void configure(const std::string& meta_path, const std::string& rules_path) = 0;
};

} 