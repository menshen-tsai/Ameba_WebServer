/*
  Parsing.cpp - HTTP request parsing.

  Copyright (c) 2015 Ivan Grokhotkov. All rights reserved.

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
  Modified 8 May 2015 by Hristo Gochkov (proper post and file upload handling)
*/

#include <Arduino.h>
#include "WiFiServer.h"
#include "WiFiClient.h"
#include "AmebaWebServer.h"

#define DEBUG_OUTPUT Serial

bool AmebaWebServer::_parseRequest(WiFiClient& client) {
  // Read the first line of HTTP request
  String req = client.readStringUntil('\r');
  client.readStringUntil('\n');
  //reset header value
  for (int i = 0; i < _headerKeysCount; ++i) {
    _currentHeaders[i].value =String();
  }
  
  // First line of HTTP request looks like "GET /path HTTP/1.1"
  // Retrieve the "/path" part by finding the spaces
  int addr_start = req.indexOf(' ');
  int addr_end = req.indexOf(' ', addr_start + 1);
  if (addr_start == -1 || addr_end == -1) {
#ifdef DEBUG
    DEBUG_OUTPUT.print("Invalid request: ");
    DEBUG_OUTPUT.println(req);
#endif
    return false;
  }

  String methodStr = req.substring(0, addr_start);
  String url = req.substring(addr_start + 1, addr_end);
  String searchStr = "";
  int hasSearch = url.indexOf('?');
  if (hasSearch != -1){
    searchStr = url.substring(hasSearch + 1);
    url = url.substring(0, hasSearch);
  }
  _currentUri = url;


  HTTPMethod method = HTTP_GET;
  if (methodStr == "POST") {
    method = HTTP_POST;
  } else if (methodStr == "DELETE") {
    method = HTTP_DELETE;
  } else if (methodStr == "OPTIONS") {
    method = HTTP_OPTIONS;
  } else if (methodStr == "PUT") {
    method = HTTP_PUT;
  } else if (methodStr == "PATCH") {
    method = HTTP_PATCH;
  }
  _currentMethod = method;

#ifdef DEBUG
  DEBUG_OUTPUT.print("method: ");
  DEBUG_OUTPUT.print(methodStr);
  DEBUG_OUTPUT.print(" url: ");
  DEBUG_OUTPUT.print(url);
  DEBUG_OUTPUT.print(" search: ");
  DEBUG_OUTPUT.println(searchStr);
#endif

  //attach handler
  RequestHandler* handler;
  for (handler = _firstHandler; handler; handler = handler->next()) {
    if (handler->canHandle(_currentMethod, _currentUri))
      break;
  }
  _currentHandler = handler;

////////////
  String formData;
  // below is needed only when POST type request
  if (method == HTTP_POST || method == HTTP_PUT || method == HTTP_PATCH || method == HTTP_DELETE){
    String boundaryStr;
    String headerName;
    String headerValue;
    bool isForm = false;
    uint32_t contentLength = 0;
    //parse headers
    while(1){
      req = client.readStringUntil('\r');
      client.readStringUntil('\n');
      if (req == "") break;//no moar headers
      int headerDiv = req.indexOf(':');
      if (headerDiv == -1){
        break;
      }
      headerName = req.substring(0, headerDiv);
      headerValue = req.substring(headerDiv + 1);
      headerValue.trim();
       _collectHeader(headerName.c_str(),headerValue.c_str());
	  
	  #ifdef DEBUG
	  DEBUG_OUTPUT.print("headerName: ");
	  DEBUG_OUTPUT.println(headerName);
	  DEBUG_OUTPUT.print("headerValue: ");
	  DEBUG_OUTPUT.println(headerValue);
	  #endif
	  
      if (headerName == "Content-Type"){
        if (headerValue.startsWith("text/plain")){
          isForm = false;
        } else if (headerValue.startsWith("multipart/form-data")){
          boundaryStr = headerValue.substring(headerValue.indexOf('=')+1);
          isForm = true;
        }
      } else if (headerName == "Content-Length"){
        contentLength = headerValue.toInt();
      } else if (headerName == "Host"){
        _hostHeader = headerValue;
      }
    }

    if (!isForm){
      if (searchStr != "") searchStr += '&';
      //some clients send headers first and data after (like we do)
      //give them a chance
      int tries = 100;//100ms max wait
      while(!client.available() && tries--)delay(1);
      size_t plainLen = client.available();
      char *plainBuf = (char*)malloc(plainLen+1);
      client.readBytes(plainBuf, plainLen);
      plainBuf[plainLen] = '\0';
#ifdef DEBUG
      DEBUG_OUTPUT.print("Plain: ");
      DEBUG_OUTPUT.println(plainBuf);
#endif
      if(plainBuf[0] == '{' || plainBuf[0] == '[' || strstr(plainBuf, "=") == NULL){
        //plain post json or other data
        searchStr += "plain=";
        searchStr += plainBuf;
      } else {
        searchStr += plainBuf;
      }
      free(plainBuf);
    }
    _parseArguments(searchStr);
    if (isForm){
      if (!_parseForm(client, boundaryStr, contentLength)) {
        return false;
      }
    }
  } else {
    String headerName;
    String headerValue;
    //parse headers
    while(1){
      req = client.readStringUntil('\r');
      client.readStringUntil('\n');
      if (req == "") break;//no moar headers
      int headerDiv = req.indexOf(':');
      if (headerDiv == -1){
        break;
      }
      headerName = req.substring(0, headerDiv);
      headerValue = req.substring(headerDiv + 2);
      _collectHeader(headerName.c_str(),headerValue.c_str());
	  
	  #ifdef DEBUG
	  DEBUG_OUTPUT.print("headerName: ");
	  DEBUG_OUTPUT.println(headerName);
	  DEBUG_OUTPUT.print("headerValue: ");
	  DEBUG_OUTPUT.println(headerValue);
	  #endif
	  
	  if (headerName == "Host"){
        _hostHeader = headerValue;
      }
    }
    _parseArguments(searchStr);
  }
  client.flush();

#ifdef DEBUG
  DEBUG_OUTPUT.print("Request: ");
  DEBUG_OUTPUT.println(url);
  DEBUG_OUTPUT.print(" Arguments: ");
  DEBUG_OUTPUT.println(searchStr);
#endif


///////////
  return true;

}

