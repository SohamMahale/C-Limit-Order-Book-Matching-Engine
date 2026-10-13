#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>
#include "lob/book.hpp"
using namespace lob;

struct Op { int kind; Order o; Id cid; };  // 0 limit, 1 market, 2 cancel
static uint64_t x = 88172645463325252ull;
static uint64_t rnd() { x ^= x << 13; x ^= x >> 7; x ^= x << 17; return x; }

// buy px = 10000-d, sell px = 10001+d. d<0 crosses the spread.
static std::vector<Op> gen(size_t n, int dlo, int dhi, int pcancel, int pmarket) {
  std::vector<Op> v; v.reserve(n);
  for (Id id = 1; v.size() < n; ++id) {
    int r = rnd() % 100;
    if (r < pcancel && id > 10) { v.push_back({2, {}, 1 + rnd() % (id - 1)}); continue; }
    Side s = rnd() % 2 ? Side::Buy : Side::Sell;
    if (r < pcancel + pmarket) { v.push_back({1, {id, s, Type::Market, 0, (Qty)(1 + rnd() % 5)}, 0}); continue; }
    int d = dlo + (int)(rnd() % (dhi - dlo + 1));
    v.push_back({0, {id, s, Type::Limit, s == Side::Buy ? 10000 - d : 10001 + d, (Qty)(1 + rnd() % 10)}, 0});
  }
  return v;
}
static void apply(Book& b, const Op& op) { if (op.kind == 2) b.cancel(op.cid); else b.submit(op.o); }
static void run(const char* name, const std::vector<Op>& ops) {
  Book b; b.record_tape = false;
  uint64_t t0 = now_ns();
  for (auto& op : ops) apply(b, op);
  double secs = (now_ns() - t0) / 1e9;
  Book b2; b2.record_tape = false;
  std::vector<uint32_t> lat; lat.reserve(ops.size());
  for (auto& op : ops) { uint64_t a = now_ns(); apply(b2, op); lat.push_back((uint32_t)(now_ns() - a)); }
  std::sort(lat.begin(), lat.end());
  auto pct = [&](double p) { return lat[(size_t)(p * (lat.size() - 1))]; };
  std::printf("%-12s %9.0f ops/s  p50 %5u ns  p99 %6u ns  p99.9 %7u ns  max %8u ns  trades %llu  resting %zu\n",
              name, ops.size() / secs, pct(.5), pct(.99), pct(.999), lat.back(),
              (unsigned long long)b.trade_count, b.live_orders());
}
int main(int argc, char** argv) {
  size_t n = argc > 1 ? std::stoull(argv[1]) : 1000000;
  std::printf("orders per workload: %zu (latency includes ~20-30ns timer overhead)\n", n);
  run("passive", gen(n, 0, 50, 0, 0));
  run("mixed", gen(n, -1, 10, 25, 15));
  run("aggressive", gen(n, -5, 5, 0, 10));
  run("cancel-heavy", gen(n, 0, 20, 45, 5));
}
