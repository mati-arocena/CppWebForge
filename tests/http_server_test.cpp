#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <curl/curl.h>
#include "../src/http_server.h"

namespace cppwebforge {

size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

class HttpServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        server_ = std::make_unique<HttpServer>(18080);
        server_thread_ = std::thread([this]() {
            server_->start();
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    void TearDown() override {
        server_->stop();
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
    }

    std::string makeRequest(const std::string& url) {
        CURL* curl = curl_easy_init();
        std::string response_string;
        
        if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);
            
            CURLcode res = curl_easy_perform(curl);
            curl_easy_cleanup(curl);
            
            if (res != CURLE_OK) {
                return "Request failed";
            }
        }
        
        return response_string;
    }

    std::unique_ptr<HttpServer> server_;
    std::thread server_thread_;
};

TEST_F(HttpServerTest, RootEndpoint) {
    std::string response = makeRequest("http://localhost:18080/");
    EXPECT_EQ(response, "Hello World!");
}

TEST_F(HttpServerTest, JsonEndpoint) {
    std::string response = makeRequest("http://localhost:18080/json");
    EXPECT_TRUE(response.find("Hello from JSON endpoint!") != std::string::npos);
    EXPECT_TRUE(response.find("success") != std::string::npos);
}

} // namespace cppwebforge