bool AmebaWebServer::_collectHeader(const char* headerName, const char* headerValue) {
  for (int i = 0; i < _headerKeysCount; i++) {
    if (_currentHeaders[i].key==headerName) {
            _currentHeaders[i].value=headerValue;
            return true;
        }
  }
  return false;
}

void AmebaWebServer::_parseArguments(String data) {
#ifdef DEBUG
  DEBUG_OUTPUT.print("args: ");
  DEBUG_OUTPUT.println(data);
#endif
  if (_currentArgs)
    delete[] _currentArgs;
  _currentArgs = 0;
  if (data.length() == 0) {
    _currentArgCount = 0;
    return;
  }
  _currentArgCount = 1;

  for (int i = 0; i < (int)data.length(); ) {
    i = data.indexOf('&', i);
    if (i == -1)
      break;
    ++i;
    ++_currentArgCount;
  }
#ifdef DEBUG
  DEBUG_OUTPUT.print("args count: ");
  DEBUG_OUTPUT.println(_currentArgCount);
#endif

  _currentArgs = new RequestArgument[_currentArgCount];
  int pos = 0;
  int iarg;
  for (iarg = 0; iarg < _currentArgCount;) {
    int equal_sign_index = data.indexOf('=', pos);
    int next_arg_index = data.indexOf('&', pos);
#ifdef DEBUG
    DEBUG_OUTPUT.print("pos ");
    DEBUG_OUTPUT.print(pos);
    DEBUG_OUTPUT.print("=@ ");
    DEBUG_OUTPUT.print(equal_sign_index);
    DEBUG_OUTPUT.print(" &@ ");
    DEBUG_OUTPUT.println(next_arg_index);
#endif
    if ((equal_sign_index == -1) || ((equal_sign_index > next_arg_index) && (next_arg_index != -1))) {
#ifdef DEBUG
      DEBUG_OUTPUT.print("arg missing value: ");
      DEBUG_OUTPUT.println(iarg);
#endif
      if (next_arg_index == -1)
        break;
      pos = next_arg_index + 1;
      continue;
    }
    RequestArgument& arg = _currentArgs[iarg];
    arg.key = data.substring(pos, equal_sign_index);
	arg.value = urlDecode(data.substring(equal_sign_index + 1, next_arg_index));
#ifdef DEBUG
    DEBUG_OUTPUT.print("arg ");
    DEBUG_OUTPUT.print(iarg);
    DEBUG_OUTPUT.print(" key: ");
    DEBUG_OUTPUT.print(arg.key);
    DEBUG_OUTPUT.print(" value: ");
    DEBUG_OUTPUT.println(arg.value);
#endif
    ++iarg;
    if (next_arg_index == -1)
      break;
    pos = next_arg_index + 1;
  }
  _currentArgCount = iarg;
#ifdef DEBUG
  DEBUG_OUTPUT.print("args count: ");
  DEBUG_OUTPUT.println(_currentArgCount);
#endif

}



