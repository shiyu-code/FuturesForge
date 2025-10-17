#include "TradingSystem/Engine.h"
#include "TradingSystem/Strategy.h"
#include "TradingSystem/IMarketData.h"
#include "TradingSystem/ITrader.h"
#include "TradingSystem/Event.h"
#include "TradingSystem/ConfigUtil.h"
#include "TradingSystem/BacktestMarketData.h"
#include "TradingSystem/BacktestTrader.h"
#include "stub/StubMarketData.h"
#include "stub/StubTrader.h"
#ifdef USE_CTP
#include "ctp/CtpMarketData.h"
#include "ctp/CtpTrader.h"
#endif
#include <iostream>
#include <memory>
#include <deque>
#include <unordered_map>

using namespace ts;

class DualMAStrategy : public Strategy {
public:
  DualMAStrategy(int fast, int slow, double slippage)
    : fast_(fast), slow_(slow), slip_(slippage) {
      if (fast_ < 1) fast_ = 1; if (slow_ < fast_) slow_ = fast_ + 1;
    }
  void on_market_data(const MarketDataEvent& md, ITrader* trader) override {
    // 本策略基于Bar触发；Tick仅用于日志或扩展（此处空实现）
    (void)md; (void)trader;
  }
  void on_bar(const BarEvent& bar, ITrader* trader) override {
    auto& dq = bars_[bar.instrument];
    dq.push_back(bar);
    if ((int)dq.size() > slow_) dq.pop_front();
    if ((int)dq.size() < slow_) return;

    double fast_ma = ma(dq, fast_);
    double slow_ma = ma(dq, slow_);

    auto& pos = position_[bar.instrument];
    if (fast_ma > slow_ma && pos <= 0) {
      OrderRequest req{bar.instrument, Direction::Buy, Offset::Open, OrderType::Limit, bar.close + slip_, 1};
      trader->place_order(req);
      pos += 1;
    } else if (fast_ma < slow_ma && pos >= 0) {
      OrderRequest req{bar.instrument, Direction::Sell, Offset::Open, OrderType::Limit, bar.close - slip_, 1};
      trader->place_order(req);
      pos -= 1;
    }
  }
  void on_order_status(const OrderStatusEvent& ev) override {
    std::cout << "[Strategy] OrderStatus id=" << ev.order_id
              << " status=" << ev.status
              << " inst=" << ev.instrument
              << " qty=" << ev.filled_qty
              << " px=" << ev.fill_price
              << " remaining=" << ev.remaining_qty
              << " msg=" << ev.message << "\n";
  }
private:
  static double ma(const std::deque<BarEvent>& dq, int n) {
    double s = 0.0; int cnt = 0;
    for (int i = (int)dq.size() - n; i < (int)dq.size(); ++i) { if (i >= 0) { s += dq[i].close; ++cnt; } }
    return cnt ? s / cnt : 0.0;
  }
  int fast_, slow_;
  double slip_;
  std::unordered_map<std::string, std::deque<BarEvent>> bars_;
  std::unordered_map<std::string, int> position_;
};

int main(int argc, char* argv[]) {
  // 解析命令行参数：支持 -c/--config 指定配置文件路径
  std::string cfg_path = "config.ini";
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if ((arg == "-c" || arg == "--config") && i + 1 < argc) {
      cfg_path = argv[++i];
    }
  }

  bool ok = false;
  AppConfig cfg = load_app_config(cfg_path, &ok);
  if (!ok) {
    std::cout << "[Main] Using default demo config" << std::endl;
  }

  std::unique_ptr<IMarketData> md;
  std::unique_ptr<ITrader> td;
  if (cfg.use_backtest) {
    md = std::make_unique<BacktestMarketData>(cfg.backtest_speed_ms);
    td = std::make_unique<BacktestTrader>();
    // 将md_front改为CSV路径以兼容引擎连接流程
    if (!cfg.backtest_file.empty()) cfg.md_front = cfg.backtest_file;
  }
#ifdef USE_CTP
  else if (cfg.use_ctp) {
    md = std::make_unique<CtpMarketData>();
    td = std::make_unique<CtpTrader>();
  }
#endif
  else {
    md = std::make_unique<StubMarketData>();
    td = std::make_unique<StubTrader>();
  }

  auto strat = std::make_unique<DualMAStrategy>(cfg.strat_ma_fast, cfg.strat_ma_slow, cfg.strat_threshold);
  Engine eng{cfg, std::move(md), std::move(td), std::move(strat)};
  return eng.run();
}