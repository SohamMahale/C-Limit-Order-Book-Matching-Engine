CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Iinclude
BIN := build
all: $(BIN)/lob_cli $(BIN)/lob_tests $(BIN)/lob_bench $(BIN)/lob_research
$(BIN)/lob_cli: src/main.cpp include/lob/*.hpp ; @mkdir -p $(BIN) && $(CXX) $(CXXFLAGS) $< -o $@
$(BIN)/lob_tests: tests/test_book.cpp include/lob/*.hpp ; @mkdir -p $(BIN) && $(CXX) $(CXXFLAGS) $< -o $@
$(BIN)/lob_bench: bench/bench.cpp include/lob/*.hpp ; @mkdir -p $(BIN) && $(CXX) $(CXXFLAGS) $< -o $@
$(BIN)/lob_research: research/imbalance.cpp include/lob/*.hpp ; @mkdir -p $(BIN) && $(CXX) $(CXXFLAGS) $< -o $@
test: $(BIN)/lob_tests ; ./$(BIN)/lob_tests
bench: $(BIN)/lob_bench ; ./$(BIN)/lob_bench
research: $(BIN)/lob_research ; ./$(BIN)/lob_research
clean: ; rm -rf $(BIN)
.PHONY: all test bench research clean
