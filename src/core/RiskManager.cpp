#include "TradingSystem/RiskManager.h"
#include <algorithm>
#include <iostream>

namespace ts {
RiskManager::RiskManager(RiskConfig cfg) : cfg_(cfg) {}

bool RiskManager::can_place(const OrderRequest& req, std::string* reject_reason) {
  const auto& inst = req.instrument;
  auto now = std::chrono::steady_clock::now();
  // 每Bar限单
  int used = orders_this_bar_[inst];
  if (used >= cfg_.max_orders_per_bar) {
    if (reject_reason) *reject_reason = "Exceeded max orders per bar";
    return false;
  }
  // 最小间隔
  auto it = last_order_time_.find(inst);
  if (it != last_order_time_.end()) {
    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second).count();
    if (diff < cfg_.min_order_interval_ms) {
      if (reject_reason) *reject_reason = "Order interval too short";
      return false;
    }
  }
  // 最大持仓（仅针对开仓）
  bool is_open = (req.offset == Offset::Open);
  if (is_open) {
    int cur = pos_[inst];
    // 买开视为+1，卖开视为-1；限制绝对值
    int trial = cur + (req.direction == Direction::Buy ? 1 : -1);
    if (std::abs(trial) > cfg_.max_pos_per_instrument) {
      if (reject_reason) *reject_reason = "Exceeded max position per instrument";
      return false;
    }
  }
  return true;
}

void RiskManager::on_order_placed(const std::string& instrument) {
  orders_this_bar_[instrument]++;
  last_order_time_[instrument] = std::chrono::steady_clock::now();
}

void RiskManager::on_new_bar(const std::string& instrument) {
  orders_this_bar_[instrument] = 0;
}

void RiskManager::register_order(const std::string& order_id, const OrderRequest& req) {
  pending_orders_[order_id] = req;
}

void RiskManager::on_order_status(const OrderStatusEvent& ev) {
  auto it = pending_orders_.find(ev.order_id);
  if (it == pending_orders_.end()) {
    std::cerr << "[Risk] unmatched order_id=" << ev.order_id
              << " status=" << ev.status
              << " inst=" << ev.instrument << std::endl;
    return;
  }
  const OrderRequest& req = it->second;
  const auto& inst = req.instrument;
  // 根据结构化字段更新持仓与盈亏，支持部分成交
  if ((ev.status == "Filled" || ev.status == "PartiallyFilled") && ev.filled_qty > 0) {
    int qty = ev.filled_qty;
    double px = ev.fill_price;
    int delta = (req.direction == Direction::Buy ? qty : -qty);
    auto &d = pos_detail_[inst];
    auto &p = pnl_[inst];
    if (req.offset == Offset::Open) {
      pos_[inst] += delta;
      if (req.direction == Direction::Buy) {
        d.long_qty += qty;
        p.long_open_qty += qty;
        p.long_open_cost_sum += qty * px;
      } else {
        d.short_qty += qty;
        p.short_open_qty += qty;
        p.short_open_cost_sum += qty * px;
      }
    } else { // Close
      pos_[inst] -= delta; // 对冲减少净持仓
      if (req.direction == Direction::Buy) {
        // 平空：以平均空头成本结算
        int avail = std::max(0, p.short_open_qty);
        double avg_short = (avail > 0 ? p.short_open_cost_sum / avail : 0.0);
        int used = std::min(qty, avail);
        p.realized_pnl += used * (avg_short - px);
        // 更新空头库存
        p.short_open_qty = std::max(0, avail - used);
        p.short_open_cost_sum = std::max(0.0, p.short_open_cost_sum - used * avg_short);
        // 明细减少空头
        d.short_qty = std::max(0, d.short_qty - used);
      } else {
        // 平多：以平均多头成本结算
        int avail = std::max(0, p.long_open_qty);
        double avg_long = (avail > 0 ? p.long_open_cost_sum / avail : 0.0);
        int used = std::min(qty, avail);
        p.realized_pnl += used * (px - avg_long);
        // 更新多头库存
        p.long_open_qty = std::max(0, avail - used);
        p.long_open_cost_sum = std::max(0.0, p.long_open_cost_sum - used * avg_long);
        // 明细减少多头
        d.long_qty = std::max(0, d.long_qty - used);
      }
    }
  }
  if (ev.status == "Filled") {
    pending_orders_.erase(it);
  } else if (ev.status == "Canceled" || ev.status == "Rejected") {
    pending_orders_.erase(it);
  }
}

void RiskManager::on_market_data(const MarketDataEvent& ev) {
  if (ev.instrument.empty()) return;
  last_price_[ev.instrument] = ev.last_price;
  // 计算并更新未实现盈亏（基于最新价与库存均价）
  auto pit = pnl_.find(ev.instrument);
  if (pit != pnl_.end()) {
    auto &p = pit->second;
    double last = ev.last_price;
    double long_avg = (p.long_open_qty > 0 ? p.long_open_cost_sum / p.long_open_qty : 0.0);
    double short_avg = (p.short_open_qty > 0 ? p.short_open_cost_sum / p.short_open_qty : 0.0);
    p.unrealized_pnl = (last - long_avg) * static_cast<double>(std::max(0, p.long_open_qty))
                     + (short_avg - last) * static_cast<double>(std::max(0, p.short_open_qty));
  }
}

const std::unordered_map<std::string, int>& RiskManager::positions() const {
  return pos_;
}

const std::unordered_map<std::string, RiskManager::PositionDetail>& RiskManager::positions_detail() const {
  return pos_detail_;
}

const std::unordered_map<std::string, RiskManager::PnLInfo>& RiskManager::pnl_info() const {
  return pnl_;
}

const std::unordered_map<std::string, double>& RiskManager::last_prices() const {
  return last_price_;
}

} // namespace ts