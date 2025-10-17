#include "TradingSystem/ConfigUtil.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <iostream>

namespace ts {
namespace {
  std::string trim(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e-1]))) --e;
    return s.substr(b, e - b);
  }
  bool parse_bool(const std::string& v) {
    std::string s;
    s.reserve(v.size());
    for (char c : v) s.push_back(std::tolower(static_cast<unsigned char>(c)));
    return (s == "1" || s == "true" || s == "yes" || s == "on");
  }
}

AppConfig load_app_config(const std::string& path, bool* ok) {
  AppConfig cfg;
  std::ifstream ifs(path);
  if (!ifs.good()) {
    if (ok) *ok = false;
    std::cerr << "[Config] cannot open " << path << ", using defaults" << std::endl;
    // 默认值；保持与示例main一致
    cfg.use_ctp = false;
    cfg.md_front = "stub://md";
    cfg.td_front = "stub://td";
    cfg.broker_id = "9999";
    cfg.user_id = "demo";
    cfg.password = "pass";
    cfg.instruments = {"IF2411", "rb2410"};
    return cfg;
  }

  if (ok) *ok = true;
  std::string line;
  while (std::getline(ifs, line)) {
    line = trim(line);
    if (line.empty() || line[0] == '#' || line[0] == ';') continue;
    auto pos = line.find('=');
    if (pos == std::string::npos) continue;
    std::string key = trim(line.substr(0, pos));
    std::string val = trim(line.substr(pos + 1));
    if (key == "use_ctp") cfg.use_ctp = parse_bool(val);
    else if (key == "use_backtest") cfg.use_backtest = parse_bool(val);
    else if (key == "md_front") cfg.md_front = val;
    else if (key == "td_front") cfg.td_front = val;
    else if (key == "broker_id") cfg.broker_id = val;
    else if (key == "user_id") cfg.user_id = val;
    else if (key == "password") cfg.password = val;
    else if (key == "instruments") {
      cfg.instruments.clear();
      std::stringstream ss(val);
      std::string tok;
      while (std::getline(ss, tok, ',')) {
        tok = trim(tok);
        if (!tok.empty()) cfg.instruments.push_back(tok);
      }
    } else if (key == "bar_interval_sec") {
      try { cfg.bar_interval_sec = std::max(1, std::stoi(val)); }
      catch (...) { /* keep default */ }
    } else if (key == "max_pos_per_instrument") {
      try { cfg.max_pos_per_instrument = std::max(0, std::stoi(val)); }
      catch (...) { /* keep default */ }
    } else if (key == "max_orders_per_bar") {
      try { cfg.max_orders_per_bar = std::max(0, std::stoi(val)); }
      catch (...) { /* keep default */ }
    } else if (key == "min_order_interval_ms") {
      try { cfg.min_order_interval_ms = std::max(0, std::stoi(val)); }
      catch (...) { /* keep default */ }
    } else if (key == "backtest_file") {
      cfg.backtest_file = val;
    } else if (key == "backtest_speed_ms") {
      try { cfg.backtest_speed_ms = std::max(1, std::stoi(val)); }
      catch (...) { /* keep default */ }
    } else if (key == "backtest_meta") {
      cfg.backtest_meta = val;
    } else if (key == "backtest_rules") {
      cfg.backtest_rules = val;
    } else if (key == "run_seconds") {
      try { cfg.run_seconds = std::max(1, std::stoi(val)); }
      catch (...) { /* keep default */ }
    } else if (key == "enable_csv_logs") {
      cfg.enable_csv_logs = parse_bool(val);
    } else if (key == "csv_dir") {
      cfg.csv_dir = val;
    } else if (key == "strat_ma_fast") {
      try { cfg.strat_ma_fast = std::max(1, std::stoi(val)); }
      catch (...) { /* keep default */ }
    } else if (key == "strat_ma_slow") {
      try { cfg.strat_ma_slow = std::max(1, std::stoi(val)); }
      catch (...) { /* keep default */ }
    } else if (key == "strat_threshold") {
      try { cfg.strat_threshold = std::stod(val); }
      catch (...) { /* keep default */ }
    }
  }
  return cfg;
}

} // namespace ts