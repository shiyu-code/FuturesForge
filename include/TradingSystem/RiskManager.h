#pragma once
#include <string>
#include <unordered_map>
#include <chrono>
#include "Event.h"

namespace ts {
struct RiskConfig {
  int max_pos_per_instrument{1};
  int max_orders_per_bar{1};
  int min_order_interval_ms{500};
};

class RiskManager {
 public:
  explicit RiskManager(RiskConfig cfg);
  bool can_place(const OrderRequest& req, std::string* reject_reason);
  void on_order_placed(const std::string& instrument);
  void on_new_bar(const std::string& instrument);
  void register_order(const std::string& order_id, const OrderRequest& req);
  void on_order_status(const OrderStatusEvent& ev);
  
  void on_market_data(const MarketDataEvent& ev);
  
  const std::unordered_map<std::string, int>& positions() const;
  
  struct PositionDetail { int long_qty{0}; int short_qty{0}; };
  const std::unordered_map<std::string, PositionDetail>& positions_detail() const;
  
  struct PnLInfo { int long_open_qty{0}; double long_open_cost_sum{0.0}; int short_open_qty{0}; double short_open_cost_sum{0.0}; double realized_pnl{0.0}; double unrealized_pnl{0.0}; };
  const std::unordered_map<std::string, PnLInfo>& pnl_info() const;
  
  const std::unordered_map<std::string, double>& last_prices() const;
 private:
  RiskConfig cfg_;
  std::unordered_map<std::string, int> pos_;
  std::unordered_map<std::string, int> orders_this_bar_;
  std::unordered_map<std::string, std::chrono::steady_clock::time_point> last_order_time_;
  std::unordered_map<std::string, OrderRequest> pending_orders_;
  
  std::unordered_map<std::string, PositionDetail> pos_detail_;
  
  std::unordered_map<std::string, PnLInfo> pnl_;
  
  std::unordered_map<std::string, double> last_price_;
};
}