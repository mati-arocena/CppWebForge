#include "http_server.h"
#include "httplib.h"
#include <stdexcept>

namespace cppwebforge {

namespace {
constexpr int DEFAULT_PORT = 8080;
constexpr const char* DEFAULT_ADDRESS = "0.0.0.0";
}

class Request::RequestImpl {
public:
    RequestImpl(const httplib::Request& req) : req_(req) {}
    const httplib::Request& req_;
};

Request::Request() : impl_(std::make_unique<RequestImpl>(httplib::Request())) {}
Request::~Request() = default;

const std::string& Request::body() const { return impl_->req_.body; }
const std::string& Request::path() const { return impl_->req_.path; }
const std::string& Request::method() const { return impl_->req_.method; }
std::string Request::get_header_value(const std::string& key) const { return impl_->req_.get_header_value(key); }
bool Request::has_header(const std::string& key) const { return impl_->req_.has_header(key); }

// Response Implementation
class Response::ResponseImpl {
public:
    ResponseImpl() : res_(nullptr) {}
    ResponseImpl(httplib::Response& res) : res_(&res) {}
    httplib::Response* res_;
};

Response::Response() : impl_(std::make_unique<ResponseImpl>()) {}
Response::~Response() = default;

void Response::set_content(const std::string& content, const std::string& content_type) {
    if (impl_->res_ != nullptr) {
        impl_->res_->set_content(content, content_type);
    }
}

void Response::set_header(const std::string& key, const std::string& value) {
    if (impl_->res_ != nullptr) {
        impl_->res_->set_header(key, value);
    }
}

void Response::set_status(int status) {
    if (impl_->res_ != nullptr) {
        impl_->res_->status = status;
    }
}

class HTTPServer::HTTPServerImpl {
public:
    HTTPServerImpl() 
        : server_(std::make_unique<httplib::Server>())
        , port_(DEFAULT_PORT)
        , address_(DEFAULT_ADDRESS) {}

    std::unique_ptr<httplib::Server> server_;
    int port_;
    std::string address_;

    static void wrap_handler(const Handler& handler, const httplib::Request& req, httplib::Response& res) {
        Request wrapped_req;
        wrapped_req.impl_ = std::make_unique<Request::RequestImpl>(req);
        Response wrapped_res;
        wrapped_res.impl_ = std::make_unique<Response::ResponseImpl>(res);
        handler(wrapped_req, wrapped_res);
    }
};

HTTPServer::HTTPServer() : impl_(std::make_unique<HTTPServerImpl>()) {}
HTTPServer::~HTTPServer()
{
    stop();
}

void HTTPServer::start() {
    if (!impl_->server_->listen(impl_->address_, impl_->port_)) {
        throw std::runtime_error("Failed to start server on " + impl_->address_ + ":" + std::to_string(impl_->port_));
    }
}

void HTTPServer::stop() {
    if (impl_->server_) {
        impl_->server_->stop();
    }
}

class HTTPServer::Builder::BuilderImpl {
public:
    BuilderImpl() : server_(new HTTPServer()) {}
    std::unique_ptr<HTTPServer> server_;
};

HTTPServer::Builder::Builder() : impl_(std::make_unique<BuilderImpl>()) {}
HTTPServer::Builder::~Builder() = default;

HTTPServer::Builder& HTTPServer::Builder::get(const std::string& path, const Handler& handler) {
    impl_->server_->impl_->server_->Get(path, 
        [handler](const httplib::Request& req, httplib::Response& res) {
            HTTPServerImpl::wrap_handler(handler, req, res);
        });
    return *this;
}

HTTPServer::Builder& HTTPServer::Builder::post(const std::string& path, const Handler& handler) {
    impl_->server_->impl_->server_->Post(path,
        [handler](const httplib::Request& req, httplib::Response& res) {
            HTTPServerImpl::wrap_handler(handler, req, res);
        });
    return *this;
}

HTTPServer::Builder& HTTPServer::Builder::put(const std::string& path, const Handler& handler) {
    impl_->server_->impl_->server_->Put(path,
        [handler](const httplib::Request& req, httplib::Response& res) {
            HTTPServerImpl::wrap_handler(handler, req, res);
        });
    return *this;
}

HTTPServer::Builder& HTTPServer::Builder::del(const std::string& path, const Handler& handler) {
    impl_->server_->impl_->server_->Delete(path,
        [handler](const httplib::Request& req, httplib::Response& res) {
            HTTPServerImpl::wrap_handler(handler, req, res);
        });
    return *this;
}

HTTPServer::Builder& HTTPServer::Builder::port(int port) {
    impl_->server_->impl_->port_ = port;
    return *this;
}

HTTPServer::Builder& HTTPServer::Builder::address(const std::string& addr) {
    impl_->server_->impl_->address_ = addr;
    return *this;
}

std::unique_ptr<HTTPServer> HTTPServer::Builder::build() {
    return std::move(impl_->server_);
}

}
