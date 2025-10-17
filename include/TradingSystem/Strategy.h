#pragma once
#include "Event.h"

namespace ts {
class ITrader;
class Strategy {
 public:
  virtual ~Strategy() = default;
  virtual void on_market_data(const MarketDataEvent& md, ITrader* trader) = 0;
  // 可选：订单状态回调，默认空实现
  virtual void on_order_status(const OrderStatusEvent& ev) {}
  // 可选：bar回调，默认空实现
  virtual void on_bar(const BarEvent& bar, ITrader* trader) {}
};
}