#pragma once
#include <vector>
#include <future>
#include <atomic>

// Stage 6: bounded-concurrency helper shared by PortScannerDialog,
// DirectoryBusterDialog, and HydraLiteDialog - mirrors the
// Executors.newFixedThreadPool(MAX_CONCURRENCY) pattern those Kotlin files
// used, via chunked std::async batches rather than a persistent thread
// pool class (simpler, same bounded-concurrency effect, since each dialog
// runs exactly one scan at a time).
//
// `cancelled` is checked between chunks (not inside an in-flight worker
// call) - same semantics the original Kotlin had: clicking Stop prevents
// new work from starting but doesn't forcibly abort a socket call already
// in progress.
template <typename Item, typename WorkerFn>
void RunConcurrent(const std::vector<Item>& items, size_t maxConcurrency,
                    const std::atomic<bool>& cancelled, WorkerFn worker) {
    for (size_t i = 0; i < items.size() && !cancelled.load(); i += maxConcurrency) {
        size_t end = (i + maxConcurrency < items.size()) ? i + maxConcurrency : items.size();
        std::vector<std::future<void>> futures;
        for (size_t j = i; j < end; ++j) {
            futures.push_back(std::async(std::launch::async, worker, items[j]));
        }
        for (auto& f : futures) f.get(); // also what bounds concurrency: next chunk waits for this one
    }
}
