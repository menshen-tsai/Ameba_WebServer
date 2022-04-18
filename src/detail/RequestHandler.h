#ifndef REQUESTHANDLER_H
#define REQUESTHANDLER_H

class AmebaWebServer;			// Forward declaration

class RequestHandler {
public:
    virtual bool canHandle(HTTPMethod method, String uri) { return false; }
    virtual bool canUpload(String uri) { return false; }
    virtual bool handle(AmebaWebServer& server, HTTPMethod requestMethod, String requestUri) { return false; }
    virtual void upload(AmebaWebServer& server, String requestUri, HTTPUpload& upload) {}

    RequestHandler* next() { return _next; }
    void next(RequestHandler* r) { _next = r; }

private:
    RequestHandler* _next = nullptr;
};

#endif //REQUESTHANDLER_H
