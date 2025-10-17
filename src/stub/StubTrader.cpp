#include "stub/StubTrader.h"
#include <iostream>
#include <cstdlib>
#include <thread>
#include <chrono>

namespace ts {
bool StubTrader::connect(const std::string& front) {
  std::cout << "[StubTD] Connect to " << front << std::endl;
  return true;
}

bool StubTrader::login(const std::string& broker_id, const std::string& user_id, const std::string& password) {
  std::cout << "[StubTD] Login broker=" << broker_id << " user=" << user_id << std::endl;
  return true;
}

std::string StubTrader::place_order(const OrderRequest& req) {
  std::string id = "STUB_" + std::to_string(std::rand());
  std::cout << "[StubTD] Place order id=" << id << " " << req.instrument
            << " dir=" << (req.direction == Direction::Buy ? "Buy":"Sell")
            << " vol=" << req.volume << " price=" << req.price << std::endl;
  if (handler_) {
    OrderStatusEvent ev; ev.order_id = id; ev.status = "Accepted"; ev.message = "Order accepted";
    ev.instrument = req.instrument; ev.filled_qty = 0; ev.fill_price = 0.0; ev.remaining_qty = req.volume;
    handler_(ev);
    // simulate fill
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    OrderStatusEvent ev2; ev2.order_id = id; ev2.status = "Filled"; ev2.message = "Order filled";
    ev2.instrument = req.instrument; ev2.filled_qty = req.volume; ev2.fill_price = req.price; ev2.remaining_qty = 0;
    handler_(ev2);
  }
  return id;
}

bool StubTrader::cancel_order(const std::string& order_id) {
  std::cout << "[StubTD] Cancel order " << order_id << std::endl;
  if (handler_) {
    OrderStatusEvent ev{order_id, "Canceled", "Order canceled"};
    handler_(ev);
  }
  return true;
}

void StubTrader::set_order_status_handler(OrderStatusHandler handler) {
  handler_ = std::move(handler);
}

} // namespace ts