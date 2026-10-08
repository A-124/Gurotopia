#include "pch.hpp"
#include <mutex>
#include "event_bus.hpp"
namespace event_bus {
namespace { struct subscription { type kind; listener callback; }; std::vector<subscription> subscriptions; std::mutex mutex; }
void subscribe(type kind, listener callback) { if (!callback) return; std::scoped_lock lock(mutex); subscriptions.push_back({kind, std::move(callback)}); }
void emit(const event& value) { std::vector<listener> callbacks; { std::scoped_lock lock(mutex); for (const auto& s : subscriptions) if (s.kind == value.kind) callbacks.push_back(s.callback); } for (const auto& callback : callbacks) callback(value); }
void clear() { std::scoped_lock lock(mutex); subscriptions.clear(); }
}
