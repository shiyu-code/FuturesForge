#include "TradingSystem/BacktestMarketData.h"
#include "TradingSystem/Event.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <chrono>

namespace ts {
BacktestMarketData::BacktestMarketData(int speed_ms) : speed_ms_(speed_ms) {}
BacktestMarketData::~BacktestMarketData() {
  running_.store(false);
  if (worker_.joinable()) worker_.join();
}

bool BacktestMarketData::connect(const std::string& front) {
  file_ = front; // front即CSV路径
  std::cout << "[BTMD] Using file " << file_ << std::endl;
  return true;
}

bool BacktestMarketData::login(const std::string& broker_id, const std::string& user_id, const std::string& password) {
  std::cout << "[BTMD] Login (noop)" << std::endl;
  return true;
}

bool BacktestMarketData::subscribe(const std::vector<std::string>& instruments) {
  sub_set_.clear();
  for (const auto& s : instruments) sub_set_.insert(s);
  running_.store(true);
  worker_ = std::thread(&BacktestMarketData::run_loop, this);
  return true;
}

void BacktestMarketData::set_market_data_handler(MarketDataHandler handler) {
  handler_ = std::move(handler);
}

static bool looks_like_datetime(const std::string& s) {
  // 粗略判断是否为日期时间字符串
  return (s.find('-') != std::string::npos) || (s.find(':') != std::string::npos) || (s.find('T') != std::string::npos);
}

void BacktestMarketData::run_loop() {
  std::ifstream ifs(file_);
  if (!ifs.good()) {
    std::cerr << "[BTMD] Cannot open file: " << file_ << std::endl;
    running_.store(false);
    return;
  }
  std::string line;
  // 支持两种CSV：
  // 1) instrument,last_price,volume[,bid_price,ask_price,update_time]
  // 2) instrument,datetime,last_price,bid_price,ask_price,bid_volume,ask_volume,volume,[...]
  while (running_.load() && std::getline(ifs, line)) {
    if (line.empty()) continue;
    // 跳过可能的表头
    if (line.find("instrument") != std::string::npos && line.find(",") != std::string::npos) continue;
    std::stringstream ss(line);
    std::string tok;
    std::vector<std::string> cols;
    while (std::getline(ss, tok, ',')) cols.push_back(tok);
    if (cols.size() < 3) continue;

    MarketDataEvent ev;
    ev.instrument = cols[0];
    if (!sub_set_.empty() && sub_set_.find(ev.instrument) == sub_set_.end()) {
      continue; // 未订阅的合约跳过
    }

    // 判断格式
    bool fmt2 = (cols.size() >= 8 && looks_like_datetime(cols[1]));
    try {
      if (fmt2) {
        ev.update_time = cols[1];
        ev.last_price = std::stod(cols[2]);
        ev.bid_price = std::stod(cols[3]);
        ev.ask_price = std::stod(cols[4]);
        ev.bid_volume = std::stoi(cols[5]);
        ev.ask_volume = std::stoi(cols[6]);
        ev.volume = std::stoi(cols[7]);
      } else {
        ev.last_price = std::stod(cols[1]);
        ev.volume = std::stoi(cols[2]);
        if (cols.size() > 3) {
          try { ev.bid_price = std::stod(cols[3]); } catch (...) { ev.bid_price = ev.last_price - 0.5; }
        } else { ev.bid_price = ev.last_price - 0.5; }
        if (cols.size() > 4) {
          try { ev.ask_price = std::stod(cols[4]); } catch (...) { ev.ask_price = ev.last_price + 0.5; }
        } else { ev.ask_price = ev.last_price + 0.5; }
        if (cols.size() > 5) ev.update_time = cols[5]; else ev.update_time = "bt";
      }
    } catch (...) {
      continue; // 非法行
    }

    if (handler_) handler_(ev);
    std::this_thread::sleep_for(std::chrono::milliseconds(speed_ms_));
  }
  running_.store(false);
}

} // namespace ts