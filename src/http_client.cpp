#include "http_client.h"
#include <iostream>
#include <sstream>
#include <ctime>
#include <curl/curl.h>
#include <crow/json.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/err.h>

namespace {
    constexpr long HTTP_OK = 200;
    constexpr long HTTP_MOVED_PERMANENTLY = 301;
    constexpr long HTTP_FOUND = 302;
    constexpr long HTTP_SEE_OTHER = 303;
    constexpr long HTTP_TEMPORARY_REDIRECT = 307;
    constexpr long HTTP_PERMANENT_REDIRECT = 308;
    
    constexpr size_t LOCATION_PREFIX_LEN = 10;
    constexpr size_t SET_COOKIE_PREFIX_LEN = 12;
    constexpr int TOKEN_EXPIRY_SECONDS = 3600;
}

namespace cppwebforge {

class HttpClientImpl {
public:
    HttpClientImpl() : headerList_(nullptr) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
    }

    ~HttpClientImpl() {
        if (headerList_ != nullptr) {
            curl_slist_free_all(headerList_);
            headerList_ = nullptr;
        }
        
        curl_global_cleanup();
    }

    void setHeaders(const std::map<std::string, std::string>& headers) {
        headers_ = headers;
    }

    void addHeader(const std::string& name, const std::string& value) {
        headers_[name] = value;
    }

    void setCookies(const std::string& cookies) {
        cookies_ = cookies;
    }

    std::string getCookies() const {
        return cookies_;
    }

    static size_t writeCallback(void* rawData, size_t elementSize, size_t elementCount, std::string* outputBuffer) {
        size_t newLength = elementSize * elementCount;
        try {
            outputBuffer->append((char*)rawData, newLength);
            return newLength;
        } catch(std::bad_alloc& e) {
            return 0;
        }
    }

    static size_t headerCallback(char* buffer, size_t size, size_t nitems, void* userdata) {
        size_t totalSize = size * nitems;
        std::string header(buffer, totalSize);
        
        HttpResponse* response = static_cast<HttpResponse*>(userdata);
        
        if (header.find("Location: ") == 0) {
            response->redirect_url = header.substr(LOCATION_PREFIX_LEN);
            if (!response->redirect_url.empty() && 
                response->redirect_url[response->redirect_url.size() - 1] == '\n') {
                response->redirect_url.pop_back();
            }
            if (!response->redirect_url.empty() && 
                response->redirect_url[response->redirect_url.size() - 1] == '\r') {
                response->redirect_url.pop_back();
            }
        }
        
        if (header.find("Set-Cookie: ") == 0) {
            std::string cookie = extractCookieFromHeader(header);
            if (!cookie.empty() && response->client_ptr != nullptr) {
                HttpClient* client = response->client_ptr;
                std::string currentCookies = client->getCookies();
                if (!currentCookies.empty()) {
                    currentCookies += "; ";
                }
                currentCookies += cookie;
                client->setCookies(currentCookies);
            }
        }
        
        size_t colonPos = header.find(':');
        if (colonPos != std::string::npos) {
            std::string name = header.substr(0, colonPos);
            std::string value = header.substr(colonPos + 1);
            
            while (!value.empty() && (value[0] == ' ' || value[0] == '\t')) {
                value.erase(0, 1);
            }
            while (!value.empty() && (value.back() == '\r' || value.back() == '\n')) {
                value.pop_back();
            }
            
            response->headers[name] = value;
        }
        
        return totalSize;
    }

    static std::string extractCookieFromHeader(const std::string& header) {
        if (header.find("Set-Cookie: ") != 0) {
            return "";
        }
        
        std::string cookieStr = header.substr(SET_COOKIE_PREFIX_LEN);
        
        size_t semicolonPos = cookieStr.find(';');
        if (semicolonPos != std::string::npos) {
            cookieStr = cookieStr.substr(0, semicolonPos);
        }
        
        while (!cookieStr.empty() && (cookieStr.back() == '\r' || cookieStr.back() == '\n' || 
                                    cookieStr.back() == ' ' || cookieStr.back() == '\t')) {
            cookieStr.pop_back();
        }
        
        return cookieStr;
    }