void AmebaWebServer::_uploadWriteByte(uint8_t b){
  if (_currentUpload.currentSize == HTTP_UPLOAD_BUFLEN){
    if(_currentHandler && _currentHandler->canUpload(_currentUri))
      _currentHandler->upload(*this, _currentUri, _currentUpload);
    _currentUpload.totalSize += _currentUpload.currentSize;
    _currentUpload.currentSize = 0;
  }
  _currentUpload.buf[_currentUpload.currentSize++] = b;
}

uint8_t AmebaWebServer::_uploadReadByte(WiFiClient& client){
  int res = client.read();
  if(res == -1){
    while(!client.available() && client.connected())
      yield();
    res = client.read();
  }
  return (uint8_t)res;
}


bool AmebaWebServer::_parseForm(WiFiClient& client, String boundary, uint32_t len){

#ifdef DEBUG
  DEBUG_OUTPUT.print("Parse Form: Boundary: ");
  DEBUG_OUTPUT.print(boundary);
  DEBUG_OUTPUT.print(" Length: ");
  DEBUG_OUTPUT.println(len);
#endif
  String line;
  (void) len;
  int retry = 0;
  do {
    line = client.readStringUntil('\r');
    ++retry;
  } while (line.length() == 0 && retry < 3);

  client.readStringUntil('\n');
  //start reading the form
  if (line == ("--"+boundary)){
    RequestArgument* postArgs = new RequestArgument[32];
    int postArgsLen = 0;
    while(1){
      String argName;
      String argValue;
      String argType;
      String argFilename;
      bool argIsFile = false;

      line = client.readStringUntil('\r');
      client.readStringUntil('\n');
      if (line.startsWith("Content-Disposition")){
        int nameStart = line.indexOf('=');
        if (nameStart != -1){
          argName = line.substring(nameStart+2);
          nameStart = argName.indexOf('=');
          if (nameStart == -1){
            argName = argName.substring(0, argName.length() - 1);
          } else {
            argFilename = argName.substring(nameStart+2, argName.length() - 1);
            argName = argName.substring(0, argName.indexOf('"'));
            argIsFile = true;
#ifdef DEBUG
            DEBUG_OUTPUT.print("PostArg FileName: ");
            DEBUG_OUTPUT.println(argFilename);
#endif
            //use GET to set the filename if uploading using blob
            if (argFilename == "blob" && hasArg("filename")) argFilename = arg("filename");
          }
#ifdef DEBUG
          DEBUG_OUTPUT.print("PostArg Name: ");
          DEBUG_OUTPUT.println(argName);
#endif
          argType = "text/plain";
          line = client.readStringUntil('\r');
          client.readStringUntil('\n');
          if (line.startsWith("Content-Type")){
            argType = line.substring(line.indexOf(':')+2);
            //skip next line
            client.readStringUntil('\r');
            client.readStringUntil('\n');
          }
#ifdef DEBUG
          DEBUG_OUTPUT.print("PostArg Type: ");
          DEBUG_OUTPUT.println(argType);
#endif
          if (!argIsFile){
            while(1){
              line = client.readStringUntil('\r');
              client.readStringUntil('\n');
              if (line.startsWith("--"+boundary)) break;
              if (argValue.length() > 0) argValue += "\n";
              argValue += line;
            }
#ifdef DEBUG
            DEBUG_OUTPUT.print("PostArg Value: ");
            DEBUG_OUTPUT.println(argValue);
            DEBUG_OUTPUT.println();
#endif

            RequestArgument& arg = postArgs[postArgsLen++];
            arg.key = argName;
            arg.value = argValue;

            if (line == ("--"+boundary+"--")){
#ifdef DEBUG
              DEBUG_OUTPUT.println("Done Parsing POST");
#endif
              break;
            }
          } else {
            _currentUpload.status = UPLOAD_FILE_START;
            _currentUpload.name = argName;
            _currentUpload.filename = argFilename;
            _currentUpload.type = argType;
            _currentUpload.totalSize = 0;
            _currentUpload.currentSize = 0;
#ifdef DEBUG
            DEBUG_OUTPUT.print("Start File: ");
            DEBUG_OUTPUT.print(_currentUpload.filename);
            DEBUG_OUTPUT.print(" Type: ");
            DEBUG_OUTPUT.println(_currentUpload.type);
#endif
            if(_currentHandler && _currentHandler->canUpload(_currentUri))
              _currentHandler->upload(*this, _currentUri, _currentUpload);
            _currentUpload.status = UPLOAD_FILE_WRITE;
            uint8_t argByte = _uploadReadByte(client);
readfile:
            while(argByte != 0x0D){
              if (!client.connected()) return _parseFormUploadAborted();
              _uploadWriteByte(argByte);
              argByte = _uploadReadByte(client);
            }

            argByte = _uploadReadByte(client);
            if (!client.connected()) return _parseFormUploadAborted();
            if (argByte == 0x0A){
              argByte = _uploadReadByte(client);
              if (!client.connected()) return _parseFormUploadAborted();
              if ((char)argByte != '-'){
                //continue reading the file
                _uploadWriteByte(0x0D);
                _uploadWriteByte(0x0A);
                goto readfile;
              } else {
                argByte = _uploadReadByte(client);
                if (!client.connected()) return _parseFormUploadAborted();
                if ((char)argByte != '-'){
                  //continue reading the file
                  _uploadWriteByte(0x0D);
                  _uploadWriteByte(0x0A);
                  _uploadWriteByte((uint8_t)('-'));
                  goto readfile;
                }
              }

              uint8_t endBuf[boundary.length()];
              client.readBytes(endBuf, boundary.length());

              if (strstr((const char*)endBuf, boundary.c_str()) != NULL){
                if(_currentHandler && _currentHandler->canUpload(_currentUri))
                  _currentHandler->upload(*this, _currentUri, _currentUpload);
                _currentUpload.totalSize += _currentUpload.currentSize;
                _currentUpload.status = UPLOAD_FILE_END;
                if(_currentHandler && _currentHandler->canUpload(_currentUri))
                  _currentHandler->upload(*this, _currentUri, _currentUpload);
#ifdef DEBUG
                DEBUG_OUTPUT.print("End File: ");
                DEBUG_OUTPUT.print(_currentUpload.filename);
                DEBUG_OUTPUT.print(" Type: ");
                DEBUG_OUTPUT.print(_currentUpload.type);
                DEBUG_OUTPUT.print(" Size: ");
                DEBUG_OUTPUT.println(_currentUpload.totalSize);
#endif
                line = client.readStringUntil(0x0D);
                client.readStringUntil(0x0A);
                if (line == "--"){
#ifdef DEBUG
                  DEBUG_OUTPUT.println("Done Parsing POST");
#endif
                  break;
                }
                continue;
              } else {
                _uploadWriteByte(0x0D);
                _uploadWriteByte(0x0A);
                _uploadWriteByte((uint8_t)('-'));
                _uploadWriteByte((uint8_t)('-'));
                uint32_t i = 0;
                while(i < boundary.length()){
                  _uploadWriteByte(endBuf[i++]);
                }
                argByte = _uploadReadByte(client);
                goto readfile;
              }
            } else {
              _uploadWriteByte(0x0D);
              goto readfile;
            }
            break;
          }
        }
      }
    }

    int iarg;
    int totalArgs = ((32 - postArgsLen) < _currentArgCount)?(32 - postArgsLen):_currentArgCount;
    for (iarg = 0; iarg < totalArgs; iarg++){
      RequestArgument& arg = postArgs[postArgsLen++];
      arg.key = _currentArgs[iarg].key;
      arg.value = _currentArgs[iarg].value;
    }
    if (_currentArgs) delete[] _currentArgs;
    _currentArgs = new RequestArgument[postArgsLen];
    for (iarg = 0; iarg < postArgsLen; iarg++){
      RequestArgument& arg = _currentArgs[iarg];
      arg.key = postArgs[iarg].key;
      arg.value = postArgs[iarg].value;
    }
    _currentArgCount = iarg;
    if (postArgs) delete[] postArgs;
    return true;
  }
#ifdef DEBUG
  DEBUG_OUTPUT.print("Error: line: ");
  DEBUG_OUTPUT.println(line);
#endif
	
  return false;
}

String AmebaWebServer::urlDecode(const String& text)
{
	String decoded = "";
	char temp[] = "0x00";
	unsigned int len = text.length();
	unsigned int i = 0;
	while (i < len)
	{
		char decodedChar;
		char encodedChar = text.charAt(i++);
		if ((encodedChar == '%') && (i + 1 < len))
		{
			temp[2] = text.charAt(i++);
			temp[3] = text.charAt(i++);

			decodedChar = strtol(temp, NULL, 16);
		}
		else {
			if (encodedChar == '+')
			{
				decodedChar = ' ';
			}
			else {
				decodedChar = encodedChar;  // normal ascii char
			}
		}
		decoded += decodedChar;
	}
	return decoded;
}


bool AmebaWebServer::_parseFormUploadAborted(){
  _currentUpload.status = UPLOAD_FILE_ABORTED;
  if(_currentHandler && _currentHandler->canUpload(_currentUri))
    _currentHandler->upload(*this, _currentUri, _currentUpload);
  return false;
}
