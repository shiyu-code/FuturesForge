#include "TradingSystem/TraderProxy.h"
#include <chrono>

namespace ts {
TraderProxy::TraderProxy(std::unique_ptr<ITrader> inner, RiskManager* risk)
    : inner_(std::move(inner)), risk_(risk) {}

bool TraderProxy::connect(const std::string& front_addr) {
  return inner_->connect(front_addr);
}

bool TraderProxy::login(const std::string& broker_id, const std::string& user_id, const std::string& password) {
  return inner_->login(broker_id, user_id, password);
}

std::string TraderProxy::place_order(const OrderRequest& req) {
  std::string reason;
  if (!risk_->can_place(req, &reason)) {
    OrderStatusEvent ev;
    ev.order_id = "REJECT_" + req.instrument + "_" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
    ev.status = "Rejected";
    ev.message = reason;
    emit_order_status(ev);
    return ev.order_id;
  }
  // 先暂存请求，用于在Accepted事件到来时注册ID
  pending_register_reqs_.push_back(req);
  auto id = inner_->place_order(req);
  risk_->on_order_placed(req.instrument);
  // 回退保障：若未在Accepted事件处注册，仍进行一次注册
  risk_->register_order(id, req);
  return id;
}

bool TraderProxy::cancel_order(const std::string& order_id) {
  return inner_->cancel_order(order_id);
}

void TraderProxy::set_order_status_handler(OrderStatusHandler handler) {
  user_handler_ = handler;
  // 将内部交易的订单状态回调转发到代理层，以便更新风险并通知上层
  inner_->set_order_status_handler([this](const OrderStatusEvent& ev){
    if (risk_) {
      // 在Accepted到来时绑定ID与原始请求，避免回测撮合同步事件先于注册的问题
      if (ev.status == "Accepted" && !pending_register_reqs_.empty()) {
        auto req = pending_register_reqs_.front();
        pending_register_reqs_.pop_front();
        risk_->register_order(ev.order_id, req);
      }
      risk_->on_order_status(ev);
    }
    emit_order_status(ev);
  });
}

void TraderProxy::emit_order_status(const OrderStatusEvent& ev) {
  if (user_handler_) user_handler_(ev);
}

void TraderProxy::on_market_data(const MarketDataEvent& ev) {
  if (auto bm = dynamic_cast<IBacktestMatching*>(inner_.get())) {
    bm->on_market_data(ev);
  }
}

void TraderProxy::configure_backtest(const std::string& meta_path, const std::string& rules_path) {
  if (auto bm = dynamic_cast<IBacktestMatching*>(inner_.get())) {
    bm->configure(meta_path, rules_path);
  }
}

} // namespace ts