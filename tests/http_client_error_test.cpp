#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <thread>
#include <chrono>
#include <crow.h>
#include <nlohmann/json.hpp>
#include "../include/http_client.h"

namespace cppwebforge {

class ErrorMockHttpServer {
public:
    ErrorMockHttpServer(int port) : port_(port), running_(false) {
        CROW_ROUTE(app_, "/not_found")
        ([]() {
            crow::response resp;
            resp.code = 404;
            resp.body = "Resource not found";
            return resp;
        });

        CROW_ROUTE(app_, "/server_error")
        ([]() {
            crow::response resp;
            resp.code = 500;
            resp.body = "Internal server error";
            return resp;
        });

        CROW_ROUTE(app_, "/timeout")
        ([]() {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            return "Delayed response";
        });

        CROW_ROUTE(app_, "/bad_json")
        ([]() {
            return "{\"incomplete_json\":true,";
        });

        CROW_ROUTE(app_, "/large_response")
        ([]() {
            std::string large_data(1024 * 1024, 'X'); // 1MB of data
            return large_data;
        });

        CROW_ROUTE(app_, "/redirect1")
        ([this]() {
            crow::response resp;
            resp.code = 302;
            resp.set_header("Location", "http://localhost:" + std::to_string(port_) + "/redirect2");
            return resp;
        });

        CROW_ROUTE(app_, "/redirect2")
        ([this]() {
            crow::response resp;
            resp.code = 302;
            resp.set_header("Location", "http://localhost:" + std::to_string(port_) + "/redirect3");
            return resp;
        });

        CROW_ROUTE(app_, "/redirect3")
        ([]() {
            return "Final destination";
        });

        CROW_ROUTE(app_, "/circular_redirect")
        ([this]() {
            crow::response resp;
            resp.code = 302;
            resp.set_header("Location", "http://localhost:" + std::to_string(port_) + "/circular_redirect");
            return resp;
        });
    }

    void start() {
        if (!running_) {
            running_ = true;
            server_thread_ = std::thread([this]() {
                app_.port(port_).run();
            });
            // Give the server a moment to start
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    void stop() {
        if (running_) {
            app_.stop();
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
    crow::SimpleApp app_;
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
