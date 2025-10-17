#include "TradingSystem/Engine.h"
#include "TradingSystem/BarAggregator.h"
#include "TradingSystem/RiskManager.h"
#include "TradingSystem/TraderProxy.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <unordered_map>
#include <fstream>
#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <ctime>

namespace ts {

Engine::Engine(AppConfig cfg,
               std::unique_ptr<IMarketData> md,
               std::unique_ptr<ITrader> td,
               std::unique_ptr<Strategy> strat)
    : cfg_(std::move(cfg)), md_(std::move(md)), td_(std::move(td)), strat_(std::move(strat)) {}

int Engine::run() {
  // 连接行情与交易
#ifdef USE_CTP
  if (cfg_.use_ctp) {
    if (!md_->connect(cfg_.md_front)) {
      std::cerr << "[Engine] Market data connect failed: " << cfg_.md_front << "\n";
      return 1;
    }
    if (!td_->connect(cfg_.td_front)) {
      std::cerr << "[Engine] Trader connect failed: " << cfg_.td_front << "\n";
      return 1;
    }
  } else {
#endif
    // 在回测模式下，md_front承载CSV路径
    if (!md_->connect(cfg_.md_front)) {
      std::cerr << "[Engine] Market data connect failed: " << cfg_.md_front << "\n";
      return 1;
    }
    if (!td_->connect(cfg_.use_backtest ? "backtest" : "stub")) {
      std::cerr << "[Engine] Trader connect failed\n";
      return 1;
    }
#ifdef USE_CTP
  }
#endif

  RiskManager risk(RiskConfig{cfg_.max_pos_per_instrument, cfg_.max_orders_per_bar, cfg_.min_order_interval_ms});
  // 用交易代理包装底层交易接口，加入风控
  td_.reset(new TraderProxy(std::move(td_), &risk));
  // 回测模式下，加载撮合配置
  if (cfg_.use_backtest) {
    if (auto proxy = dynamic_cast<TraderProxy*>(td_.get())) {
      proxy->configure_backtest(cfg_.backtest_meta, cfg_.backtest_rules);
    }
  }

  BarAggregator bar_agg(cfg_.bar_interval_sec);
  bar_agg.set_bar_handler([this, &risk](const BarEvent& bar) {
    risk.on_new_bar(bar.instrument);
    if (strat_) strat_->on_bar(bar, td_.get());
  });

  // 订单事件CSV日志
  std::string csv_dir_cfg = cfg_.csv_dir.empty() ? std::string("data") : cfg_.csv_dir;
  std::filesystem::path csv_path(csv_dir_cfg);
  std::string csv_dir = std::filesystem::absolute(csv_path).string();
  std::cout << "[Engine] csv_dir resolved: " << csv_dir << " (from " << csv_dir_cfg << ")\n";
  // 确保CSV目录存在（避免在不同工作目录下写文件失败）
  std::error_code ec;
  std::filesystem::create_directories(csv_dir, ec);
  if (ec) {
    std::cerr << "[Engine] ensure csv_dir failed: " << csv_dir << " error=" << ec.message() << "\n";
  }
  std::ofstream trade_log(csv_dir + "/trade_log.csv");
  if (cfg_.enable_csv_logs && trade_log) {
    trade_log << "order_id,status,instrument,filled_qty,fill_price,remaining_qty,message\n";
  }

  md_->set_market_data_handler([this, &bar_agg, &risk](const MarketDataEvent& md_ev) {
    if (strat_) {
      strat_->on_market_data(md_ev, td_.get());
    }
    // 将行情转发给交易代理，用于回测撮合
    if (auto proxy = dynamic_cast<TraderProxy*>(td_.get())) {
      proxy->on_market_data(md_ev);
    }
    // 风控接收行情以追踪最新价和浮盈
    risk.on_market_data(md_ev);
    bar_agg.on_tick(md_ev);
  });

  // 简单成交统计（按合约累计成交量与成交金额）
  std::unordered_map<std::string, std::pair<long, double>> stats;
  td_->set_order_status_handler([this, &stats, &trade_log](const OrderStatusEvent& ev) {
     std::cout << "[OrderStatus] id=" << ev.order_id
               << " status=" << ev.status
               << " inst=" << ev.instrument
               << " qty=" << ev.filled_qty
               << " px=" << ev.fill_price
               << " remaining=" << ev.remaining_qty
               << " msg=" << ev.message << std::endl;
    if (cfg_.enable_csv_logs && trade_log) {
      trade_log << ev.order_id << "," << ev.status << "," << ev.instrument << ","
                << ev.filled_qty << "," << ev.fill_price << "," << ev.remaining_qty << "," << ev.message << "\n";
    }
     if ((ev.status == "Filled" || ev.status == "PartiallyFilled") && ev.filled_qty > 0 && !ev.instrument.empty()) {
       auto &s = stats[ev.instrument];
       s.first += ev.filled_qty;
       s.second += ev.filled_qty * ev.fill_price;
     }
     if (strat_) { strat_->on_order_status(ev); }
   });

  if (!md_->login(cfg_.broker_id, cfg_.user_id, cfg_.password)) {
    std::cerr << "[Engine] Market data login failed\n";
    return 1;
  }
  if (!td_->login(cfg_.broker_id, cfg_.user_id, cfg_.password)) {
    std::cerr << "[Engine] Trader login failed\n";
    return 1;
  }

  if (!md_->subscribe(cfg_.instruments)) {
    std::cerr << "[Engine] Subscribe failed\n";
    return 1;
  }

  // 主循环：等待行情线程运行完成（stub/backtest内部管理线程）
  std::this_thread::sleep_for(std::chrono::seconds(cfg_.run_seconds));

  // 汇总成交均价CSV
  if (cfg_.enable_csv_logs) {
    std::ofstream sum(csv_dir + "/trade_summary.csv");
    if (sum) {
      sum << "instrument,total_qty,avg_price\n";
      for (const auto& kv : stats) {
        double avg = (kv.second.first > 0 ? kv.second.second / kv.second.first : 0.0);
        sum << kv.first << "," << kv.second.first << "," << avg << "\n";
      }
    }
    // 持仓快照CSV
    std::ofstream pos(csv_dir + "/positions.csv");
    if (pos) {
      pos << "instrument,net_pos\n";
      for (const auto& kv : risk.positions()) {
        pos << kv.first << "," << kv.second << "\n";
      }
    }
    // 持仓明细CSV
    std::ofstream posd(csv_dir + "/positions_detail.csv");
    if (posd) {
      posd << "instrument,long_pos,short_pos,net_pos\n";
      for (const auto& kv : risk.positions_detail()) {
        int net = kv.second.long_qty - kv.second.short_qty;
        posd << kv.first << "," << kv.second.long_qty << "," << kv.second.short_qty << "," << net << "\n";
      }
    }
    // 盈亏报表CSV（包含已实现与未实现盈亏，以及库存均价）
    std::string pnl_path = csv_dir + "/pnl.csv";
    std::ofstream pnl(pnl_path);
    if (pnl) {
      pnl << "instrument,realized_pnl,unrealized_pnl,long_open_qty,long_avg_cost,short_open_qty,short_avg_cost\n";
      for (const auto& kv : risk.pnl_info()) {
        const auto &info = kv.second;
        double long_avg = (info.long_open_qty > 0 ? info.long_open_cost_sum / info.long_open_qty : 0.0);
        double short_avg = (info.short_open_qty > 0 ? info.short_open_cost_sum / info.short_open_qty : 0.0);
        pnl << kv.first << "," << info.realized_pnl << "," << info.unrealized_pnl << "," << info.long_open_qty << "," << long_avg
            << "," << info.short_open_qty << "," << short_avg << "\n";
      }
    } else {
      // 回退：若pnl.csv被占用（无法打开），写入时间戳文件
      auto now = std::chrono::system_clock::now();
      std::time_t tt = std::chrono::system_clock::to_time_t(now);
      std::tm tm{};
#ifdef _WIN32
      localtime_s(&tm, &tt);
#else
      localtime_r(&tm, &tt);
#endif
      std::ostringstream ts;
      ts << std::put_time(&tm, "%Y%m%d_%H%M%S");
      std::string fb_path = csv_dir + "/pnl_" + ts.str() + ".csv";
      std::ofstream pnl_fb(fb_path);
      if (pnl_fb) {
        pnl_fb << "instrument,realized_pnl,unrealized_pnl,long_open_qty,long_avg_cost,short_open_qty,short_avg_cost\n";
        for (const auto& kv : risk.pnl_info()) {
          const auto &info = kv.second;
          double long_avg = (info.long_open_qty > 0 ? info.long_open_cost_sum / info.long_open_qty : 0.0);
          double short_avg = (info.short_open_qty > 0 ? info.short_open_cost_sum / info.short_open_qty : 0.0);
          pnl_fb << kv.first << "," << info.realized_pnl << "," << info.unrealized_pnl << "," << info.long_open_qty << "," << long_avg
                 << "," << info.short_open_qty << "," << short_avg << "\n";
        }
        std::cerr << "[Engine] pnl.csv open failed; wrote fallback: " << fb_path << "\n";
      } else {
        std::cerr << "[Engine] cannot open pnl.csv nor fallback. dir=" << csv_dir << "\n";
      }
    }
  }

  return 0;
}

} // namespace ts