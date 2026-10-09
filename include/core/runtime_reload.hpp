#pragma once
#include <string_view>
namespace runtime_reload { enum class result { ok, unknown_target, failed }; result reload(std::string_view target); std::string_view result_message(result value); }
