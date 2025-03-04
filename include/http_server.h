#pragma once

#include <memory>
#include <string>
#include <functional>

namespace cppwebforge {

using RouteHandler = std::function<std::string()>;

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
    
    void addRoute(const std::string& path, const RouteHandler& handler);
private:
    class HttpServerImpl;
    std::unique_ptr<HttpServerImpl> impl_;
    bool running_;
};

} // namespace cppwebforge
