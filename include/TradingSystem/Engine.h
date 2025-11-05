#pragma once
#include <memory>
#include <vector>
#include <string>
#include "IMarketData.h"
#include "ITrader.h"
#include "Strategy.h"

namespace ts {

struct AppConfig {
  bool use_ctp{false};
  bool use_backtest{false};
  std::string md_front;
  std::string td_front;
  std::string broker_id;
  std::string user_id;
  std::string password;
  std::vector<std::string> instruments;
  int bar_interval_sec{1};
  
  int max_pos_per_instrument{1};
  int max_orders_per_bar{1};
  int min_order_interval_ms{500};
  
  std::string backtest_file;
  int backtest_speed_ms{5};
  std::string backtest_meta;    
  std::string backtest_rules;   
  
  int run_seconds{20};
  bool enable_csv_logs{true};
  std::string csv_dir{"data"};
  
  int strat_ma_fast{3};
  int strat_ma_slow{8};
  double strat_threshold{0.5};
};

class Engine {
 public:
  Engine(AppConfig cfg,
         std::unique_ptr<IMarketData> md,
         std::unique_ptr<ITrader> td,
         std::unique_ptr<Strategy> strat);
  int run();
 private:
  AppConfig cfg_;
  std::unique_ptr<IMarketData> md_;
  std::unique_ptr<ITrader> td_;
  std::unique_ptr<Strategy> strat_;
};

} 