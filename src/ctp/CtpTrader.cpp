#ifdef USE_CTP
#include "ctp/CtpTrader.h"
#include "TradingSystem/Event.h"
#include <iostream>
#include <cstring>

namespace ts {
CtpTrader::CtpTrader() {
  api_ = CThostFtdcTraderApi::CreateFtdcTraderApi();
  api_->RegisterSpi(this);
}
CtpTrader::~CtpTrader() {
  if (api_) { api_->Release(); api_ = nullptr; }
}
bool CtpTrader::connect(const std::string& front) {
  api_->RegisterFront(const_cast<char*>(front.c_str()));
  api_->Init();
  std::cout << "[CTP TD] Connecting to " << front << std::endl;
  return true;
}
bool CtpTrader::login(const std::string& broker_id, const std::string& user_id, const std::string& password) {
  broker_ = broker_id; user_ = user_id; pass_ = password;
  CThostFtdcReqUserLoginField req{};
  strncpy(req.BrokerID, broker_.c_str(), sizeof(req.BrokerID));
  strncpy(req.UserID, user_.c_str(), sizeof(req.UserID));
  strncpy(req.Password, pass_.c_str(), sizeof(req.Password));
  int ret = api_->ReqUserLogin(&req, ++req_id_);
  std::cout << "[CTP TD] Send login request ret=" << ret << std::endl;
  return ret == 0;
}
std::string CtpTrader::place_order(const OrderRequest& r) {
  CThostFtdcInputOrderField ord{};
  strncpy(ord.BrokerID, broker_.c_str(), sizeof(ord.BrokerID));
  strncpy(ord.InvestorID, user_.c_str(), sizeof(ord.InvestorID));
  strncpy(ord.InstrumentID, r.instrument.c_str(), sizeof(ord.InstrumentID));
  ord.Direction = (r.direction == Direction::Buy) ? THOST_FTDC_D_Buy : THOST_FTDC_D_Sell;
  ord.CombOffsetFlag[0] = (r.offset == Offset::Open) ? THOST_FTDC_OF_Open : THOST_FTDC_OF_Close;
  ord.OrderPriceType = (r.type == OrderType::Limit) ? THOST_FTDC_OPT_LimitPrice : THOST_FTDC_OPT_AnyPrice;
  ord.LimitPrice = r.price;
  ord.VolumeTotalOriginal = r.volume;
  ord.TimeCondition = THOST_FTDC_TC_GFD;
  ord.VolumeCondition = THOST_FTDC_VC_AV;
  ord.ContingentCondition = THOST_FTDC_CC_Immediately;
  ord.MinVolume = 1;
  ord.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;
  int ret = api_->ReqOrderInsert(&ord, ++req_id_);
  std::cout << "[CTP TD] ReqOrderInsert ret=" << ret << std::endl;
  return std::to_string(req_id_);
}
bool CtpTrader::cancel_order(const std::string& order_id) {
  // Minimal example; you'd use CThostFtdcInputOrderActionField with appropriate fields.
  std::cout << "[CTP TD] Cancel not implemented in scaffold. Id=" << order_id << std::endl;
  return false;
}
void CtpTrader::set_order_status_handler(OrderStatusHandler h) { handler_ = std::move(h); }

void CtpTrader::OnFrontConnected() { std::cout << "[CTP TD] Front connected\n"; }
void CtpTrader::OnFrontDisconnected(int nReason) { std::cout << "[CTP TD] Front disconnected reason=" << nReason << "\n"; }
void CtpTrader::OnRspUserLogin(CThostFtdcRspUserLoginField* pRspUserLogin,
                      CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast) {
  std::cout << "[CTP TD] Login response: ErrorID=" << (pRspInfo ? pRspInfo->ErrorID : 0)
            << " Msg=" << (pRspInfo ? pRspInfo->ErrorMsg : "") << std::endl;
}
void CtpTrader::OnRtnOrder(CThostFtdcOrderField* pOrder) {
  if (!handler_ || !pOrder) return;
  OrderStatusEvent ev;
  ev.order_id = pOrder->OrderSysID;
  ev.status = pOrder->OrderStatus;
  ev.message = "RtnOrder";
  handler_(ev);
}
void CtpTrader::OnRtnTrade(CThostFtdcTradeField* pTrade) {
  if (!handler_ || !pTrade) return;
  OrderStatusEvent ev;
  ev.order_id = pTrade->OrderSysID;
  ev.status = "Traded";
  ev.message = "RtnTrade";
  handler_(ev);
}
} // namespace ts
#endif