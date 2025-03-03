#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include <ctime>

class HttpClient;

using HttpResponseCallback = std::function<size_t(void*, size_t, size_t, void*)>;

enum class HttpMethod {
    GET,
    POST,
    PUT,
    DELETE
};

struct HttpResponse {
    long status_code;
    std::string body;
    std::map<std::string, std::string> headers;
    std::string redirect_url;
    HttpClient* client_ptr;
};

struct OAuth2Token {
    std::string access_token;
    std::string token_type;
    int expires_in;
    std::string refresh_token;
    std::string scope;
    std::time_t expiry_time;
};

struct OAuth2Params {
    std::string service_account_json;
    std::string scope;
    std::string token_endpoint;
};

class HttpClientImpl;

class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;
    
    HttpClient(HttpClient&&) noexcept;
    HttpClient& operator=(HttpClient&&) noexcept;
    
    void setHeaders(const std::map<std::string, std::string>& headers);
    
    void addHeader(const std::string& name, const std::string& value);
    
    void setCookies(const std::string& cookies);
    
    std::string getCookies() const;
    
    HttpResponse request(const std::string& url, 
                         HttpMethod method = HttpMethod::GET,
                         const std::string& body = "");
    
    HttpResponse requestWithManualRedirects(const std::string& url, 
                                           HttpMethod method = HttpMethod::GET,
                                           const std::string& body = "");
    
    OAuth2Token getOAuth2TokenWithJWT(const OAuth2Params& params);
    
    OAuth2Token getOAuth2TokenWithJWT(
        const std::string& serviceAccountJson,
        const std::string& scope,
        const std::string& tokenEndpoint);
    
    static std::string base64UrlEncode(const std::string& input);
    
    static std::string signWithRSA(const std::string& data, const std::string& privateKey);

private:
    std::unique_ptr<HttpClientImpl> pImpl;
}; 