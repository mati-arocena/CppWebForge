#include "http_server.h"
#include <crow.h>

namespace cppwebforge {

class HttpServer::Impl {
public:
    Impl(int port) : port_(port) {
        CROW_ROUTE(app_, "/")([](){
            return "Hello World!";
        });
        
        CROW_ROUTE(app_, "/json")([](){
            crow::json::wvalue response;
            response["message"] = "Hello from JSON endpoint!";
            response["status"] = "success";
            return response;
        });
    }
    
    void start() {
        app_.port(port_).run();
    }
    
    void stop() {
        app_.stop();
    }
    
private:
    crow::SimpleApp app_;
    int port_;
};

HttpServer::HttpServer(int port) 
    : impl_(std::make_unique<Impl>(port))
    , running_(false) {
}

HttpServer::~HttpServer() = default;

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

bool HttpServer::is_running() const {
    return running_;
}

} // namespace cppwebforge
