cmake_minimum_required(VERSION 3.15)
project(TradeSystem LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(USE_CTP "Build with CTP SDK support" OFF)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/src)

set(SRC_CORE
    src/core/Engine.cpp
    src/core/ConfigUtil.cpp
    src/core/BarAggregator.cpp
    src/core/RiskManager.cpp
    src/core/TraderProxy.cpp
)

set(SRC_STUB
  src/stub/StubMarketData.cpp
  src/stub/StubTrader.cpp
)

set(SRC_MAIN
  src/main.cpp
)

set(SRC_BACKTEST
  src/backtest/BacktestMarketData.cpp
  src/backtest/BacktestTrader.cpp
)

add_executable(trade_app
  ${SRC_MAIN}
  ${SRC_CORE}
  ${SRC_STUB}
  ${SRC_BACKTEST}
)

target_compile_features(trade_app PRIVATE cxx_std_17)

if(USE_CTP)
  message(STATUS "Building with CTP SDK")
  # Expect environment variable CTP_SDK_DIR or CMake cache var provided; typical structure: include, lib
  if(DEFINED ENV{CTP_SDK_DIR})
    set(CTP_SDK_DIR $ENV{CTP_SDK_DIR})
  endif()
  if(NOT CTP_SDK_DIR)
    message(FATAL_ERROR "CTP_SDK_DIR is not set. Set environment variable CTP_SDK_DIR or pass -DCTP_SDK_DIR=... to CMake.")
  endif()

  include_directories(${CTP_SDK_DIR}/include)
  set(CTP_LIB_DIR ${CTP_SDK_DIR}/lib)

  # Library names vary (simulated: *_se; production: no suffix). Try both.
  find_library(CTP_MD_LIB NAMES thostmduserapi_se thostmduserapi PATHS ${CTP_LIB_DIR} NO_DEFAULT_PATH)
  find_library(CTP_TRADER_LIB NAMES thosttraderapi_se thosttraderapi PATHS ${CTP_LIB_DIR} NO_DEFAULT_PATH)

  if(NOT CTP_MD_LIB OR NOT CTP_TRADER_LIB)
    message(FATAL_ERROR "Cannot find CTP libs in ${CTP_LIB_DIR}. Expected thostmduserapi[_se].lib and thosttraderapi[_se].lib")
  endif()

  target_sources(trade_app PRIVATE
    src/ctp/CtpMarketData.cpp
    src/ctp/CtpTrader.cpp
  )
  target_compile_definitions(trade_app PRIVATE USE_CTP=1)
  target_link_libraries(trade_app PRIVATE ${CTP_MD_LIB} ${CTP_TRADER_LIB})
endif()

# Threads (for stub run loop)
find_package(Threads REQUIRED)
target_link_libraries(trade_app PRIVATE Threads::Threads)

# Windows: ensure Unicode
if(WIN32)
  add_definitions(-DUNICODE -D_UNICODE)
endif()