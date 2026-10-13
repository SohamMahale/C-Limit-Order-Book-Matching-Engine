#include <iostream>
#include <sstream>
#include "lob/book.hpp"
using namespace lob;

static const char* msg(Status s) {
  switch (s) {
    case Status::Accepted: return "accepted";
    case Status::BadId: return "rejected: id must be > 0";
    case Status::DuplicateId: return "rejected: duplicate id";
    case Status::BadQty: return "rejected: qty must be > 0";
    case Status::BadPrice: return "rejected: price must be > 0 ticks";
  }
  return "?";
}

int main() {
  Book b;
  std::cout << "Commands (prices in integer ticks):\n"
               "  buy|sell <id> <price> <qty>   limit order\n"
               "  mbuy|msell <id> <qty>         market order\n"
               "  cancel <id> | book [levels] | tape [n] | quit\n";
  std::string line;
  while (std::cout << "> " && std::getline(std::cin, line)) {
    std::istringstream in(line);
    std::string c; in >> c;
    size_t before = b.tape.size();
    if (c == "buy" || c == "sell") {
      Id id; Px p; Qty q;
      if (!(in >> id >> p >> q)) { std::cout << "usage: " << c << " <id> <price> <qty>\n"; continue; }
      std::cout << msg(b.submit({id, c == "buy" ? Side::Buy : Side::Sell, Type::Limit, p, q})) << "\n";
    } else if (c == "mbuy" || c == "msell") {
      Id id; Qty q;
      if (!(in >> id >> q)) { std::cout << "usage: " << c << " <id> <qty>\n"; continue; }
      std::cout << msg(b.submit({id, c == "mbuy" ? Side::Buy : Side::Sell, Type::Market, 0, q})) << "\n";
    } else if (c == "cancel") {
      Id id; in >> id;
      std::cout << (b.cancel(id) ? "cancelled" : "unknown order") << "\n";
    } else if (c == "book") {
      int n = 5; in >> n; b.print(std::cout, n);
    } else if (c == "tape") {
      size_t n = 10; in >> n;
      size_t s = b.tape.size() > n ? b.tape.size() - n : 0;
      for (size_t i = s; i < b.tape.size(); ++i) {
        auto& t = b.tape[i];
        std::cout << t.ts << "ns  px " << t.price << "  qty " << t.qty << "  buyer " << t.buy_id << "  seller " << t.sell_id << "\n";
      }
    } else if (c == "quit" || c == "exit") break;
    else if (!c.empty()) std::cout << "unknown command\n";
    for (size_t i = before; i < b.tape.size(); ++i)
      std::cout << "  TRADE " << b.tape[i].qty << " @ " << b.tape[i].price << " (buy " << b.tape[i].buy_id << ", sell " << b.tape[i].sell_id << ")\n";
  }
}
