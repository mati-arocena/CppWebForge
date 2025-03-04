#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include "../include/performance.h"

namespace cppwebforge {

TEST(PerformanceTest, ScopedPerformance) {
    SCOPED_PERF("Outer Scope");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    {
        SCOPED_PERF("Inner Scope - Section 1");
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    {
        SCOPED_PERF("Inner Scope - Section 2");
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
}

} // namespace cppwebforge
