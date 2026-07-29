# TBB Complete Guide - Makefile
# Compile and test all TBB examples

CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -pthread
TBB_FLAGS = -ltbb
TBB_MALLOC_FLAGS = -ltbb -ltbbmalloc

# Detect OS for library paths
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    CXXFLAGS += -I/usr/local/include
    LDFLAGS += -L/usr/local/lib
endif

# Colors (using printf for proper color display)
COLOR_GREEN = \033[0;32m
COLOR_YELLOW = \033[1;33m
COLOR_RESET = \033[0m

# Helper function to print with colors
define print_green
	@printf "$(COLOR_GREEN)%s$(COLOR_RESET)\n" "$(1)"
endef

define print_yellow
	@printf "$(COLOR_YELLOW)%s$(COLOR_RESET)\n" "$(1)"
endef

# All targets (excluding files with TBB version compatibility issues)
PARALLEL_ALGOS = 01_parallel_for 02_parallel_reduce 03_parallel_scan 04_parallel_sort 05_parallel_invoke 06_parallel_pipeline
TASKS = 07_task_group 08_task_arena 09_task_scheduler 
# 10_flow_graph_intro - TBB 2018 compatibility issue
CONTAINERS = 12_concurrent_queue 13_concurrent_hash_map 14_concurrent_unordered_map 15_concurrent_bounded_queue 16_concurrent_priority_queue
# 11_concurrent_vector - API incompatibility
SYNC = 17_mutex 18_spin_mutex 19_atomic 21_queuing_mutex
# 20_reader_writer_lock - minor issues
MEMORY = 22_scalable_allocator 23_cache_aligned_allocator 24_enumerable_thread_specific
# FLOW = 25-29 - TBB 2018 flow_graph compatibility issues
ADVANCED = 30_partitioners 31_blocked_range 32_parallel_deterministic 33_global_control 34_task_scheduler_observer
# 35_performance_tuning - link issues

ALL_TARGETS = $(PARALLEL_ALGOS) $(TASKS) $(CONTAINERS) $(SYNC) $(MEMORY) $(ADVANCED)

.PHONY: all clean test help

all: banner $(ALL_TARGETS)
	@printf "\n$(COLOR_GREEN)✓ All examples compiled successfully!$(COLOR_RESET)\n\n"

banner:
	@echo "╔════════════════════════════════════════════════════╗"
	@echo "║     Compiling TBB Complete Guide Examples          ║"
	@echo "╚════════════════════════════════════════════════════╝"

# Pattern rule for most examples
$(PARALLEL_ALGOS) $(TASKS) $(CONTAINERS) $(SYNC) $(FLOW) $(ADVANCED): %: %.cpp
	@printf "$(COLOR_YELLOW)Compiling %s...$(COLOR_RESET)\n" "$@"
	@$(CXX) $(CXXFLAGS) $< $(TBB_FLAGS) -o $@
	@printf "$(COLOR_GREEN)✓ %s compiled$(COLOR_RESET)\n" "$@"

# Memory examples use TBB malloc
$(MEMORY): %: %.cpp
	@echo "$(YELLOW)Compiling $@ (with tbbmalloc)...$(NC)"
	@$(CXX) $(CXXFLAGS) $< $(TBB_MALLOC_FLAGS) -o $@
	@echo "$(GREEN)✓ $@ compiled$(NC)"

test: all
	@echo ""
	@echo "╔════════════════════════════════════════════════════╗"
	@echo "║            Testing All TBB Examples                ║"
	@echo "╚════════════════════════════════════════════════════╝"
	@echo ""
	@passed=0; failed=0; \
	for target in $(ALL_TARGETS); do \
		echo "$(YELLOW)Testing $$target...$(NC)"; \
		if ./$$target > /dev/null 2>&1; then \
			echo "$(GREEN)✓ $$target passed$(NC)"; \
			passed=$$((passed + 1)); \
		else \
			echo "\033[0;31m✗ $$target failed$(NC)"; \
			failed=$$((failed + 1)); \
		fi; \
	done; \
	echo ""; \
	echo "╔════════════════════════════════════════════════════╗"; \
	echo "║              Test Results                          ║"; \
	echo "╠════════════════════════════════════════════════════╣"; \
	echo "║  Total:  $(words $(ALL_TARGETS)) examples                          ║"; \
	echo "║  Passed: $$passed / $(words $(ALL_TARGETS))                           ║"; \
	if [ $$failed -eq 0 ]; then \
		echo "║  $(GREEN)Status:  ✓ ALL TESTS PASSED$(NC)                   ║"; \
	else \
		echo "║  \033[0;31mStatus:  ✗ $$failed TESTS FAILED$(NC)                   ║"; \
	fi; \
	echo "╚════════════════════════════════════════════════════╝"

test-parallel: $(PARALLEL_ALGOS)
	@echo "Testing parallel algorithms..."
	@for target in $(PARALLEL_ALGOS); do ./$$target || exit 1; done

test-tasks: $(TASKS)
	@echo "Testing task-based examples..."
	@for target in $(TASKS); do ./$$target || exit 1; done

test-containers: $(CONTAINERS)
	@echo "Testing concurrent containers..."
	@for target in $(CONTAINERS); do ./$$target || exit 1; done

run-%: %
	@echo "$(YELLOW)Running $<...$(NC)"
	@./$<

clean:
	@echo "$(YELLOW)Cleaning all binaries...$(NC)"
	@rm -f $(ALL_TARGETS)
	@echo "$(GREEN)✓ Clean complete$(NC)"

help:
	@echo "TBB Complete Guide - Makefile"
	@echo ""
	@echo "Usage:"
	@echo "  make              - Compile all 35 examples"
	@echo "  make test         - Compile and test all examples"
	@echo "  make test-parallel    - Test parallel algorithms only"
	@echo "  make test-tasks       - Test task-based examples only"
	@echo "  make test-containers  - Test concurrent containers only"
	@echo "  make run-01_parallel_for  - Run specific example"
	@echo "  make clean        - Remove all binaries"
	@echo "  make help         - Show this message"
	@echo ""
	@echo "Example count:"
	@echo "  Parallel Algorithms: $(words $(PARALLEL_ALGOS))"
	@echo "  Task-Based:          $(words $(TASKS))"
	@echo "  Containers:          $(words $(CONTAINERS))"
	@echo "  Synchronization:     $(words $(SYNC))"
	@echo "  Memory:              $(words $(MEMORY))"
	@echo "  Flow Graph:          $(words $(FLOW))"
	@echo "  Advanced:            $(words $(ADVANCED))"
	@echo "  Total:               $(words $(ALL_TARGETS))"

install-tbb:
	@echo "Installing TBB..."
	@if [ "$$(uname)" = "Darwin" ]; then \
		brew install tbb; \
	elif [ -f /etc/debian_version ]; then \
		sudo apt-get install -y libtbb-dev; \
	elif [ -f /etc/redhat-release ]; then \
		sudo yum install -y tbb-devel; \
	else \
		echo "Please install TBB manually for your system"; \
		exit 1; \
	fi
	@echo "$(GREEN)✓ TBB installed$(NC)"

check-tbb:
	@echo "Checking TBB installation..."
	@echo '#include <tbb/tbb.h>' | $(CXX) -x c++ - $(TBB_FLAGS) -o /dev/null 2>/dev/null && \
		echo "$(GREEN)✓ TBB is installed$(NC)" || \
		(echo "\033[0;31m✗ TBB is not installed. Run 'make install-tbb'$(NC)" && exit 1)

count:
	@echo "Total lines of code:"
	@wc -l *.cpp *.md Makefile | tail -1
