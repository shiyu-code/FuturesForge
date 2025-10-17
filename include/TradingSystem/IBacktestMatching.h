#pragma once
#include <string>
#include "TradingSystem/Event.h"

namespace ts {

// 供回测撮合器在引擎内接收行情与加载配置
struct IBacktestMatching {
  virtual ~IBacktestMatching() = default;
  virtual void on_market_data(const MarketDataEvent& ev) = 0;
  virtual void configure(const std::string& meta_path, const std::string& rules_path) = 0;
};

} // namespace ts