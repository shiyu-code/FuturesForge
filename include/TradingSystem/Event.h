#pragma once
#include <string>
#include <vector>
#include <functional>

namespace ts {

enum class Direction { Buy, Sell };
enum class Offset { Open, Close };


enum class OrderType { Limit, Market, IOC, FOK };

struct MarketDataEvent {
  std::string instrument;
  double last_price{0.0};
  double bid_price{0.0};
  double ask_price{0.0};
  int volume{0};
  int bid_volume{0};
  int ask_volume{0};
  std::string update_time; 
};

struct OrderRequest {
  std::string instrument;
  Direction direction{Direction::Buy};
  Offset offset{Offset::Open};
  OrderType type{OrderType::Limit};
  double price{0.0};
  int volume{1};
};

struct OrderStatusEvent {
  std::string order_id;
  std::string status; 
  std::string message;
  
  std::string instrument;
  int filled_qty{0};
  double fill_price{0.0};
  int remaining_qty{-1};
};

struct BarEvent {
  std::string instrument;
  double open{0.0};
  double high{0.0};
  double low{0.0};
  double close{0.0};
  int volume{0};
  std::string ts; 
};

using MarketDataHandler = std::function<void(const MarketDataEvent&)>;
using BarEventHandler = std::function<void(const BarEvent&)>;
using OrderStatusHandler = std::function<void(const OrderStatusEvent&)>;

} 