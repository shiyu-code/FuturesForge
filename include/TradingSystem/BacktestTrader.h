#pragma once
#include <string>
#include <unordered_map>
#include <deque>
#include "FuturesForge/ITrader.h"
#include "FuturesForge/IBacktestMatching.h"
#include "FuturesForge/Event.h"

namespace ts {

struct InstrumentMeta {
  double tick_size{1.0};
  int contract_multiplier{1};
  double slippage_tick{0.0};
};

class BacktestTrader : public ITrader, public IBacktestMatching {
 public:
  BacktestTrader() = default;
  ~BacktestTrader() override = default;

  bool connect(const std::string& front) override;
  bool login(const std::string& broker_id, const std::string& user_id, const std::string& password) override;
  void set_order_status_handler(OrderStatusHandler handler) override;

  std::string place_order(const OrderRequest& order) override;
  bool cancel_order(const std::string& order_id) override;

  // IBacktestMatching
  void on_market_data(const MarketDataEvent& ev) override;
  void configure(const std::string& meta_path, const std::string& rules_path) override;

 private:
  struct Tick {
    double bid{0.0};
    double ask{0.0};
    int bid_vol{0};
    int ask_vol{0};
    double last{0.0};
    std::string ts;
  };
  struct OrderRec {
    std::string id;
    OrderRequest req;
    int remaining{0};
    std::string ts;
  };

  OrderStatusHandler handler_;
  std::unordered_map<std::string, Tick> last_tick_;
  std::unordered_map<std::string, std::deque<OrderRec>> pending_; // 每合约的挂单队列（FIFO）
  std::unordered_map<std::string, InstrumentMeta> meta_;

  // 规则简版
  bool partial_fill_{true};
  double global_slippage_tick_{0.0};

  // 内部辅助
  double tick_size(const std::string& instr) const;
  double slippage_tick(const std::string& instr) const;
  void try_match(const std::string& instr, const Tick& tk);
  void emit_status(const std::string& id, const std::string& status, const std::string& msg,
                   const std::string& instrument = "",
                   int filled_qty = 0,
                   double fill_price = 0.0,
                   int remaining_qty = -1);
};

} // namespace ts