#pragma once
#include <chrono>
#include <cstdint>
namespace lob {
using Id = uint64_t;
using Px = int64_t;   // integer price ticks, never floating point
using Qty = int64_t;
enum class Side : uint8_t { Buy, Sell };
enum class Type : uint8_t { Limit, Market };
enum class Status { Accepted, BadId, DuplicateId, BadQty, BadPrice };
struct Order { Id id; Side side; Type type; Px price; Qty qty; };
struct Trade { uint64_t ts; Px price; Qty qty; Id buy_id, sell_id; };
inline uint64_t now_ns() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::steady_clock::now().time_since_epoch()).count();
}
}  // namespace lob
