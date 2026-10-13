#include <cstdio>
#include "lob/book.hpp"
using namespace lob;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)
static Order L(Id id, Side s, Px p, Qty q) { return {id, s, Type::Limit, p, q}; }
static Order M(Id id, Side s, Qty q) { return {id, s, Type::Market, 0, q}; }

int main() {
  { // price priority: best price fills first, trade at resting price
    Book b; b.submit(L(1, Side::Sell, 102, 5)); b.submit(L(2, Side::Sell, 101, 5));
    b.submit(L(3, Side::Buy, 105, 3));
    CHECK(b.tape.size() == 1 && b.tape[0].sell_id == 2 && b.tape[0].price == 101 && b.tape[0].qty == 3);
  }
  { // time priority within a level
    Book b; b.submit(L(1, Side::Sell, 100, 5)); b.submit(L(2, Side::Sell, 100, 5));
    b.submit(L(3, Side::Buy, 100, 7));
    CHECK(b.tape.size() == 2 && b.tape[0].sell_id == 1 && b.tape[0].qty == 5 && b.tape[1].sell_id == 2 && b.tape[1].qty == 2);
    CHECK(b.depth(Side::Sell, 1) == 3);
  }
  { // partial fill: aggressor remainder rests
    Book b; b.submit(L(1, Side::Sell, 100, 4)); b.submit(L(2, Side::Buy, 100, 10));
    CHECK(b.tape.size() == 1 && b.tape[0].qty == 4);
    CHECK(*b.best_bid() == 100 && b.depth(Side::Buy, 1) == 6 && !b.best_ask());
  }
  { // limit that does not cross rests
    Book b; b.submit(L(1, Side::Sell, 101, 1)); b.submit(L(2, Side::Buy, 100, 1));
    CHECK(b.tape.empty() && *b.spread() == 1);
  }
  { // market orders sweep levels, remainder discarded
    Book b; b.submit(L(1, Side::Sell, 100, 2)); b.submit(L(2, Side::Sell, 101, 2));
    b.submit(M(3, Side::Buy, 10));
    CHECK(b.tape.size() == 2 && b.tape[1].price == 101 && !b.best_ask() && !b.best_bid() && b.live_orders() == 0);
  }
  { // cancellation
    Book b; b.submit(L(1, Side::Buy, 99, 5)); b.submit(L(2, Side::Buy, 99, 5));
    CHECK(b.cancel(1)); CHECK(!b.cancel(1)); CHECK(!b.cancel(77));
    CHECK(b.depth(Side::Buy, 1) == 5);
    b.submit(L(3, Side::Sell, 99, 5));
    CHECK(b.tape.size() == 1 && b.tape[0].buy_id == 2);
    CHECK(b.cancel(2) == false);  // already fully filled
  }
  { // cancelling last order at a level removes the level
    Book b; b.submit(L(1, Side::Buy, 99, 5)); b.submit(L(2, Side::Buy, 98, 5));
    b.cancel(1); CHECK(*b.best_bid() == 98 && b.bid_levels() == 1);
  }
  { // invalid orders
    Book b;
    CHECK(b.submit(L(0, Side::Buy, 100, 1)) == Status::BadId);
    CHECK(b.submit(L(1, Side::Buy, 100, 0)) == Status::BadQty);
    CHECK(b.submit(L(1, Side::Buy, 100, -5)) == Status::BadQty);
    CHECK(b.submit(L(1, Side::Buy, 0, 5)) == Status::BadPrice);
    CHECK(b.submit(L(1, Side::Buy, -3, 5)) == Status::BadPrice);
    CHECK(b.submit(L(1, Side::Buy, 100, 5)) == Status::Accepted);
    CHECK(b.submit(L(1, Side::Sell, 200, 5)) == Status::DuplicateId);
    CHECK(b.live_orders() == 1);
  }
  { // imbalance
    Book b; b.submit(L(1, Side::Buy, 99, 30)); b.submit(L(2, Side::Sell, 101, 10));
    CHECK(b.imbalance(1) == 0.5);
    b.submit(L(3, Side::Sell, 102, 20));
    CHECK(b.imbalance(2) == 0.0);
  }
  { // invariant: book never crossed under random flow
    Book b; b.record_tape = false; uint64_t x = 88172645463325252ull;
    auto r = [&] { x ^= x << 13; x ^= x >> 7; x ^= x << 17; return x; };
    for (Id i = 1; i < 20000; ++i) {
      if (r() % 4 == 0) b.cancel(1 + r() % i);
      else b.submit(L(i, r() % 2 ? Side::Buy : Side::Sell, 90 + r() % 21, 1 + r() % 5));
      if (b.two_sided()) CHECK(*b.best_bid() < *b.best_ask());
    }
  }
  std::printf(fails ? "%d FAILED\n" : "all tests passed\n", fails);
  return fails ? 1 : 0;
}
