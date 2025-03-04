#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <thread>
#include <chrono>
#include <curl/curl.h>
#include "../include/http_server.h"

namespace cppwebforge {

size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

class HttpServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        server_ = std::make_unique<HttpServer>(18092);
        
        server_->addRoute("/test_post", []() {
            return "POST test response";
        });

        
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

    std::string makeRequest(const std::string& url, 
                          const std::string& method = "GET",
                          const std::string& data = "") {
        CURL* curl = curl_easy_init();
        std::string response_string;
        
        if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);
            
            if (method == "POST") {
                curl_easy_setopt(curl, CURLOPT_POST, 1L);
                if (!data.empty()) {
                    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
                }
            } else if (method == "PUT") {
                curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
                if (!data.empty()) {
                    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
                }
            } else if (method == "DELETE") {
                curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
                if (!data.empty()) {
                    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
                }
            }
            
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

TEST_F(HttpServerTest, GetEndpoint) {
    std::string response = makeRequest("http://localhost:18080/test_get");
    EXPECT_EQ(response, "GET test response");
}

TEST_F(HttpServerTest, GetEndpointWithParams) {
    std::string response = makeRequest("http://localhost:18080/test_get_params", "GET", "test data");
    EXPECT_EQ(response, "GET with body: test data");
}

TEST_F(HttpServerTest, PostEndpoint) {
    std::string response = makeRequest("http://localhost:18080/test_post", "POST");
    EXPECT_EQ(response, "POST test response");
}

TEST_F(HttpServerTest, PostEndpointWithParams) {
    std::string response = makeRequest("http://localhost:18080/test_post_params", "POST", "test data");
    EXPECT_EQ(response, "POST received: test data");
}

TEST_F(HttpServerTest, PutEndpoint) {
    std::string response = makeRequest("http://localhost:18080/test_put", "PUT");
    EXPECT_EQ(response, "PUT test response");
}

TEST_F(HttpServerTest, PutEndpointWithParams) {
    std::string response = makeRequest("http://localhost:18080/test_put_params", "PUT", "test data");
    EXPECT_EQ(response, "PUT received: test data");
}

TEST_F(HttpServerTest, DeleteEndpoint) {
    std::string response = makeRequest("http://localhost:18080/test_delete", "DELETE");
    EXPECT_EQ(response, "DELETE test response");
}

TEST_F(HttpServerTest, DeleteEndpointWithParams) {
    std::string response = makeRequest("http://localhost:18080/test_delete_params", "DELETE", "test data");
    EXPECT_EQ(response, "DELETE received: test data");
}

TEST_F(HttpServerTest, NonExistentEndpoint) {
    CURL* curl = curl_easy_init();
    long http_code = 0;
    
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:18080/nonexistent");
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
        CURLcode res = curl_easy_perform(curl);
        
        if (res == CURLE_OK) {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        }
        
        curl_easy_cleanup(curl);
    }
    
    EXPECT_EQ(http_code, 404);
}

} // namespace cppwebforge
