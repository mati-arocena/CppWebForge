#include <gtest/gtest.h>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include "../include/logger.h"

namespace cppwebforge {

class LoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        testLogFile = "test_rotation.log";
        std::filesystem::remove(testLogFile);
    }

    void TearDown() override {
        std::filesystem::remove(testLogFile);
    }

    std::string testLogFile;

    size_t countFileLines(const std::string& filename) {
        std::ifstream file(filename);
        return std::count(std::istreambuf_iterator<char>(file),
                         std::istreambuf_iterator<char>(), '\n');
    }

    std::vector<std::string> readAllLines(const std::string& filename) {
        std::vector<std::string> lines;
        std::ifstream file(filename);
        std::string line;
        while (std::getline(file, line)) {
            lines.push_back(line);
        }
        return lines;
    }
};

TEST_F(LoggerTest, LogRotationTest) {
    Logger& logger = Logger::getInstance();
    logger.setLogFile(testLogFile);

    const int LINES_TO_WRITE = 6000;
    for (int i = 0; i < LINES_TO_WRITE; ++i) {
        FORGE_INFO("Test log message {}", i);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    ASSERT_TRUE(std::filesystem::exists(testLogFile));

    size_t lineCount = countFileLines(testLogFile);
    
    EXPECT_LE(lineCount, 5000);
    EXPECT_GE(lineCount, 2500);

    auto lines = readAllLines(testLogFile);
    ASSERT_FALSE(lines.empty());

    std::string lastMessage = lines.back();
    EXPECT_TRUE(lastMessage.find("Test log message " + std::to_string(LINES_TO_WRITE - 1)) != std::string::npos);
}

#ifdef DEBUG_MODE
TEST_F(LoggerTest, DebugLogRotationTest) {
    Logger& logger = Logger::getInstance();
    logger.setLogFile(testLogFile);

    const int LINES_TO_WRITE = 6000;
    for (int i = 0; i < LINES_TO_WRITE; ++i) {
        FORGE_DEBUG("Test debug message {}", i);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    ASSERT_TRUE(std::filesystem::exists(testLogFile));

    size_t lineCount = countFileLines(testLogFile);
    
    EXPECT_LE(lineCount, 5000);
    EXPECT_GE(lineCount, 2500);

    auto lines = readAllLines(testLogFile);
    ASSERT_FALSE(lines.empty());

    std::string lastMessage = lines.back();
    EXPECT_TRUE(lastMessage.find("Test debug message " + std::to_string(LINES_TO_WRITE - 1)) != std::string::npos);
}
#endif

} // namespace cppwebforge
