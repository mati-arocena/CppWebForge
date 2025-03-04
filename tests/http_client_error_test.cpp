#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <thread>
#include <chrono>
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <httplib.h>
#include <nlohmann/json.hpp>
#include "../include/http_client.h"

namespace cppwebforge {

class ErrorMockHttpServer {
public:
    ErrorMockHttpServer(int port) : port_(port), running_(false) {
        svr_.Get("/not_found", [](const httplib::Request&, httplib::Response& res) {
            res.status = 404;
            res.set_content("Resource not found", "text/plain");
        });

        svr_.Get("/server_error", [](const httplib::Request&, httplib::Response& res) {
            res.status = 500;
            res.set_content("Internal server error", "text/plain");
        });

        svr_.Get("/timeout", [](const httplib::Request&, httplib::Response& res) {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            res.set_content("Delayed response", "text/plain");
        });

        svr_.Get("/bad_json", [](const httplib::Request&, httplib::Response& res) {
            res.set_content("{\"incomplete_json\":true,", "application/json");
        });

        svr_.Get("/large_response", [](const httplib::Request&, httplib::Response& res) {
            std::string large_data(1024 * 1024, 'X'); // 1MB of data
            res.set_content(large_data, "text/plain");
        });

        svr_.Get("/redirect1", [this](const httplib::Request&, httplib::Response& res) {
            res.status = 302;
            res.set_header("Location", "http://localhost:" + std::to_string(port_) + "/redirect2");
        });

        svr_.Get("/redirect2", [this](const httplib::Request&, httplib::Response& res) {
            res.status = 302;
            res.set_header("Location", "http://localhost:" + std::to_string(port_) + "/redirect3");
        });

        svr_.Get("/redirect3", [](const httplib::Request&, httplib::Response& res) {
            res.set_content("Final destination", "text/plain");
        });

        svr_.Get("/circular_redirect", [this](const httplib::Request&, httplib::Response& res) {
            res.status = 302;
            res.set_header("Location", "http://localhost:" + std::to_string(port_) + "/circular_redirect");
        });
    }

    void start() {
        if (!running_) {
            running_ = true;
            server_thread_ = std::thread([this]() {
                svr_.listen("localhost", port_);
            });
            // Give the server a moment to start
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    void stop() {
        if (running_) {
            svr_.stop();
            if (server_thread_.joinable()) {
                server_thread_.join();
            }
            running_ = false;
        }
    }

    ~ErrorMockHttpServer() {
        stop();
    }

private:
    httplib::Server svr_;
    int port_;
    bool running_;
    std::thread server_thread_;
};

class HttpClientErrorTest : public ::testing::Test {
protected:
    void SetUp() override {
        server_ = std::make_unique<ErrorMockHttpServer>(18082);
        server_->start();
        client_ = std::make_unique<HttpClient>();
    }

    void TearDown() override {
        server_->stop();
    }

    std::unique_ptr<ErrorMockHttpServer> server_;
    std::unique_ptr<HttpClient> client_;
};

TEST_F(HttpClientErrorTest, NotFoundError) {
    HttpResponse response = client_->request("http://localhost:18082/not_found");
    EXPECT_EQ(response.status_code, 404);
    EXPECT_EQ(response.body, "Resource not found");
}

TEST_F(HttpClientErrorTest, ServerError) {
    HttpResponse response = client_->request("http://localhost:18082/server_error");
    EXPECT_EQ(response.status_code, 500);
    EXPECT_EQ(response.body, "Internal server error");
}

TEST_F(HttpClientErrorTest, NonExistentHost) {
    try {
        HttpResponse response = client_->request("http://non.existent.host.local/");
        FAIL() << "Expected an exception for non-existent host";
    } catch (const std::exception& e) {
        SUCCEED();
    }
}

TEST_F(HttpClientErrorTest, InvalidURL) {
    try {
        HttpResponse response = client_->request("not_a_valid_url");
        FAIL() << "Expected an exception for invalid URL";
    } catch (const std::exception& e) {
        SUCCEED();
    }
}

TEST_F(HttpClientErrorTest, LargeResponse) {
    HttpResponse response = client_->request("http://localhost:18082/large_response");
    EXPECT_EQ(response.status_code, 200);
    EXPECT_EQ(response.body.size(), 1024 * 1024);
}

TEST_F(HttpClientErrorTest, BadJsonResponse) {
    HttpResponse response = client_->request("http://localhost:18082/bad_json");
    EXPECT_EQ(response.status_code, 200);

    bool parsing_failed = false;
    try {
        auto parsed = nlohmann::json::parse(response.body);
        auto test_value = parsed["incomplete_json"];
        if (test_value.is_null()) {
            parsing_failed = true;
        }
    } catch (const std::exception& e) {
        parsing_failed = true;
    }
    
    EXPECT_TRUE(parsing_failed) << "Expected JSON parsing to fail or return invalid data";
}

TEST_F(HttpClientErrorTest, MultipleRedirects) {
    HttpResponse response = client_->requestWithManualRedirects("http://localhost:18082/redirect1");
    EXPECT_EQ(response.status_code, 200);
    EXPECT_EQ(response.body, "Final destination");
}

TEST_F(HttpClientErrorTest, CircularRedirects) {
    try {
        HttpResponse response = client_->requestWithManualRedirects("http://localhost:18082/circular_redirect");
        EXPECT_NE(response.status_code, 200);
    } catch (const std::exception& e) {
        SUCCEED();
    }
}

} // namespace cppwebforge
