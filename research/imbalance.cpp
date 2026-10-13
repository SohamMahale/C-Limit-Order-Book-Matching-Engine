// Order-book-imbalance research harness.
//   ./lob_research                     synthetic flow (planted effect, validates the pipeline)
//   ./lob_research --csv events.csv    replay real/historical flow
// CSV rows:  L,id,B|S,price_ticks,qty   M,id,B|S,0,qty   C,id,0,0,0
// Look-ahead control: the signal at event t is computed from the book BEFORE event t is applied;
// the target is mid(t+h) - mid(t); the threshold is fitted on the first half only and
// evaluated untouched on the second half. Costs are charged on every trade taken.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>
#include "lob/book.hpp"
using namespace lob;

struct Ev { char t; Id id; Side s; Px p; Qty q; };
struct Obs { double I1, I5; int64_t mid2; };

static void apply(Book& b, const Ev& e) {
  if (e.t == 'C') b.cancel(e.id);
  else b.submit({e.id, e.s, e.t == 'M' ? Type::Market : Type::Limit, e.p, e.q});
}

struct Stats { size_t trades = 0; double hit = 0, net = 0, corr = 0; };

static double corr(const std::vector<double>& a, const std::vector<double>& b) {
  double ma = 0, mb = 0, n = (double)a.size();
  for (size_t i = 0; i < a.size(); ++i) { ma += a[i]; mb += b[i]; }
  ma /= n; mb /= n;
  double sab = 0, saa = 0, sbb = 0;
  for (size_t i = 0; i < a.size(); ++i) { sab += (a[i] - ma) * (b[i] - mb); saa += (a[i] - ma) * (a[i] - ma); sbb += (b[i] - mb) * (b[i] - mb); }
  return (saa > 0 && sbb > 0) ? sab / std::sqrt(saa * sbb) : 0;
}

// signal s[i], forward return r[i] (ticks) over [lo,hi). Go long if s>thr, short if s<-thr.
static Stats evaluate(const std::vector<double>& s, const std::vector<double>& r, size_t lo, size_t hi, double thr, double cost) {
  Stats st; size_t wins = 0; double pnl = 0;
  for (size_t i = lo; i < hi; ++i) {
    if (std::fabs(s[i]) <= thr) continue;
    double g = (s[i] > 0 ? 1 : -1) * r[i];
    ++st.trades; pnl += g - cost; if (g > 0) ++wins;
  }
  st.hit = st.trades ? (double)wins / st.trades : 0;
  st.net = st.trades ? pnl / st.trades : 0;
  st.corr = corr(std::vector<double>(s.begin() + lo, s.begin() + hi), std::vector<double>(r.begin() + lo, r.begin() + hi));
  return st;
}

int main(int argc, char** argv) {
  std::string csv; size_t n = 400000; int h = 20; double cost = 0.5; unsigned seed = 1;
  for (int i = 1; i < argc; ++i) {
    if (!strcmp(argv[i], "--csv") && i + 1 < argc) csv = argv[++i];
    else if (!strcmp(argv[i], "--n") && i + 1 < argc) n = std::stoull(argv[++i]);
    else if (!strcmp(argv[i], "--h") && i + 1 < argc) h = std::stoi(argv[++i]);
    else if (!strcmp(argv[i], "--cost") && i + 1 < argc) cost = std::stod(argv[++i]);
    else if (!strcmp(argv[i], "--seed") && i + 1 < argc) seed = std::stoul(argv[++i]);
  }
  Book b; b.record_tape = false;
  std::vector<Obs> obs;
  auto observe = [&] { if (b.two_sided()) obs.push_back({b.imbalance(1), b.imbalance(5), b.mid2()}); };

  if (!csv.empty()) {
    std::ifstream f(csv); std::string line;
    while (std::getline(f, line)) {
      std::stringstream ss(line); std::string t, id, s, p, q;
      std::getline(ss, t, ','); std::getline(ss, id, ','); std::getline(ss, s, ','); std::getline(ss, p, ','); std::getline(ss, q, ',');
      if (t.empty()) continue;
      Ev e{t[0], std::stoull(id), (s == "S") ? Side::Sell : Side::Buy, std::stoll(p), std::stoll(q)};
      observe(); apply(b, e);
    }
  } else {
    std::mt19937_64 g(seed); std::uniform_real_distribution<double> U(0, 1);
    std::vector<Id> live; Id next = 1;
    auto bid0 = [&] { return b.best_bid() ? *b.best_bid() : 1000; };
    auto ask0 = [&] { return b.best_ask() ? *b.best_ask() : 1001; };
    for (size_t i = 0; i < n; ++i) {
      double I = b.two_sided() ? b.imbalance(1) : 0, u = U(g);
      Ev e;
      if (u < 0.20 && !live.empty()) {                       // cancel (may be stale -> no-op)
        size_t k = g() % live.size(); e = {'C', live[k], Side::Buy, 0, 0};
        live[k] = live.back(); live.pop_back();
      } else if (u < 0.50) {                                 // market order; PLANTED effect: direction leans with I
        Side s = U(g) < 0.5 + 0.3 * I ? Side::Buy : Side::Sell;
        e = {'M', next++, s, 0, (Qty)(1 + g() % 5)};
      } else {                                               // limit order near the touch
        Side s = U(g) < 0.5 ? Side::Buy : Side::Sell; int off = g() % 5;
        Px p = s == Side::Buy ? bid0() + 1 - off : ask0() - 1 + off;
        e = {'L', next++, s, p, (Qty)(1 + g() % 10)}; live.push_back(e.id);
      }
      observe(); apply(b, e);
    }
  }

  size_t m = obs.size() > (size_t)h ? obs.size() - h : 0;
  if (m < 1000) { std::printf("too few observations (%zu)\n", obs.size()); return 1; }
  std::vector<double> ret(m);
  for (size_t i = 0; i < m; ++i) ret[i] = (obs[i + h].mid2 - obs[i].mid2) / 2.0;
  size_t split = m / 2;
  std::printf("observations %zu | horizon %d events | cost %.2f ticks/trade | train [0,%zu) test [%zu,%zu)\n", m, h, cost, split, split, m);

  for (int depth : {1, 5}) {
    std::vector<double> sig(m);
    for (size_t i = 0; i < m; ++i) sig[i] = depth == 1 ? obs[i].I1 : obs[i].I5;
    double bestThr = 0, bestNet = -1e18;
    for (double thr = 0; thr < 0.95; thr += 0.1) {         // fit on TRAIN only
      Stats s = evaluate(sig, ret, 0, split, thr, cost);
      double total = s.net * s.trades;
      if (s.trades >= 100 && total > bestNet) { bestNet = total; bestThr = thr; }
    }
    Stats tr = evaluate(sig, ret, 0, split, bestThr, cost), te = evaluate(sig, ret, split, m, bestThr, cost);
    std::printf("\nI(depth=%d) thr=%.1f (chosen on train)\n", depth, bestThr);
    std::printf("  train: corr(I,ret)=%+.3f  trades=%zu  hit=%.3f  net=%+.3f ticks/trade\n", tr.corr, tr.trades, tr.hit, tr.net);
    std::printf("  test : corr(I,ret)=%+.3f  trades=%zu  hit=%.3f  net=%+.3f ticks/trade  (out-of-sample)\n", te.corr, te.trades, te.hit, te.net);
  }
  std::printf("\nNote: overlapping horizons make samples autocorrelated; treat significance cautiously.\n");
}
