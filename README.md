# lob: C++17 limit order book, matching engine and microstructure lab

Header-only engine (`include/lob/book.hpp`), integer price ticks, no dependencies.

    make test       # unit tests (priority, partials, cancels, invalid orders, invariants)
    make bench      # throughput + latency percentiles on 4 workloads
    make research   # imbalance signal experiment (synthetic flow or --csv file)
    make && ./build/lob_cli   # interactive terminal exchange (CMakeLists.txt also provided)

## Design
- `std::map<price, Level>` per side (best price = begin()), each level a FIFO `std::list` with a cached total.
- `unordered_map<id, iterator>` index: O(1) lookup, O(log L) cancel. Trades print at the resting order's price.
- Market orders are IOC: unfilled remainder is discarded. Rejects: id 0, duplicate live id, qty <= 0, limit price <= 0.

## Benchmarks

1M orders per workload. Windows laptop, AMD Ryzen 9 [exact model], [compiler + version], [flags, e.g. -O2], plugged in, run from `make bench`.
Timing uses std::chrono::steady_clock (QueryPerformanceCounter on Windows, about 100 ns resolution), with about 20-30 ns timer overhead per sample.

| workload                       | ops/s | p50    | p99    | p99.9   |
| ------------------------------ | ----- | ------ | ------ | ------- |
| passive                        | 7.3M  | 100 ns | 200 ns | 22.2 us |
| mixed (25% cancel, 15% market) | 5.2M  | 100 ns | 900 ns | 1.7 us  |
| aggressive                     | 8.8M  | 100 ns | 500 ns | 1.1 us  |
| cancel-heavy                   | 6.4M  | 100 ns | 700 ns | 1.7 us  |

Reading the numbers:
- p50 is identical across workloads because it sits at the timer resolution, so treat it as a floor, not a precise median. p99 and p99.9 are the informative columns.
- Max latency (7 to 26 ms) is OS scheduling jitter on a non-realtime laptop, not engine cost.
- Throughput is measured over the full run, so it is more reliable than per-op latency at this scale.

Trade-offs: std::map handles any price range but is pointer-chasing, and std::list allocates per order. Both contribute to the p99 tail. Next experiments: flat array of levels indexed by tick (O(1) best price with a bitmap), pooled allocator, intrusive lists. Compare them under these same workloads.

## Research
`research/imbalance.cpp`: I = (Qb-Qa)/(Qb+Qa), depth 1 and 5. Signal uses the book before each event; target is
mid change over h events; threshold fit on first half, scored on second half; per-trade cost deducted.
Synthetic flow has a PLANTED imbalance effect, so it validates the pipeline, not the hypothesis. Real conclusions need
`--csv` historical order flow (format in file header). Not modelled: queue position, fill uncertainty, market impact.
