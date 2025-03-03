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
    class Impl;
    std::unique_ptr<Impl> impl_;
    bool running_;
};

} // namespace cppwebforge
