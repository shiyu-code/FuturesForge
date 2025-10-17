#pragma once
#ifdef USE_CTP
#include "TradingSystem/IMarketData.h"
#include <string>

// CTP headers
#include "ThostMDUserApi.h"

namespace ts {
class CtpMarketData : public IMarketData, public CThostFtdcMdSpi {
 public:
  CtpMarketData();
  ~CtpMarketData() override;

  bool connect(const std::string& front) override;
  bool login(const std::string& broker_id, const std::string& user_id, const std::string& password) override;
  bool subscribe(const std::vector<std::string>& instruments) override;
  void set_market_data_handler(MarketDataHandler handler) override;

  // CTP SPI callbacks
  void OnFrontConnected() override;
  void OnFrontDisconnected(int nReason) override;
  void OnRspUserLogin(CThostFtdcRspUserLoginField* pRspUserLogin,
                      CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast) override;
  void OnRtnDepthMarketData(CThostFtdcDepthMarketDataField* pDepthMarketData) override;

 private:
  MarketDataHandler handler_;
  CThostFtdcMdApi* api_{nullptr};
  std::string front_;
  std::string broker_;
  std::string user_;
  std::string pass_;
};
} // namespace ts
#endif