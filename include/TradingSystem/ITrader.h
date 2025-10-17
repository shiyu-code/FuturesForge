#pragma once
#include <string>
#include <functional>
#include "Event.h"

namespace ts {
class ITrader {
 public:
  using OrderStatusHandler = std::function<void(const OrderStatusEvent&)>;
  virtual ~ITrader() = default;
  virtual bool connect(const std::string& front) = 0;
  virtual bool login(const std::string& broker_id, const std::string& user_id, const std::string& password) = 0;
  virtual std::string place_order(const OrderRequest& req) = 0;
  virtual bool cancel_order(const std::string& order_id) = 0;
  virtual void set_order_status_handler(OrderStatusHandler handler) = 0;
};
}