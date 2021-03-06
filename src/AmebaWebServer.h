/*
  AmebaWebServer.h - Library for Arduino Wifi shield.
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

#ifndef AmebaWebServer_h
#define AmebaWebServer_h

#include <Arduino.h>
#include <inttypes.h>

#undef max
#undef min
#include <functional>		// Cause compiling errors

#define         PGM_P           const char *
#define         PGM_VOID_P      const void *

extern "C" {
    #include "wl_definitions.h"
    #include "wl_types.h"
	
    char* unconstchar(const char* s) ;
    void *__mempcpy (void *dest, const void *src, size_t len);
    void *__memccpy (void *dest, const void *src, int c, size_t n);
}

#define memccpy_P(dest, src, c, n) __memccpy((dest), (src), (c), (n))

#include <WiFi.h>
//#include "IPAddress.h"
//#include "IPv6Address.h"
#include "WiFiClient.h"
#include "WiFiServer.h"
//#include "WiFiSSLClient.h"
//#include "WiFiUdp.h"
#include "FatFs_SD.h"

#define FPSTR(pstr_pointer) (reinterpret_cast<const __FlashStringHelper *>(pstr_pointer))



enum HTTPMethod { HTTP_ANY, HTTP_GET, HTTP_POST, HTTP_PUT, 
                  HTTP_PATCH, HTTP_DELETE, HTTP_OPTIONS };
				  
enum HTTPUploadStatus { UPLOAD_FILE_START, UPLOAD_FILE_WRITE, 
                        UPLOAD_FILE_END, UPLOAD_FILE_ABORTED };

#define HTTP_DOWNLOAD_UNIT_SIZE 1460
#define HTTP_UPLOAD_BUFLEN 2048
#define HTTP_MAX_DATA_WAIT 1000 //ms to wait for the client to send the request
#define HTTP_MAX_CLOSE_WAIT 2000 //ms to wait for the client to close the connection

#define CONTENT_LENGTH_UNKNOWN ((size_t) -1)
#define CONTENT_LENGTH_NOT_SET ((size_t) -2)




class AmberWebServer;

typedef struct {
  HTTPUploadStatus status;
  String  filename;
  String  name;
  String  type;
  size_t  totalSize;    // file size
  size_t  currentSize;  // size of data currently in buf
  uint8_t buf[HTTP_UPLOAD_BUFLEN];
} HTTPUpload;

#include "detail/RequestHandler.h"


class AmebaWebServer
{
    public:
      AmebaWebServer();
      AmebaWebServer(uint16_t port);
      ~AmebaWebServer();

      void begin();
      void handleClient() ;
      virtual void close();
      void stop();
	  
      typedef std::function<void(void)> THandlerFunction;


      void on(const char* uri, THandlerFunction handler);
      void on(const char* uri, HTTPMethod method, THandlerFunction fn);
      void on(const char* uri, HTTPMethod method, THandlerFunction fn, THandlerFunction ufn);

      void addHandler(RequestHandler* handler);
	  
////      void serveStatic(const char* uri, fs::FS& fs, const char* path, const char* cache_header = NULL );
      void serveStatic(const char* uri, FatFsSD& fs, const char* path, const char* cache_header = NULL );
      void onNotFound(THandlerFunction fn);  //called when handler is not assigned
      void onFileUpload(THandlerFunction fn); //handle file uploads

      String uri() { return _currentUri; }
      HTTPMethod method() { return _currentMethod; }
      WiFiClient client() { return _currentClient; }
      HTTPUpload& upload() { return _currentUpload; }

      String arg(const char* name);   // get request argument value by name
      String arg(int i);              // get request argument value by number
      String argName(int i);          // get request argument name by number
      int args();                     // get arguments count
      bool hasArg(const char* name);  // check if argument exists
      void collectHeaders(const char* headerKeys[], const size_t headerKeysCount); // set the request headers to collect
      String header(const char* name);   // get request header value by name
      String header(int i);              // get request header value by number
      String headerName(int i);          // get request header name by number
      int headers();                     // get header count
      bool hasHeader(const char* name);  // check if header exists

      String hostHeader();            // get request host header if available or empty String if not

  // send response to the client
  // code - HTTP response code, can be 200 or 404
  // content_type - HTTP content type, like "text/plain" or "image/png"
  // content - actual content body
  void send(int code, const char* content_type = NULL, const String& content = String(""));
  void send(int code, char* content_type, const String& content);
  void send(int code, const String& content_type, const String& content);
  void send_P(int code, PGM_P content_type, PGM_P content);
  void send_P(int code, PGM_P content_type, PGM_P content, size_t contentLength);

  void setContentLength(size_t contentLength) { _contentLength = contentLength; }
  void sendHeader(const String& name, const String& value, bool first = false);
  void sendContent(const String& content);
  void sendContent_P(PGM_P content);
  void sendContent_P(PGM_P content, size_t size);


//  Temporary Variables/Functions for testing
	  WiFiClient available();  
      static char* firmwareVersion();



      friend class WiFiClient;
      friend class WiFiServer;
      friend class WiFiSSLClient;
		
    protected:
	  void _addRequestHandler(RequestHandler* handler);
	  void _handleRequest();
	  bool _parseRequest(WiFiClient& client);
      void _parseArguments(String data);
      static const char* _responseCodeToString(int code);
      bool _parseForm(WiFiClient& client, String boundary, uint32_t len);
      bool _parseFormUploadAborted();
      void _uploadWriteByte(uint8_t b);
      uint8_t _uploadReadByte(WiFiClient& client);
      void _prepareHeader(String& response, int code, const char* content_type, size_t contentLength);
      bool _collectHeader(const char* headerName, const char* headerValue);
      String urlDecode(const String& text);


      struct RequestArgument {
        String key;
        String value;
      };		

	  
      WiFiServer  _server;
	  
      WiFiClient  _currentClient;
      HTTPMethod  _currentMethod;
      String      _currentUri;

      RequestHandler*  _currentHandler;
      RequestHandler*  _firstHandler;
      RequestHandler*  _lastHandler;
      THandlerFunction _notFoundHandler;
      THandlerFunction _fileUploadHandler;

      int              _currentArgCount;
      RequestArgument* _currentArgs;
      HTTPUpload       _currentUpload;

      int              _headerKeysCount;
      RequestArgument* _currentHeaders;	  
      size_t           _contentLength;
      String           _responseHeaders;
 
      String           _hostHeader;	  

    private:
      static void init();
	  uint16_t _port;
	
};

////extern AmebaWebServer WebServer;

#endif
