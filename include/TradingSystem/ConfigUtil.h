#pragma once
#include <string>
#include "Engine.h"

namespace ts {
// 从简易INI配置文件加载AppConfig；ok为可选输出指示是否加载成功
AppConfig load_app_config(const std::string& path, bool* ok = nullptr);
}