#pragma once
#ifdef USE_CTP
#include "TradingSystem/ITrader.h"
#include "ThostTraderApi.h"
#include <string>

namespace ts {
class CtpTrader : public ITrader, public CThostFtdcTraderSpi {
 public:
  CtpTrader();
  ~CtpTrader() override;

  bool connect(const std::string& front) override;
  bool login(const std::string& broker_id, const std::string& user_id, const std::string& password) override;
  std::string place_order(const OrderRequest& req) override;
  bool cancel_order(const std::string& order_id) override;
  void set_order_status_handler(OrderStatusHandler handler) override;

  // SPI callbacks
  void OnFrontConnected() override;
  void OnFrontDisconnected(int nReason) override;
  void OnRspUserLogin(CThostFtdcRspUserLoginField* pRspUserLogin,
                      CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast) override;
  void OnRtnOrder(CThostFtdcOrderField* pOrder) override;
  void OnRtnTrade(CThostFtdcTradeField* pTrade) override;

 private:
  OrderStatusHandler handler_;
  CThostFtdcTraderApi* api_{nullptr};
  int req_id_{0};
  std::string broker_, user_, pass_;
};
}
#endif