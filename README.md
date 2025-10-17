# FuturesForge期货量化交易系统（C++ / CTP）

本项目是一个使用 C++ 开发、可接入 CTP 接口的期货量化交易系统骨架。默认提供 Stub 行情/交易以便在没有 CTP SDK 的情况下也能本地演示事件驱动流程；启用 `USE_CTP=ON` 后，可编译真实 CTP 封装（需要本机安装 CTP SDK）。

## 目录结构
```
FuturesForge/
├─ CMakeLists.txt
├─ include/FuturesForge/
│  ├─ Event.h
│  ├─ IMarketData.h
│  ├─ ITrader.h
│  ├─ Strategy.h
│  └─ Engine.h
├─ src/
│  ├─ core/Engine.cpp
│  ├─ stub/
│  │  ├─ StubMarketData.h
│  │  ├─ StubMarketData.cpp
│  │  ├─ StubTrader.h
│  │  └─ StubTrader.cpp
│  ├─ ctp/  (仅 USE_CTP=ON 编译)
│  │  ├─ CtpMarketData.h
│  │  ├─ CtpMarketData.cpp
│  │  ├─ CtpTrader.h
│  │  └─ CtpTrader.cpp
│  └─ main.cpp
```

## 构建与运行（Stub 演示）
1. 生成构建文件：
   - 在项目根目录执行：
     ```
     cmake -S . -B build
     ```
2. 编译：
   ```
   cmake --build build --config Release -j 4
   ```
3. 运行：
   - 可执行文件位于 `build/bin/`（部分生成器会使用 `build/bin/Release/`）。运行：
     ```
     build\bin\trade_app.exe
     ```
   - 程序会打印 Stub 行情与策略触发的下单/成交日志，约运行 20 秒后退出。

## 启用 CTP 接入
1. 准备 CTP SDK：
   - 将 CTP SDK 的 `include/` 与 `lib/` 放在某目录（例如 `e:\SDKs\CTP`）。
   - 常见库名：
     - 模拟盘：`thostmduserapi_se.lib`, `thosttraderapi_se.lib`
     - 实盘：`thostmduserapi.lib`, `thosttraderapi.lib`
2. 配置 CMake 选项：
   - 设置环境变量 `CTP_SDK_DIR` 指向 SDK 根目录，或在 CMake 时传入：
     ```
     cmake -S . -B build -DUSE_CTP=ON -DCTP_SDK_DIR=e:\SDKs\CTP
     ```
3. 编译：
   ```
   cmake --build build --config Release -j 4
   ```
4. 运行：
   - 修改 `main.cpp` 中 `AppConfig` 为真实前置、资金账号、密码、合约等。
   - 运行同上。

> 说明：CTP 接入使用 `ThostMDUserApi.h` / `ThostTraderApi.h`。如你的 SDK 使用不同大小写或路径，确保包含路径正确。

## 开发说明
- 架构分层：
  - `IMarketData`/`ITrader` 抽象接口，Stub 与 CTP 封装均实现这些接口。
  - `Engine` 负责连接、登录、订阅，并将行情事件分发给 `Strategy`；策略可调用交易接口下单。
  - `Strategy` 示例实现为 `PrintStrategy`，输出行情并在价格超阈值时示意下单。
- 扩展建议：
  - 策略框架：新增 `on_order_status`、`on_bar` 等回调，更丰富的事件类型。
  - 风控模块：开仓限额、止损止盈、断线重连、交易时段控制。
  - 持仓与账户：实时查询与内存账本一致性校验。
  - 日志与持久化：滚动日志、关键事件落盘。

## 回测模式（逐 Tick 撮合）
- 构建：默认已包含 `BacktestMarketData.cpp` 与 `BacktestTrader.cpp`，无需额外选项。
- 运行：编辑 `config.ini` 设置回测与数据路径：
  - `mode=backtest`
  - `backtest_file=data/ticks.csv`
  - `backtest_meta=data/meta.json`
  - `backtest_rules=data/config.json`
  - `instruments=` 留空表示订阅全部合约。
- 行情 CSV：支持常见逐 Tick 格式（包含 `bid/ask/bid_vol/ask_vol/last` 与时间戳、合约字段）。
- 规则与元数据：
  - `meta.json`（逐合约）：`tick_size`、`contract_multiplier`、`slippage_tick`。
  - `config.json`（全局）：`slippage_tick` 与 `partial_fill`。
- 部分成交语义：
  - 当 `partial_fill=false` 且订单类型不是 `IOC` 时，仅在当前 Tick 可用量足以完全成交时才撮合；否则跳过该 Tick。
  - `FOK` 在下单时校验能否全成，不满足则直接拒绝；`IOC` 允许部分成交，剩余立即取消。
- 订单状态事件：`OrderStatusEvent` 现新增结构化字段：`instrument`、`filled_qty`、`fill_price`、`remaining_qty`，同时保留原 `msg` 文本，便于下游直接读取成交与剩余量信息。
- 示例运行：
  - 构建：`cmake --build build --config Release -j 4`
  - 运行：`build\\bin\\trade_app.exe`（或生成器对应的输出目录），日志会展示回放的撮合结果。

## 命令行与配置
- 指定配置文件：使用 `-c` 或 `--config`，例如：
  - `build\\bin\\trade_app.exe -c E:\\09Code\\Test\\TradeSystem\\build\\config.ini`
- 关键配置项：
  - `enable_csv_logs=true|false`：启用 CSV 报表输出。
  - `csv_dir=<目录>`：CSV 输出目录；相对路径会在启动时解析为绝对路径并打印提示，建议使用绝对路径以避免工作目录变化导致的混淆。
  - `run_seconds=<秒>`：运行时长（Stub/回测）。
- CSV 输出说明：
  - 生成 `trade_log.csv`、`trade_summary.csv`、`positions.csv`、`positions_detail.csv`、`pnl.csv`。
  - `pnl.csv` 表头：`instrument,realized_pnl,unrealized_pnl,long_open_qty,long_avg_cost,short_open_qty,short_avg_cost`。
  - 若 `pnl.csv` 被其他程序占用导致无法写入，将自动回退生成 `pnl_YYYYMMDD_HHMMSS.csv` 并在控制台提示。

## 注意事项
- 生产前请完善风控、异常处理与日志，真实环境下务必使用仿真盘充分测试。
- 不要在代码中硬编码账户与密码；建议使用环境变量或配置文件（加密存储）。
- 连接前置、交易权限与合约交易参数由各期货公司/交易所规则决定，请遵循合规要求。