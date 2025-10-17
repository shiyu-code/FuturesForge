#pragma once
#include "TradingSystem/ITrader.h"

namespace ts {
class StubTrader : public ITrader {
 public:
  bool connect(const std::string& front) override;
  bool login(const std::string& broker_id, const std::string& user_id, const std::string& password) override;
  std::string place_order(const OrderRequest& req) override;
  bool cancel_order(const std::string& order_id) override;
  void set_order_status_handler(OrderStatusHandler handler) override;
 private:
  OrderStatusHandler handler_;
};
}