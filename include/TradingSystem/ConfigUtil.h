#pragma once
#include <string>
#include "Engine.h"

namespace ts {

AppConfig load_app_config(const std::string& path, bool* ok = nullptr);
}