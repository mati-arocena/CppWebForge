#pragma once

#include <chrono>
#include <string>
#include <sstream>
#include "logger.h"

#ifdef PERFORMANCE_TESTING
namespace cppwebforge {
/**
 * This class allows for easy performance measurement of code blocks by creating
 * a scoped object that measures the time between its construction and destruction.
 * 
 * Usage:
 *   {
 *     SCOPED_PERF("Function X - Section Y");
 *     // Code to measure
 *   }
 */
class ScopedPerformance {
public:
    explicit ScopedPerformance(const std::string& label) 
        : label_(label), start_(std::chrono::high_resolution_clock::now()) {
    }

    ~ScopedPerformance() {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start_).count();
        
        std::stringstream strStream;
        strStream << "[PERF] " << label_ << " : ";
        
        if (duration < 1000) {
            strStream << duration << " μs";
            FORGE_INFO(strStream.str());
        } else if (duration < 1000000) {
            strStream << (duration / 1000.0) << " ms";
            FORGE_INFO(strStream.str());
        } else {
            strStream << (duration / 1000000.0) << " s";
            FORGE_INFO(strStream.str());
        }
    }

    ScopedPerformance(const ScopedPerformance&) = delete;
    ScopedPerformance& operator=(const ScopedPerformance&) = delete;
    ScopedPerformance(ScopedPerformance&&) = delete;
    ScopedPerformance& operator=(ScopedPerformance&&) = delete;

private:
    std::string label_;
    std::chrono::time_point<std::chrono::high_resolution_clock> start_;
};

} // namespace cppwebforge

#define SCOPED_PERF(label) cppwebforge::ScopedPerformance scopedPerf_##__LINE__(label)

#else
#define SCOPED_PERF(label)
#endif // PERFORMANCE_TESTING 