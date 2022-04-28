/*
  AmebaWebServer.cpp - Library for Arduino Wifi shield.
  Copyright (c) 2011-2014 Arduino LLC.  All right reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "AmebaWebServer.h"
#include "detail/RequestHandlersImpl.h"

#include <inttypes.h>

#include "wifi_drv.h"
#include "wiring.h"


// #define DEBUG
#define DEBUG_OUTPUT Serial

AmebaWebServer::AmebaWebServer(uint16_t port)
: _server(port)
, _currentMethod(HTTP_ANY)
, _currentHandler(0)
, _firstHandler(0)
, _lastHandler(0)
, _currentArgCount(0)
, _currentArgs(0)
, _headerKeysCount(0)
, _currentHeaders(0)
, _contentLength(0)

{
////	_port = port;
}

AmebaWebServer::~AmebaWebServer() {
  if (_currentHeaders)
    delete[]_currentHeaders;
  _headerKeysCount = 0;
  RequestHandler* handler = _firstHandler;
  while (handler) {
    RequestHandler* next = handler->next();
    delete handler;
    handler = next;
  }
}


void AmebaWebServer::init()
{
    WiFiDrv::wifiDriverInit();
}

char* AmebaWebServer::firmwareVersion()
{
    return WiFiDrv::getFwVersion();
}

void AmebaWebServer::begin() {
  _server.begin();
}


void AmebaWebServer::on(const char* uri, AmebaWebServer::THandlerFunction handler) {
  on(uri, HTTP_ANY, handler);
}

void AmebaWebServer::on(const char* uri, HTTPMethod method, AmebaWebServer::THandlerFunction fn) {
  on(uri, method, fn, _fileUploadHandler);
}

void AmebaWebServer::on(const char* uri, HTTPMethod method, AmebaWebServer::THandlerFunction fn, AmebaWebServer::THandlerFunction ufn) {
  _addRequestHandler(new FunctionRequestHandler(fn, ufn, uri, method));
}


void AmebaWebServer::_addRequestHandler(RequestHandler* handler) {
    if (!_lastHandler) {
      _firstHandler = handler;
      _lastHandler = handler;
    }
    else {
      _lastHandler->next(handler);
      _lastHandler = handler;
    }
}

void AmebaWebServer::serveStatic(const char* uri, FatFsSD& fs, const char* path, const char* cache_header) {
////    _addRequestHandler(new StaticRequestHandler(fs, path, uri, cache_header));
}

////
WiFiClient AmebaWebServer::available() {
	return _server.available();
}

void AmebaWebServer::handleClient() {
  WiFiClient client = _server.available();
  if (!client) {
    return;
  }

#ifdef DEBUG
  DEBUG_OUTPUT.println("New client");
#endif


  // Wait for data from client to become available
  uint16_t maxWait = HTTP_MAX_DATA_WAIT;
  while(client.connected() && !client.available() && maxWait--){
    delay(1);
  }

  if (!_parseRequest(client)) {
    return;
  }

  _currentClient = client;
  _contentLength = CONTENT_LENGTH_NOT_SET;
  _handleRequest();
}

void AmebaWebServer::sendHeader(const String& name, const String& value, bool first) {
  String headerLine = name;
  headerLine += ": ";
  headerLine += value;
  headerLine += "\r\n";

  if (first) {
    _responseHeaders = headerLine + _responseHeaders;
  }
  else {
    _responseHeaders += headerLine;
  }
}

void AmebaWebServer::_prepareHeader(String& response, int code, const char* content_type, size_t contentLength) {
    response = "HTTP/1.1 ";
    response += String(code);
    response += " ";
    response += _responseCodeToString(code);
    response += "\r\n";

    if (!content_type)
        content_type = "text/html";

    sendHeader("Content-Type", content_type, true);
    if (_contentLength != CONTENT_LENGTH_UNKNOWN && _contentLength != CONTENT_LENGTH_NOT_SET) {
        sendHeader("Content-Length", String(_contentLength));
    }
    else if (contentLength > 0){
        sendHeader("Content-Length", String(contentLength));
    }
    sendHeader("Connection", "close");
    sendHeader("Access-Control-Allow-Origin", "*");

    response += _responseHeaders;
    response += "\r\n";
    _responseHeaders = String();
}

void AmebaWebServer::send(int code, const char* content_type, const String& content) {
    String header;
    _prepareHeader(header, code, content_type, content.length());
    sendContent(header);

    sendContent(content);
}

void AmebaWebServer::send_P(int code, PGM_P content_type, PGM_P content) {
    size_t contentLength = 0;

    if (content != NULL) {
        contentLength = strlen_P(content);
    }
    String header;
    char type[64];
    memccpy_P((void*)type, (PGM_VOID_P)content_type, 0, sizeof(type));
    _prepareHeader(header, code, (const char* )type, contentLength);
    sendContent(header);
    sendContent_P(content);
	
}

void AmebaWebServer::send_P(int code, PGM_P content_type, PGM_P content, size_t contentLength) {
    String header;
    char type[64];
    memccpy_P((void*)type, (PGM_VOID_P)content_type, 0, sizeof(type));
    _prepareHeader(header, code, (const char* )type, contentLength);
    sendContent(header);
    sendContent_P(content, contentLength);

}

void AmebaWebServer::send(int code, char* content_type, const String& content) {

  send(code, (const char*)content_type, content);
}


	
void AmebaWebServer::send(int code, const String& content_type, const String& content) {

  send(code, (const char*)content_type.c_str(), content);
}

void AmebaWebServer::sendContent(const String& content) {
  const size_t unit_size = HTTP_DOWNLOAD_UNIT_SIZE;
  size_t size_to_send = content.length();
  const char* send_start = content.c_str();

  while (size_to_send) {
    size_t will_send = (size_to_send < unit_size) ? size_to_send : unit_size;
    size_t sent = _currentClient.write(send_start, will_send);
    if (sent == 0) {
      break;
    }
    size_to_send -= sent;
    send_start += sent;
  }
}

void AmebaWebServer::sendContent_P(PGM_P content) {
	    char contentUnit[HTTP_DOWNLOAD_UNIT_SIZE + 1];

    contentUnit[HTTP_DOWNLOAD_UNIT_SIZE] = '\0';

    while (content != NULL) {
        size_t contentUnitLen;
        PGM_P contentNext;

        // due to the memccpy signature, lots of casts are needed
        contentNext = (PGM_P)memccpy_P((void*)contentUnit, (PGM_VOID_P)content, 0, HTTP_DOWNLOAD_UNIT_SIZE);

        if (contentNext == NULL) {
            // no terminator, more data available
            content += HTTP_DOWNLOAD_UNIT_SIZE;
            contentUnitLen = HTTP_DOWNLOAD_UNIT_SIZE;
        }
        else {
            // reached terminator. Do not send the terminator
            contentUnitLen = contentNext - contentUnit - 1;
            content = NULL;
        }

        // write is so overloaded, had to use the cast to get it pick the right one
        _currentClient.write((const char*)contentUnit, contentUnitLen);
    }

}

void AmebaWebServer::sendContent_P(PGM_P content, size_t size) {
    char contentUnit[HTTP_DOWNLOAD_UNIT_SIZE + 1];
    contentUnit[HTTP_DOWNLOAD_UNIT_SIZE] = '\0';
    size_t remaining_size = size;

    while (content != NULL && remaining_size > 0) {
        size_t contentUnitLen = HTTP_DOWNLOAD_UNIT_SIZE;

        if (remaining_size < HTTP_DOWNLOAD_UNIT_SIZE) contentUnitLen = remaining_size;
        // due to the memcpy signature, lots of casts are needed
        memcpy_P((void*)contentUnit, (PGM_VOID_P)content, contentUnitLen);

        content += contentUnitLen;
        remaining_size -= contentUnitLen;

        // write is so overloaded, had to use the cast to get it pick the right one
        _currentClient.write((const char*)contentUnit, contentUnitLen);
    }

}


String AmebaWebServer::arg(const char* name) {
  for (int i = 0; i < _currentArgCount; ++i) {
    if (_currentArgs[i].key == name)
      return _currentArgs[i].value;
  }
  return String();
}

String AmebaWebServer::arg(int i) {
  if (i < _currentArgCount)
    return _currentArgs[i].value;
  return String();
}

String AmebaWebServer::argName(int i) {
  if (i < _currentArgCount)
    return _currentArgs[i].key;
  return String();
}

int AmebaWebServer::args() {
  return _currentArgCount;
}

bool AmebaWebServer::hasArg(const char* name) {
  for (int i = 0; i < _currentArgCount; ++i) {
    if (_currentArgs[i].key == name)
      return true;
  }
  return false;
}

String AmebaWebServer::header(const char* name) {
  for (int i = 0; i < _headerKeysCount; ++i) {
    if (_currentHeaders[i].key == name)
      return _currentHeaders[i].value;
  }
  return String();
}

void AmebaWebServer::collectHeaders(const char* headerKeys[], const size_t headerKeysCount) {
  _headerKeysCount = headerKeysCount;
  if (_currentHeaders)
     delete[]_currentHeaders;
  _currentHeaders = new RequestArgument[_headerKeysCount];
  for (int i = 0; i < _headerKeysCount; i++){
    _currentHeaders[i].key = headerKeys[i];
  }
}


String AmebaWebServer::header(int i) {
  if (i < _headerKeysCount)
    return _currentHeaders[i].value;
  return String();
}

String AmebaWebServer::headerName(int i) {
  if (i < _headerKeysCount)
    return _currentHeaders[i].key;
  return String();
}

int AmebaWebServer::headers() {
  return _headerKeysCount;
}

bool AmebaWebServer::hasHeader(const char* name) {
  for (int i = 0; i < _headerKeysCount; ++i) {
    if ((_currentHeaders[i].key == name) &&  (_currentHeaders[i].value.length() > 0))
      return true;
  }
  return false;
}

String AmebaWebServer::hostHeader() {
  return _hostHeader;
}

void AmebaWebServer::onFileUpload(THandlerFunction fn) {
  _fileUploadHandler = fn;
}

void AmebaWebServer::onNotFound(THandlerFunction fn) {
  _notFoundHandler = fn;
}

void AmebaWebServer::_handleRequest() {
  bool handled = false;
  
  if (!_currentHandler){
#ifdef DEBUG
    DEBUG_OUTPUT.println("request handler not found");
#endif
  }
  else {
    handled = _currentHandler->handle(*this, _currentMethod, _currentUri);
#ifdef DEBUG
    if (!handled) {
      DEBUG_OUTPUT.println("request handler failed to handle request");
    }
#endif
  }

  
  if (!handled) {
    if(_notFoundHandler) {
      _notFoundHandler();
    }
    else {
      send(404, "text/plain", String("Not found: ") + _currentUri);
    }
  }

  uint16_t maxWait = HTTP_MAX_CLOSE_WAIT;
  while(_currentClient.connected() && maxWait--) {
    delay(1);
  }
  
#ifdef DEBUG
  DEBUG_OUTPUT.println("Stop Client");
#endif

  _currentClient.stop();
  
  _currentClient   = WiFiClient();
  _currentUri      = String();
  

}

const char* AmebaWebServer::_responseCodeToString(int code) {
  switch (code) {
    case 100: return "Continue";
    case 101: return "Switching Protocols";
    case 200: return "OK";
    case 201: return "Created";
    case 202: return "Accepted";
    case 203: return "Non-Authoritative Information";
    case 204: return "No Content";
    case 205: return "Reset Content";
    case 206: return "Partial Content";
    case 300: return "Multiple Choices";
    case 301: return "Moved Permanently";
    case 302: return "Found";
    case 303: return "See Other";
    case 304: return "Not Modified";
    case 305: return "Use Proxy";
    case 307: return "Temporary Redirect";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 402: return "Payment Required";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 406: return "Not Acceptable";
    case 407: return "Proxy Authentication Required";
    case 408: return "Request Time-out";
    case 409: return "Conflict";
    case 410: return "Gone";
    case 411: return "Length Required";
    case 412: return "Precondition Failed";
    case 413: return "Request Entity Too Large";
    case 414: return "Request-URI Too Large";
    case 415: return "Unsupported Media Type";
    case 416: return "Requested range not satisfiable";
    case 417: return "Expectation Failed";
    case 500: return "Internal Server Error";
    case 501: return "Not Implemented";
    case 502: return "Bad Gateway";
    case 503: return "Service Unavailable";
    case 504: return "Gateway Time-out";
    case 505: return "HTTP Version not supported";
    default:  return "";
  }
}


////AmebaWebServer WebServer;