    static CURL* initCurl(const std::string& url, std::string& responseBuffer) {
        CURL* curl = curl_easy_init();
        if (curl == nullptr) {
            throw std::runtime_error("Failed to initialize curl");
        }
        
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBuffer);
        
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L); // We'll handle redirects manually
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
        
        return curl;
    }

    static void setMethodOptions(CURL* curl, HttpMethod method, const std::string& body) {
        switch (method) {
            case HttpMethod::GET:
                curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
                break;
                
            case HttpMethod::POST:
                curl_easy_setopt(curl, CURLOPT_POST, 1L);
                if (!body.empty()) {
                    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
                    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body.length());
                }
                break;
                
            case HttpMethod::PUT:
                curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
                if (!body.empty()) {
                    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
                    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body.length());
                }
                break;
                
            case HttpMethod::DELETE:
                curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
                if (!body.empty()) {
                    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
                    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body.length());
                }
                break;
        }
    }

    void applyHeaders(CURL* curl) {
        if (headerList_ != nullptr) {
            curl_slist_free_all(headerList_);
            headerList_ = nullptr;
        }
        
        for (const auto& header : headers_) {
            std::string headerStr = header.first + ": " + header.second;
            headerList_ = curl_slist_append(headerList_, headerStr.c_str());
        }
        
        if (!cookies_.empty()) {
            std::string cookieHeader = "Cookie: " + cookies_;
            headerList_ = curl_slist_append(headerList_, cookieHeader.c_str());
        }
        
        if (headerList_ != nullptr) {
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList_);
        }
    }

    HttpResponse request(const std::string& url, HttpMethod method, const std::string& body, HttpClient* client_ptr) {
        std::string responseBuffer;
        HttpResponse response;
        response.client_ptr = client_ptr;
        
        CURL* curl = initCurl(url, responseBuffer);
        
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerCallback);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response);
        
        setMethodOptions(curl, method, body);
        
        applyHeaders(curl);
        
        CURLcode res = curl_easy_perform(curl);
        
        if (res != CURLE_OK) {
            std::string errorMsg = "CURL error: ";
            errorMsg += curl_easy_strerror(res);
            curl_easy_cleanup(curl);
            throw std::runtime_error(errorMsg);
        }
        
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status_code);
        
        response.body = responseBuffer;
        
        curl_easy_cleanup(curl);
        
        return response;
    }

    HttpResponse requestWithManualRedirects(const std::string& url, HttpMethod method, const std::string& body, HttpClient* client_ptr) {
        HttpResponse response = request(url, method, body, client_ptr);
        
        int redirectCount = 0;
        const int MAX_REDIRECTS = 10;
        
        while ((response.status_code == HTTP_MOVED_PERMANENTLY || 
                response.status_code == HTTP_FOUND || 
                response.status_code == HTTP_SEE_OTHER || 
                response.status_code == HTTP_TEMPORARY_REDIRECT || 
                response.status_code == HTTP_PERMANENT_REDIRECT) && 
               !response.redirect_url.empty() && 
               redirectCount < MAX_REDIRECTS) {
            
            // For 303 See Other, always use GET for the redirect
            HttpMethod redirectMethod = (response.status_code == HTTP_SEE_OTHER) ? HttpMethod::GET : method;
            
            // For 301 Moved Permanently and 302 Found, use GET if the original method was POST
            if ((response.status_code == HTTP_MOVED_PERMANENTLY || 
                 response.status_code == HTTP_FOUND) && 
                method == HttpMethod::POST) {
                redirectMethod = HttpMethod::GET;
            }
            
            std::string redirectBody = (redirectMethod == HttpMethod::GET) ? "" : body;
            response = request(response.redirect_url, redirectMethod, redirectBody, client_ptr);
            
            redirectCount++;
        }
        
        return response;
    }

    OAuth2Token getOAuth2TokenWithJWT(const std::string& serviceAccountJson, const std::string& scope, const std::string& tokenEndpoint, HttpClient* client_ptr) {
        OAuth2Params params;
        params.service_account_json = serviceAccountJson;
        params.scope = scope;
        params.token_endpoint = tokenEndpoint;
        return getOAuth2TokenWithJWT(params, client_ptr);
    }

    OAuth2Token getOAuth2TokenWithJWT(const OAuth2Params& params, HttpClient* client_ptr) {
        crow::json::rvalue serviceAccount = crow::json::load(params.service_account_json);
        
        std::string clientEmail = serviceAccount["client_email"].s();
        std::string privateKey = serviceAccount["private_key"].s();

        crow::json::wvalue header;
        header["alg"] = "RS256";
        header["typ"] = "JWT";
        
        std::time_t now = std::time(nullptr);
        std::time_t expiry = now + TOKEN_EXPIRY_SECONDS;
        
        crow::json::wvalue claims;
        claims["iss"] = clientEmail;
        claims["scope"] = params.scope;
        claims["aud"] = params.token_endpoint;
        claims["exp"] = expiry;
        claims["iat"] = now;
        
        std::string encodedHeader = base64UrlEncode(header.dump());
        std::string encodedClaims = base64UrlEncode(claims.dump());
        
        std::string jwtContent = encodedHeader + "." + encodedClaims;
        
        std::string signature = signWithRSA(jwtContent, privateKey);
        
        std::string jwt = jwtContent + "." + base64UrlEncode(signature);
        
        std::string requestBody = "grant_type=urn:ietf:params:oauth:grant-type:jwt-bearer&assertion=" + jwt;
        
        std::map<std::string, std::string> originalHeaders = headers_;
        addHeader("Content-Type", "application/x-www-form-urlencoded");
        
        HttpResponse response = request(params.token_endpoint, HttpMethod::POST, requestBody, client_ptr);
        
        setHeaders(originalHeaders);
        
        if (response.status_code != HTTP_OK) {
            throw std::runtime_error("OAuth2 token request failed: " + response.body);
        }
        
        crow::json::rvalue tokenResponse = crow::json::load(response.body);
        
        OAuth2Token token;
        token.access_token = tokenResponse["access_token"].s();
        token.token_type = tokenResponse["token_type"].s();
        token.expires_in = static_cast<int>(tokenResponse["expires_in"].i());
        
        if (tokenResponse.has("refresh_token")) {
            token.refresh_token = tokenResponse["refresh_token"].s();
        }
        
        if (tokenResponse.has("scope")) {
            token.scope = tokenResponse["scope"].s();
        }
        
        token.expiry_time = now + token.expires_in;
        
        return token;
    }

    static std::string base64UrlEncode(const std::string& input) {
        if (input.empty()) {
            return "";
        }
        
        BIO* bio = nullptr;
        BIO* b64 = nullptr;
        
        b64 = BIO_new(BIO_f_base64());
        bio = BIO_new(BIO_s_mem());
        bio = BIO_push(b64, bio);
        
        BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
        
        BIO_write(bio, input.c_str(), static_cast<int>(input.length()));
        BIO_flush(bio);
        
        if (bio == nullptr) {
            throw std::runtime_error("Failed to create BIO memory");
        }
        
        BUF_MEM *bufferPtr;
        BIO_get_mem_ptr(bio, &bufferPtr);
        
        std::string result(bufferPtr->data, bufferPtr->length);
        
        BIO_free_all(bio);
        
        // Replace '+' with '-' and '/' with '_' for URL safety
        for (char& character : result) {
            if (character == '+') {
                character = '-';
            }
            if (character == '/') {
                character = '_';
            }
        }
        
        while (!result.empty() && result.back() == '=') {
            result.pop_back();
        }
        
        return result;
    }
    
    static std::string signWithRSA(const std::string& data, const std::string& privateKey) {
        BIO* bio = BIO_new_mem_buf(privateKey.c_str(), -1);
        if (bio == nullptr) {
            throw std::runtime_error("Failed to create BIO from private key");
        }
        
        EVP_PKEY* pkey = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
        BIO_free(bio);
        
        if (pkey == nullptr) {
            throw std::runtime_error("Failed to read private key");
        }
        
        EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
        if (mdctx == nullptr) {
            EVP_PKEY_free(pkey);
            throw std::runtime_error("Failed to create message digest context");
        }
        
        if (EVP_DigestSignInit(mdctx, nullptr, EVP_sha256(), nullptr, pkey) != 1) {
            EVP_PKEY_free(pkey);
            EVP_MD_CTX_free(mdctx);
            throw std::runtime_error("Failed to initialize signing operation");
        }
        
        if (EVP_DigestSignUpdate(mdctx, data.c_str(), data.length()) != 1) {
            EVP_PKEY_free(pkey);
            EVP_MD_CTX_free(mdctx);
            throw std::runtime_error("Failed to update signing operation");
        }
        
        size_t sig_len;
        if (EVP_DigestSignFinal(mdctx, nullptr, &sig_len) != 1) {
            EVP_PKEY_free(pkey);
            EVP_MD_CTX_free(mdctx);
            throw std::runtime_error("Failed to determine signature length");
        }
        
        unsigned char* sig = static_cast<unsigned char*>(OPENSSL_malloc(sig_len));
        if (sig == nullptr) {
            EVP_MD_CTX_free(mdctx);
            EVP_PKEY_free(pkey);
            throw std::runtime_error("Failed to allocate memory for signature");
        }
        
        if (EVP_DigestSignFinal(mdctx, sig, &sig_len) != 1) {
            OPENSSL_free(sig);
            EVP_PKEY_free(pkey);
            EVP_MD_CTX_free(mdctx);
            throw std::runtime_error("Failed to create signature");
        }
        
        std::string signature(reinterpret_cast<char*>(sig), sig_len);
        
        OPENSSL_free(sig);
        EVP_PKEY_free(pkey);
        EVP_MD_CTX_free(mdctx);
        
        return signature;
    }

    std::map<std::string, std::string> headers_;
    std::string cookies_;
    struct curl_slist* headerList_;
};

