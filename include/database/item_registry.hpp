#pragma once
#include "items.hpp"
namespace item_registry { const ::item* find(u_short id) noexcept; bool exists(u_short id) noexcept; std::size_t size() noexcept; }
