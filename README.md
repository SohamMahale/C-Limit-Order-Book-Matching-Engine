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

## Benchmarks (1M ops/workload, -O2, sandbox VM; your numbers will differ)
| workload | ops/s | p50 | p99 |
|---|---|---|---|
| passive | 4.1M | 170 ns | 1.9 us |
| mixed (25% cancel, 15% market) | 3.4M | 200 ns | 1.2 us |
| aggressive | 4.0M | 206 ns | 0.9 us |
| cancel-heavy | 3.2M | 233 ns | 1.1 us |

Trade-offs: std::map is simple and handles any price range but is pointer-chasing (cache misses explain the p99 tail
and the multi-ms max, which is mostly OS jitter and allocation). std::list gives stable iterators for cheap cancel but
allocates per order. Next experiments: flat array of levels indexed by tick (O(1) best-price with a bitmap), pooled
allocator, intrusive lists, heap with lazy deletion. Compare them under these same workloads.

## Research
`research/imbalance.cpp`: I = (Qb-Qa)/(Qb+Qa), depth 1 and 5. Signal uses the book before each event; target is
mid change over h events; threshold fit on first half, scored on second half; per-trade cost deducted.
Synthetic flow has a PLANTED imbalance effect, so it validates the pipeline, not the hypothesis. Real conclusions need
`--csv` historical order flow (format in file header). Not modelled: queue position, fill uncertainty, market impact.
