#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <thread>
#include <chrono>
#include <curl/curl.h>
#include "../include/http_server.h"

namespace {

size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

class CURLWrapper {
public:
    CURLWrapper() {
        curl_ = curl_easy_init();
        if (!curl_) throw std::runtime_error("Failed to initialize CURL");
    }
    
    ~CURLWrapper() {
        if (curl_) curl_easy_cleanup(curl_);
    }
    
    std::pair<long, std::string> perform_request(const std::string& url, 
                                               const std::string& method = "GET",
                                               const std::string& data = "") {
        std::string response_data;
        
        curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &response_data);
        
        if (method != "GET") {
            curl_easy_setopt(curl_, CURLOPT_CUSTOMREQUEST, method.c_str());
            curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, data.c_str());
            curl_easy_setopt(curl_, CURLOPT_POSTFIELDSIZE, data.length());
        }
        
        CURLcode res = curl_easy_perform(curl_);
        if (res != CURLE_OK) {
            throw std::runtime_error(curl_easy_strerror(res));
        }
        
        long http_code = 0;
        curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &http_code);
        
        return {http_code, response_data};
    }
    
private:
    CURL* curl_;
};

}  // namespace

namespace cppwebforge {

class HTTPServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        curl_global_init(CURL_GLOBAL_ALL);
    }
    
    void TearDown() override {
        curl_global_cleanup();
    }
    
    void start_server(HTTPServer* server) {
        server_thread_ = std::thread([server]() {
            try {
                server->start();
            } catch (const std::exception& e) {
                server_error_ = e.what();
            }
        });
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        ASSERT_TRUE(server_error_.empty()) << "Server failed to start: " << server_error_;
    }
    
    void stop_server(HTTPServer* server) {
        if (server) {
            server->stop();
        }
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
    }
    
    const std::string base_url = "http://127.0.0.1:8080";
    
private:
    std::thread server_thread_;
    static std::string server_error_;
};

std::string HTTPServerTest::server_error_;

TEST_F(HTTPServerTest, BasicServerStartStop) {
    HTTPServer::Builder builder;
    auto server = builder.port(8080)
                        .address("127.0.0.1")
                        .build();
    
    start_server(server.get());
    stop_server(server.get());
}

TEST_F(HTTPServerTest, BasicGetRequest) {
    HTTPServer::Builder builder;
    auto server = builder.port(8081)
                        .address("127.0.0.1")
                        .get("/test", [](const Request& req, Response& res) {
                            res.set_content("Hello, World!", "text/plain");
                        })
                        .build();
    
    start_server(server.get());
    
    CURLWrapper curl;
    auto [status, response] = curl.perform_request("http://127.0.0.1:8081/test");
    
    EXPECT_EQ(status, 200);
    EXPECT_EQ(response, "Hello, World!");
    
    stop_server(server.get());
}

TEST_F(HTTPServerTest, PostRequest) {
    HTTPServer::Builder builder;
    auto server = builder.port(8082)
                        .address("127.0.0.1")
                        .post("/echo", [](const Request& req, Response& res) {
                            res.set_content(req.body(), "text/plain");
                        })
                        .build();
    
    start_server(server.get());
    
    CURLWrapper curl;
    auto [status, response] = curl.perform_request(
        "http://127.0.0.1:8082/echo", 
        "POST",
        "Hello from POST!"
    );
    
    EXPECT_EQ(status, 200);
    EXPECT_EQ(response, "Hello from POST!");
    
    stop_server(server.get());
}

TEST_F(HTTPServerTest, RequestHeaders) {
    HTTPServer::Builder builder;
    auto server = builder.port(8083)
                        .address("127.0.0.1")
                        .get("/headers", [](const Request& req, Response& res) {
                            EXPECT_TRUE(req.has_header("Test-Header"));
                            EXPECT_EQ(req.get_header_value("Test-Header"), "test-value");
                            res.set_header("Response-Header", "response-value");
                            res.set_content("OK", "text/plain");
                        })
                        .build();
    
    start_server(server.get());
    
    CURL* curl = curl_easy_init();
    ASSERT_TRUE(curl != nullptr);
    
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Test-Header: test-value");
    
    std::string response_data;
    curl_easy_setopt(curl, CURLOPT_URL, "http://127.0.0.1:8083/headers");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
    
    CURLcode res = curl_easy_perform(curl);
    EXPECT_EQ(res, CURLE_OK);
    
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    EXPECT_EQ(http_code, 200);
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    stop_server(server.get());
}

TEST_F(HTTPServerTest, NotFoundHandler) {
    HTTPServer::Builder builder;
    auto server = builder.port(8084)
                        .address("127.0.0.1")
                        .build();
    
    start_server(server.get());
    
    CURLWrapper curl;
    auto [status, response] = curl.perform_request("http://127.0.0.1:8084/nonexistent");
    
    EXPECT_EQ(status, 404);
    
    stop_server(server.get());
}

TEST_F(HTTPServerTest, MultipleEndpoints) {
    HTTPServer::Builder builder;
    auto server = builder.port(8085)
                        .address("127.0.0.1")
                        .get("/endpoint1", [](const Request& req, Response& res) {
                            res.set_content("Endpoint 1", "text/plain");
                        })
                        .post("/endpoint2", [](const Request& req, Response& res) {
                            res.set_content("Endpoint 2", "text/plain");
                        })
                        .put("/endpoint3", [](const Request& req, Response& res) {
                            res.set_content("Endpoint 3", "text/plain");
                        })
                        .del("/endpoint4", [](const Request& req, Response& res) {
                            res.set_content("Endpoint 4", "text/plain");
                        })
                        .build();
    
    start_server(server.get());
    
    CURLWrapper curl;
    
    auto [status1, response1] = curl.perform_request("http://127.0.0.1:8085/endpoint1");
    EXPECT_EQ(status1, 200);
    EXPECT_EQ(response1, "Endpoint 1");
    
    auto [status2, response2] = curl.perform_request("http://127.0.0.1:8085/endpoint2", "POST");
    EXPECT_EQ(status2, 200);
    EXPECT_EQ(response2, "Endpoint 2");
    
    auto [status3, response3] = curl.perform_request("http://127.0.0.1:8085/endpoint3", "PUT");
    EXPECT_EQ(status3, 200);
    EXPECT_EQ(response3, "Endpoint 3");
    
    auto [status4, response4] = curl.perform_request("http://127.0.0.1:8085/endpoint4", "DELETE");
    EXPECT_EQ(status4, 200);
    EXPECT_EQ(response4, "Endpoint 4");
    
    stop_server(server.get());
}

} // namespace cppwebforge
