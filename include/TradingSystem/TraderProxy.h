#pragma once
#include <memory>
#include <string>
#include <deque>
#include "FuturesForge/ITrader.h"
#include "FuturesForge/RiskManager.h"
#include "TradingFuturesForgeSystem/IBacktestMatching.h"

namespace ts {
class TraderProxy : public ITrader {
 public:
  TraderProxy(std::unique_ptr<ITrader> inner, RiskManager* risk);
  ~TraderProxy() override = default;

  bool connect(const std::string& front_addr) override;
  bool login(const std::string& broker_id, const std::string& user_id, const std::string& password) override;

  std::string place_order(const OrderRequest& req) override;
  bool cancel_order(const std::string& order_id) override;

  void set_order_status_handler(OrderStatusHandler handler) override;

  // Backtest辅助：将行情与配置转发给内部撮合器（若支持）
  void on_market_data(const MarketDataEvent& ev);
  void configure_backtest(const std::string& meta_path, const std::string& rules_path);
 private:
  std::unique_ptr<ITrader> inner_;
  RiskManager* risk_;
  OrderStatusHandler user_handler_;
  // 解决同步回调先于注册的问题：暂存下单请求，收到Accepted后注册
  std::deque<OrderRequest> pending_register_reqs_;
  void emit_order_status(const OrderStatusEvent& ev);
};
} // namespace ts