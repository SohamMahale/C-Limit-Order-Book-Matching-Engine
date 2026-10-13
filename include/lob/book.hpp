#pragma once
#include <algorithm>
#include <functional>
#include <iterator>
#include <list>
#include <map>
#include <optional>
#include <ostream>
#include <unordered_map>
#include <vector>
#include "types.hpp"

namespace lob {

// Price-time priority book. Ordered map of price -> FIFO level (std::list),
// plus hash index id -> list iterator so cancel is O(log L) (L = #levels).
class Book {
 public:
  struct Level { std::list<Order> q; Qty total = 0; };

  bool record_tape = true;
  std::vector<Trade> tape;
  uint64_t trade_count = 0;
  Qty traded_volume = 0;

  Status submit(Order o) {
    if (o.id == 0) return Status::BadId;
    if (o.qty <= 0) return Status::BadQty;
    if (o.type == Type::Limit && o.price <= 0) return Status::BadPrice;
    if (idx_.count(o.id)) return Status::DuplicateId;
    if (o.side == Side::Buy) match(o, asks_); else match(o, bids_);
    if (o.qty > 0 && o.type == Type::Limit) rest(o);  // market remainder is dropped (IOC)
    return Status::Accepted;
  }

  bool cancel(Id id) {
    auto it = idx_.find(id);
    if (it == idx_.end()) return false;
    Loc l = it->second;
    idx_.erase(it);
    if (l.side == Side::Buy) erase_from(bids_, l); else erase_from(asks_, l);
    return true;
  }

  std::optional<Px> best_bid() const { return bids_.empty() ? std::nullopt : std::optional<Px>(bids_.begin()->first); }
  std::optional<Px> best_ask() const { return asks_.empty() ? std::nullopt : std::optional<Px>(asks_.begin()->first); }
  std::optional<Px> spread() const { return (bids_.empty() || asks_.empty()) ? std::nullopt : std::optional<Px>(*best_ask() - *best_bid()); }
  // 2*mid keeps the midpoint in integer ticks.
  Px mid2() const { return *best_bid() + *best_ask(); }
  bool two_sided() const { return !bids_.empty() && !asks_.empty(); }
  size_t live_orders() const { return idx_.size(); }
  size_t bid_levels() const { return bids_.size(); }
  size_t ask_levels() const { return asks_.size(); }

  Qty depth(Side s, int levels) const { return s == Side::Buy ? depth_of(bids_, levels) : depth_of(asks_, levels); }

  // I = (Qb - Qa) / (Qb + Qa) over the top `levels` price levels.
  double imbalance(int levels = 1) const {
    double b = (double)depth(Side::Buy, levels), a = (double)depth(Side::Sell, levels);
    return (a + b) == 0 ? 0.0 : (b - a) / (b + a);
  }

  void print(std::ostream& os, int n = 5) const {
    std::vector<std::pair<Px, Qty>> a;
    int k = 0;
    for (auto& [p, l] : asks_) { if (k++ >= n) break; a.push_back({p, l.total}); }
    os << "      ASKS\n";
    for (auto it = a.rbegin(); it != a.rend(); ++it) os << "  " << it->first << "  x " << it->second << "\n";
    os << "  ---- bid ";
    if (best_bid()) os << *best_bid(); else os << "-";
    os << " | ask ";
    if (best_ask()) os << *best_ask(); else os << "-";
    os << " | spread ";
    if (spread()) os << *spread(); else os << "-";
    os << " | I1 " << imbalance(1) << " ----\n";
    k = 0;
    for (auto& [p, l] : bids_) { if (k++ >= n) break; os << "  " << p << "  x " << l.total << "\n"; }
    os << "      BIDS\n";
  }

 private:
  struct Loc { Side side; Px price; std::list<Order>::iterator it; };
  std::map<Px, Level, std::greater<Px>> bids_;
  std::map<Px, Level> asks_;
  std::unordered_map<Id, Loc> idx_;

  template <class M> static Qty depth_of(const M& m, int levels) {
    Qty s = 0; int k = 0;
    for (auto& [p, l] : m) { if (k++ >= levels) break; s += l.total; }
    return s;
  }
  template <class M> static void erase_from(M& m, const Loc& l) {
    auto lv = m.find(l.price);
    lv->second.total -= l.it->qty;
    lv->second.q.erase(l.it);
    if (lv->second.q.empty()) m.erase(lv);
  }
  void rest(const Order& o) {
    Level& lv = (o.side == Side::Buy) ? bids_[o.price] : asks_[o.price];
    lv.q.push_back(o);
    lv.total += o.qty;
    idx_[o.id] = Loc{o.side, o.price, std::prev(lv.q.end())};
  }
  template <class M> void match(Order& o, M& opp) {
    while (o.qty > 0 && !opp.empty()) {
      auto lvl = opp.begin();
      Px p = lvl->first;
      if (o.type == Type::Limit && (o.side == Side::Buy ? p > o.price : p < o.price)) break;
      Level& L = lvl->second;
      while (o.qty > 0 && !L.q.empty()) {
        Order& r = L.q.front();
        Qty f = std::min(o.qty, r.qty);
        o.qty -= f; r.qty -= f; L.total -= f;
        ++trade_count; traded_volume += f;
        if (record_tape) {
          bool buy = o.side == Side::Buy;
          tape.push_back({now_ns(), p, f, buy ? o.id : r.id, buy ? r.id : o.id});  // trades at resting price
        }
        if (r.qty == 0) { idx_.erase(r.id); L.q.pop_front(); }
      }
      if (L.q.empty()) opp.erase(lvl);
    }
  }
};

}  // namespace lob
