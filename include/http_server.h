#pragma once

#include <string>
#include <functional>
#include <memory>

namespace cppwebforge {

class Request;
class Response;

class HTTPServer {
public:
    using Handler = std::function<void(const Request&, Response&)>;

    class Builder {
    public:
        Builder();
        ~Builder();
        
        Builder& get(const std::string& path, const Handler& handler);
        Builder& post(const std::string& path, const Handler& handler);
        Builder& put(const std::string& path, const Handler& handler);
        Builder& del(const std::string& path, const Handler& handler);
        
        Builder& port(int port);
        Builder& address(const std::string& addr);
        
        std::unique_ptr<HTTPServer> build();
        
    private:
        class BuilderImpl;
        std::unique_ptr<BuilderImpl> impl_;
        friend class HTTPServer;
    };

    ~HTTPServer();
    void start();
    void stop();
    
protected:
    class HTTPServerImpl;
    HTTPServer();
    std::unique_ptr<HTTPServerImpl> impl_;
    
private:
    friend class Builder;
    friend class Request;
    friend class Response;
};

class Request {
public:
    Request();
    ~Request();
    
    const std::string& body() const;
    const std::string& path() const;
    const std::string& method() const;
    std::string get_header_value(const std::string& key) const;
    bool has_header(const std::string& key) const;
    
private:
    class RequestImpl;
    std::unique_ptr<RequestImpl> impl_;
    friend class HTTPServer::HTTPServerImpl;
};

class Response {
public:
    Response();
    ~Response();
    
    void set_content(const std::string& content, const std::string& content_type);
    void set_header(const std::string& key, const std::string& value);
    void set_status(int status);
    
private:
    class ResponseImpl;
    std::unique_ptr<ResponseImpl> impl_;
    friend class HTTPServer::HTTPServerImpl;
};

}
