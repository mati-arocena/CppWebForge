#include "http_server.h"
#include <crow.h>
#include <string>

namespace cppwebforge {

class HttpServer::HttpServerImpl {
public:
    HttpServerImpl(int port) : port_(port) {
        CROW_ROUTE(app_, "/")([](){
            return "Hello World!";
        });
    }

    ~HttpServerImpl() {
        if (running_) {
            stop();
        }
    }
    
    void start() {
        running_ = true;
        app_.port(port_).run();
    }
    
    void stop() {
        app_.stop();
        running_ = false;
    }

    void addRoute(const std::string& path, const RouteHandler& handler) {
        app_.route_dynamic(path)
        ([handler](const crow::request&) {
            return handler();
        });
    }
    
private:
    crow::SimpleApp app_;
    int port_;
    bool running_ = false;
};

HttpServer::HttpServer(int port) 
    : impl_(std::make_unique<HttpServerImpl>(port))
    , running_(false) {
}

HttpServer::~HttpServer()
{
    if (running_) {
        stop();
    }
}

HttpServer::HttpServer(HttpServer&&) noexcept = default;
HttpServer& HttpServer::operator=(HttpServer&&) noexcept = default;

void HttpServer::start() {
    running_ = true;
    impl_->start();
}

void HttpServer::stop() {
    if (running_) {
        impl_->stop();
        running_ = false;
    }
}

void HttpServer::addRoute(const std::string& path, const RouteHandler& handler) {
    impl_->addRoute(path, handler);
}

} // namespace cppwebforge
