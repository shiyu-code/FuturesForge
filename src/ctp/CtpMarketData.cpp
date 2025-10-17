#ifdef USE_CTP
#include "ctp/CtpMarketData.h"
#include "TradingSystem/Event.h"
#include <iostream>
#include <cstring>

namespace ts {

CtpMarketData::CtpMarketData() {
  api_ = CThostFtdcMdApi::CreateFtdcMdApi();
  api_->RegisterSpi(this);
}

CtpMarketData::~CtpMarketData() {
  if (api_) {
    api_->Release();
    api_ = nullptr;
  }
}

bool CtpMarketData::connect(const std::string& front) {
  front_ = front;
  api_->RegisterFront(const_cast<char*>(front_.c_str()));
  api_->Init();
  std::cout << "[CTP MD] Connecting to " << front_ << std::endl;
  return true;
}

bool CtpMarketData::login(const std::string& broker_id, const std::string& user_id, const std::string& password) {
  broker_ = broker_id; user_ = user_id; pass_ = password;
  CThostFtdcReqUserLoginField req{};
  strncpy(req.BrokerID, broker_.c_str(), sizeof(req.BrokerID));
  strncpy(req.UserID, user_.c_str(), sizeof(req.UserID));
  strncpy(req.Password, pass_.c_str(), sizeof(req.Password));
  int ret = api_->ReqUserLogin(&req, 1);
  std::cout << "[CTP MD] Send login request ret=" << ret << std::endl;
  return ret == 0;
}

bool CtpMarketData::subscribe(const std::vector<std::string>& instruments) {
  std::vector<char*> buf;
  buf.reserve(instruments.size());
  for (auto& s : instruments) {
    buf.push_back(const_cast<char*>(s.c_str()));
  }
  int ret = api_->SubscribeMarketData(buf.data(), static_cast<int>(buf.size()));
  std::cout << "[CTP MD] Subscribe ret=" << ret << std::endl;
  return ret == 0;
}

void CtpMarketData::set_market_data_handler(MarketDataHandler handler) {
  handler_ = std::move(handler);
}

void CtpMarketData::OnFrontConnected() {
  std::cout << "[CTP MD] Front connected" << std::endl;
}

void CtpMarketData::OnFrontDisconnected(int nReason) {
  std::cout << "[CTP MD] Front disconnected reason=" << nReason << std::endl;
}

void CtpMarketData::OnRspUserLogin(CThostFtdcRspUserLoginField* pRspUserLogin,
                      CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast) {
  std::cout << "[CTP MD] Login response: ErrorID=" << (pRspInfo ? pRspInfo->ErrorID : 0)
            << " Msg=" << (pRspInfo ? pRspInfo->ErrorMsg : "") << std::endl;
}

void CtpMarketData::OnRtnDepthMarketData(CThostFtdcDepthMarketDataField* p) {
  if (!handler_ || !p) return;
  MarketDataEvent ev;
  ev.instrument = p->InstrumentID;
  ev.last_price = p->LastPrice;
  ev.bid_price = p->BidPrice1;
  ev.ask_price = p->AskPrice1;
  ev.volume = p->Volume;
  ev.update_time = p->UpdateTime;
  handler_(ev);
}

} // namespace ts
#endif