HttpClient::HttpClient() : impl_(std::make_unique<HttpClientImpl>()) {
}

HttpClient::~HttpClient() = default;

HttpClient::HttpClient(HttpClient&&) noexcept = default;

HttpClient& HttpClient::operator=(HttpClient&&) noexcept = default;

void HttpClient::setHeaders(const std::map<std::string, std::string>& headers) {
    impl_->setHeaders(headers);
}

void HttpClient::addHeader(const std::string& name, const std::string& value) {
    impl_->addHeader(name, value);
}

void HttpClient::setCookies(const std::string& cookies) {
    impl_->setCookies(cookies);
}

std::string HttpClient::getCookies() const {
    return impl_->getCookies();
}

HttpResponse HttpClient::request(const std::string& url, HttpMethod method, const std::string& body) {
    return impl_->request(url, method, body, this);
}

HttpResponse HttpClient::requestWithManualRedirects(const std::string& url, HttpMethod method, const std::string& body) {
    return impl_->requestWithManualRedirects(url, method, body, this);
}

OAuth2Token HttpClient::getOAuth2TokenWithJWT(const OAuth2Params& params) {
    return impl_->getOAuth2TokenWithJWT(params, this);
}

OAuth2Token HttpClient::getOAuth2TokenWithJWT(const std::string& serviceAccountJson, const std::string& scope, const std::string& tokenEndpoint) {
    return impl_->getOAuth2TokenWithJWT(serviceAccountJson, scope, tokenEndpoint, this);
}

std::string HttpClient::base64UrlEncode(const std::string& input) {
    return HttpClientImpl::base64UrlEncode(input);
}

std::string HttpClient::signWithRSA(const std::string& data, const std::string& privateKey) {
    return HttpClientImpl::signWithRSA(data, privateKey);
}

} // namespace cppwebforge
