#pragma once

#include <memory>
#include <string>

namespace cppwebforge {

class HttpServer {
public:
    HttpServer(int port = 8080);
    ~HttpServer();
    
    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;
    
    HttpServer(HttpServer&&) noexcept;
    HttpServer& operator=(HttpServer&&) noexcept;
    
    void start();
    void stop();
    bool is_running() const;
    
private:
    class HttpServerImpl;
    std::unique_ptr<HttpServerImpl> impl_;
    bool running_;
};

} // namespace cppwebforge
