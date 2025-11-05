#pragma once
#include "Event.h"

namespace ts {
class ITrader;
class Strategy {
 public:
  virtual ~Strategy() = default;
  virtual void on_market_data(const MarketDataEvent& md, ITrader* trader) = 0;
  
  virtual void on_order_status(const OrderStatusEvent& ev) {}
  
  virtual void on_bar(const BarEvent& bar, ITrader* trader) {}
};
}