#include "TradingSystem/BacktestTrader.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdint>
#include <cmath>

namespace ts {

bool BacktestTrader::connect(const std::string& front) {
  std::cout << "[BTTR] Connect to " << front << std::endl;
  return true;
}

bool BacktestTrader::login(const std::string& broker_id, const std::string& user_id, const std::string& password) {
  std::cout << "[BTTR] Login (noop)" << std::endl;
  return true;
}

void BacktestTrader::set_order_status_handler(OrderStatusHandler handler) {
  handler_ = std::move(handler);
}

static std::string gen_id() {
  static uint64_t n = 0;
  return std::string("BT_") + std::to_string(++n);
}

std::string BacktestTrader::place_order(const OrderRequest& order) {
  OrderRec rec;
  rec.id = gen_id();
  rec.req = order;
  rec.remaining = order.volume;
  emit_status(rec.id, "Accepted", "accepted", order.instrument, 0, 0.0, rec.remaining);
  // 立即处理FOK/IOC
  auto it = last_tick_.find(order.instrument);
  if (it != last_tick_.end()) {
    const auto& tk = it->second;
    // FOK：仅当能完全成交时执行，否则拒绝
    if (order.type == OrderType::FOK) {
      int avail = (order.direction == Direction::Buy) ? tk.ask_vol : tk.bid_vol;
      bool cross = (order.direction == Direction::Buy) ? (tk.ask <= order.price) : (tk.bid >= order.price);
      if (order.type == OrderType::Market) cross = true; // 市价必交叉
      if (avail >= order.volume && (order.type == OrderType::Market || cross)) {
        pending_[order.instrument].push_back(rec);
        try_match(order.instrument, tk);
      } else {
        emit_status(rec.id, "Rejected", "FOK not fully matchable", order.instrument, 0, 0.0, rec.remaining);
      }
      return rec.id;
    }
    if (order.type == OrderType::IOC) {
      pending_[order.instrument].push_back(rec);
      try_match(order.instrument, tk);
      // 剩余部分立即取消
      auto& q = pending_[order.instrument];
      if (!q.empty() && q.front().id == rec.id && q.front().remaining > 0) {
        emit_status(rec.id, "Canceled", "IOC remainder canceled", order.instrument, 0, 0.0, q.front().remaining);
        q.pop_front();
      }
      return rec.id;
    }
    // Limit/Market：入队，等待tick撮合或立即尝试
    pending_[order.instrument].push_back(rec);
    try_match(order.instrument, tk);
  } else {
    // 无行情，直接入队
    pending_[order.instrument].push_back(rec);
  }
  return rec.id;
}

bool BacktestTrader::cancel_order(const std::string& order_id) {
  for (auto& kv : pending_) {
    auto& q = kv.second;
    for (auto it = q.begin(); it != q.end(); ++it) {
      if (it->id == order_id) {
        emit_status(order_id, "Canceled", "user canceled", kv.first, 0, 0.0, it->remaining);
        q.erase(it);
        return true;
      }
    }
  }
  return false;
}

void BacktestTrader::on_market_data(const MarketDataEvent& ev) {
  Tick tk{ev.bid_price, ev.ask_price, ev.bid_volume, ev.ask_volume, ev.last_price, ev.update_time};
  last_tick_[ev.instrument] = tk;
  try_match(ev.instrument, tk);
}

void BacktestTrader::configure(const std::string& meta_path, const std::string& rules_path) {
  // 朴素解析：从meta读取tick_size、multiplier、slippage_tick；从rules读取global slippage与partial_fill
  // meta示例：[{"instrument":"IF2401","tick_size":0.2,"contract_multiplier":300,"slippage_tick":0.5}, ...]
  std::ifstream mf(meta_path);
  if (mf.good()) {
    std::stringstream buf; buf << mf.rdbuf();
    std::string s = buf.str();
    // 简单逐项扫描（不严谨，但可用）
    size_t pos = 0;
    while ((pos = s.find("instrument\":\"", pos)) != std::string::npos) {
      pos += 13; size_t end = s.find('"', pos); if (end == std::string::npos) break; std::string instr = s.substr(pos, end-pos);
      InstrumentMeta m;
      // tick_size
      size_t tpos = s.find("tick_size\":", end);
      if (tpos != std::string::npos) { tpos += 11; m.tick_size = std::stod(s.substr(tpos)); }
      // contract_multiplier
      size_t mpos = s.find("contract_multiplier\":", end);
      if (mpos != std::string::npos) { mpos += 21; m.contract_multiplier = std::stoi(s.substr(mpos)); }
      // slippage_tick
      size_t spos = s.find("slippage_tick\":", end);
      if (spos != std::string::npos) { spos += 16; m.slippage_tick = std::stod(s.substr(spos)); }
      meta_[instr] = m;
      pos = end + 1;
    }
  }
  std::ifstream rf(rules_path);
  if (rf.good()) {
    std::stringstream buf; buf << rf.rdbuf();
    std::string s = buf.str();
    size_t spos = s.find("slippage_tick\":"); if (spos != std::string::npos) { spos += 16; global_slippage_tick_ = std::stod(s.substr(spos)); }
    size_t ppos = s.find("partial_fill\":"); if (ppos != std::string::npos) { ppos += 14; std::string v = s.substr(ppos, 5); partial_fill_ = (v.find("true") != std::string::npos || v.find("True") != std::string::npos); }
  }
  std::cout << "[BTTR] Loaded meta from " << meta_path << ", rules from " << rules_path << std::endl;
}
void BacktestTrader::emit_status(const std::string& id,
                                 const std::string& status,
                                 const std::string& msg,
                                 const std::string& instrument,
                                 int filled_qty,
                                 double fill_price,
                                 int remaining_qty) {
  if (handler_) {
    OrderStatusEvent ev{id, status, msg};
    ev.instrument = instrument;
    ev.filled_qty = filled_qty;
    ev.fill_price = fill_price;
    ev.remaining_qty = remaining_qty;
    handler_(ev);
  }
}

double BacktestTrader::tick_size(const std::string& instr) const {
  auto it = meta_.find(instr);
  return (it != meta_.end()) ? it->second.tick_size : 1.0;
}

double BacktestTrader::slippage_tick(const std::string& instr) const {
  auto it = meta_.find(instr);
  if (it != meta_.end() && it->second.slippage_tick > 0.0) return it->second.slippage_tick;
  return global_slippage_tick_;
}

void BacktestTrader::try_match(const std::string& instr, const Tick& tk) {
  auto itq = pending_.find(instr);
  if (itq == pending_.end()) return;
  auto& q = itq->second;
  if (q.empty()) return;

  // 可成交挂量（本tick）
  int avail_buy = tk.ask_vol; // 买单用ask侧挂量
  int avail_sell = tk.bid_vol; // 卖单用bid侧挂量

  // 遍历队列（FIFO），按价格与交叉条件撮合
  for (auto it = q.begin(); it != q.end();) {
    auto& ord = *it;
    double tsz = tick_size(instr);
    double slip = slippage_tick(instr) * tsz;

    bool is_buy = (ord.req.direction == Direction::Buy);
    bool cross = false;
    double trade_px = 0.0;
    if (ord.req.type == OrderType::Market) {
      cross = true;
      trade_px = is_buy ? (tk.ask + slip) : (tk.bid - slip);
    } else {
      if (is_buy) {
        cross = (tk.ask <= ord.req.price);
        trade_px = std::min(ord.req.price, tk.ask + slip);
      } else {
        cross = (tk.bid >= ord.req.price);
        trade_px = std::max(ord.req.price, tk.bid - slip);
      }
    }

    if (!cross) { ++it; continue; }

    int& avail = is_buy ? avail_buy : avail_sell;
    int fill_qty = 0;
    // IOC允许部分成交；FOK已在place_order阶段处理为全成或拒绝
    if (!partial_fill_ && ord.req.type != OrderType::IOC) {
      // 不允许部分成交：当侧量不足以完全成交时，跳过本tick
      if (avail < ord.remaining) { ++it; continue; }
      fill_qty = ord.remaining;
    } else {
      // 允许部分成交：尽量吃到侧量或订单剩余
      fill_qty = std::min(avail, ord.remaining);
    }

    if (fill_qty <= 0) { ++it; continue; }

    ord.remaining -= fill_qty;
    avail -= fill_qty;

    // 四舍五入到tick
    if (tsz > 0) {
      trade_px = std::round(trade_px / tsz) * tsz;
    }

    if (ord.remaining <= 0) {
      emit_status(ord.id, "Filled", "px=" + std::to_string(trade_px) + ",qty=" + std::to_string(fill_qty), instr, fill_qty, trade_px, ord.remaining);
      it = q.erase(it);
    } else {
      emit_status(ord.id, "PartiallyFilled", "px=" + std::to_string(trade_px) + ",qty=" + std::to_string(fill_qty), instr, fill_qty, trade_px, ord.remaining);
      // IOC在本tick后取消剩余
      if (ord.req.type == OrderType::IOC) {
        emit_status(ord.id, "Canceled", "IOC remainder canceled", instr, 0, 0.0, ord.remaining);
        it = q.erase(it);
      } else {
        ++it;
      }
    }

    if (avail <= 0) break; // 当前tick侧量已用尽
  }
}

} // namespace ts