#ifndef REQUESTHANDLERSIMPL_H
#define REQUESTHANDLERSIMPL_H

#include "FatFs_SD.h"
#include "RequestHandler.h"

class FunctionRequestHandler : public RequestHandler {
public:
    FunctionRequestHandler(AmebaWebServer::THandlerFunction fn, AmebaWebServer::THandlerFunction ufn, const char* uri, HTTPMethod method)
    : _fn(fn)
    , _ufn(ufn)
    , _uri(uri)
    , _method(method)
    {
    }

    bool canHandle(HTTPMethod requestMethod, String requestUri) override  {
        if (_method != HTTP_ANY && _method != requestMethod)
            return false;

        if (requestUri != _uri)
            return false;

        return true;
    }

    bool canUpload(String requestUri) override  {
        if (!_ufn || !canHandle(HTTP_POST, requestUri))
            return false;

        return true;
    }

    bool handle(AmebaWebServer& server, HTTPMethod requestMethod, String requestUri) override {
        (void) server;
        if (!canHandle(requestMethod, requestUri))
            return false;

        _fn();
        return true;
    }

    void upload(AmebaWebServer& server, String requestUri, HTTPUpload& upload) override {
        (void) server;
        (void) upload;
        if (canUpload(requestUri))
            _ufn();
    }


protected:
    AmebaWebServer::THandlerFunction _fn;
    AmebaWebServer::THandlerFunction _ufn;
    String _uri;
    HTTPMethod _method;
};


class StaticRequestHandler : public RequestHandler {
public:
////    StaticRequestHandler(FS& fs, const char* path, const char* uri, const char* cache_header)
    StaticRequestHandler(FatFsSD& fs, char* path, char* uri, char* cache_header)	
    : _fs(fs)
    , _uri(uri)
    , _path(path)
    , _cache_header(cache_header)
    {
////        _isFile = fs.exists(path);
        _isFile = fs.isFile(unconstchar(path));
////        DEBUGV("StaticRequestHandler: path=%s uri=%s isFile=%d, cache_header=%s\r\n", path, uri, _isFile, cache_header);
        _baseUriLength = _uri.length();
    }
	

    bool canHandle(HTTPMethod requestMethod, String requestUri) override  {
        if (requestMethod != HTTP_GET)
            return false;

        if ((_isFile && requestUri != _uri) || !requestUri.startsWith(_uri))
            return false;

        return true;
    }
#if 1
    bool handle(AmebaWebServer& server, HTTPMethod requestMethod, String requestUri) override {
        if (!canHandle(requestMethod, requestUri))
            return false;

////        DEBUGV("StaticRequestHandler::handle: request=%s _uri=%s\r\n", requestUri.c_str(), _uri.c_str());

        String path(_path);

        if (!_isFile) {
            // Base URI doesn't point to a file.
            // If a directory is requested, look for index file.
            if (requestUri.endsWith("/")) requestUri += "index.htm";

            // Append whatever follows this URI in request to get the file path.
            path += requestUri.substring(_baseUriLength);
        }
////        DEBUGV("StaticRequestHandler::handle: path=%s, isFile=%d\r\n", path.c_str(), _isFile);

        String contentType = getContentType(path);

        // look for gz file, only if the original specified path is not a gz.  So part only works to send gzip via content encoding when a non compressed is asked for
        // if you point the the path to gzip you will serve the gzip as content type "application/x-gzip", not text or javascript etc...
		
////    if (!path.endsWith(".gz") && !SPIFFS.exists(path))  {
        if (!path.endsWith(".gz") && !_fs.isFile(unconstchar(path.c_str())))  {			
            String pathWithGz = path + ".gz";
////            if(_fs.exists(pathWithGz))
            if(_fs.isFile(unconstchar(pathWithGz.c_str())))				
                path += ".gz";
        }

//        File f = _fs.open(path, "r");
        SdFatFile f = _fs.open(unconstchar(path.c_str()));
        if (!f)
            return false;

        if (_cache_header.length() != 0)
            server.sendHeader("Cache-Control", _cache_header);

////        server.streamFile(f, contentType);
        return true;
    }
#endif

    static String getContentType(const String& path) {
        if (path.endsWith(".html")) return "text/html";
        else if (path.endsWith(".htm")) return "text/html";
        else if (path.endsWith(".css")) return "text/css";
        else if (path.endsWith(".txt")) return "text/plain";
        else if (path.endsWith(".js")) return "application/javascript";
        else if (path.endsWith(".png")) return "image/png";
        else if (path.endsWith(".gif")) return "image/gif";
        else if (path.endsWith(".jpg")) return "image/jpeg";
        else if (path.endsWith(".ico")) return "image/x-icon";
        else if (path.endsWith(".svg")) return "image/svg+xml";
        else if (path.endsWith(".xml")) return "text/xml";
        else if (path.endsWith(".pdf")) return "application/pdf";
        else if (path.endsWith(".zip")) return "application/zip";
        else if(path.endsWith(".gz")) return "application/x-gzip";
        return "application/octet-stream";
    }

protected:
//    FS _fs;
	FatFsSD _fs;
    String _uri;
    String _path;
    String _cache_header;
    bool _isFile;
    size_t _baseUriLength;
};

				// StaticRequestHandler

#endif //REQUESTHANDLERSIMPL_H
