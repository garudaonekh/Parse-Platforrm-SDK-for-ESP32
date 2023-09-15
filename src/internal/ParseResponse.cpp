/*
 *  Copyright (c) 2015, Parse, LLC. All rights reserved.
 *
 *  You are hereby granted a non-exclusive, worldwide, royalty-free license to use,
 *  copy, modify, and distribute this software in source code or binary form for use
 *  in connection with the web services and APIs provided by Parse.
 *
 *  As with any software that integrates with the Parse platform, your use of
 *  this software is subject to the Parse Terms of Service
 *  [https://www.parse.com/about/terms]. This copyright notice shall be
 *  included in all copies or substantial portions of the software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 *  FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 *  COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 *  IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 *  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */


#include "ParseResponse.h"
#include "ParseInternal.h"

static const char kHttpOK[] = "HTTP/1.1 200 OK";
static const char kContentLength[] = "Content-Length:";
static const char kChunkedEncoding[] = "transfer-encoding: chunked";
// Fortunately we do not need to support *any* JSON, only the one generated by Parse.
static const char kResultsStart[] = "{\"results\":[";
static const int kJsonResponseMaxSize = 256;
static const int kQueryTimeout = 5000;
static const int kBufferSize = 1024;

// Uncomment following line if you want to debug query response with serial output.
// #define DEBUG_RESPONSE

ParseResponse::ParseResponse(ConnectionClient* client) {
  buf = NULL;
  tmpBuf = NULL;
  p = 0;
  resultCount = -1;
  bufSize = 0;
  isUserBuffer = false;
  isChunked = false;
  responseLength = 0;
  dataDone = false;
  this->client = client;
  bufferPos = kBufferSize;
  lastRead = -1;
}

ParseResponse::~ParseResponse() {
  close();
}

void ParseResponse::setBuffer(char* buffer, int size) {
  if (!buffer || size <= 0) {
    return;
  }

  buf = buffer;
  bufSize = size;
  isUserBuffer = true;
  memset(buf, 0, bufSize);
}

int ParseResponse::available() {
  return client->available();
}

void ParseResponse::read() {
  if (dataDone)
    return;
  if (buf == NULL) {
    bufSize = BUFSIZE;
    buf = new char[bufSize];
    memset(buf, 0, bufSize);
  }

  if (p == bufSize - 1) {
    return;
  }

  memset(buf + p, 0, bufSize - p);

  char c;
  int i;

  bool data = false;
  int count = 0;

  while (client->connected()) {
    delay(1);
    while (client->available()) {
      c = client->read();
      if (c != '\r') { // filter out '\r' character
        if (data) {
          if (p < bufSize - 1) {
            *(buf + p) = c;
            p++;
          }
        }
        // filter out the headers
        if (c == '\n') count++; else count = 0;
        if (count >= 2) data = true;
      }
    }
  }
  dataDone = 1;
}

void ParseResponse::readLine(char *buff, int sz) {
  memset(buff, 0, sz);
#ifdef DEBUG_RESPONSE
  Serial.print("Read line:");
#endif
  for (int i = 0; client->available(); ++i) {
    char c = client->read();
    if (c == '\r') {
      client->read(); // read /n
      return;
    }
    if (c == '\n') {
      return;
    }
#ifdef DEBUG_RESPONSE
    Serial.print(c);
#endif
    if (i < sz - 1)
      buff[i] = c;
  }
#ifdef DEBUG_RESPONSE
  Serial.println("");
#endif
}

bool ParseResponse::readJson(char *buff, int sz) {
  memset(buff, 0, sz);

  int read_bytes = 0;

  bool res = readJsonInternal(buff, sz, &read_bytes, '\0');
  if (!res) {
#ifdef DEBUG_RESPONSE
    Serial.print("Failed");
    Serial.println(buff);
#endif
  }
  return res;
}

bool ParseResponse::readJsonInternal(char *buff, int sz, int* read_bytes, char started) {
  // It is our own JSON, so we can be *very* strict in regars to format.
  char in_string = '\0';
  int ch;
  int i;

  char tmp[32];

  for (i = 0; ; ++i) {
    bool skip = false;
    if ((ch = readChunkedData(kQueryTimeout)) < 0) {
      *read_bytes = i;
      return false;
    }
    switch (ch) {
      case '\"':
      case '\'':
        if (in_string == ch)
          in_string = '\0';
        else if (in_string == '\0')
          in_string = ch;
        break;
      case ',':
        if (in_string)
          break;
        if (!started) {
          --i; // skip
          skip = true;
        }
        break;
      case '[':
      case '{':
      {
        if (in_string)
          break;
        if (!started) {
          started = ch;
          break;
        }
        int added = 0;
        if (i < sz)
          buff[i] = ch;
        bool success = readJsonInternal(buff + i + 1, sz - i - 1, &added, ch);
        if (!success)
          return false;
        i += added;
        skip = true;
      } break;
      case '}':
      case ']':
      {
        if (in_string)
          break;
        if (!started)
          return false;
        if ((started == '[' && ch != ']') || (started == '{' && ch != '}'))
          return false;
        if (i < sz)
          buff[i] = ch;
        *read_bytes = i + 1;
        return true;
      } break;
    }
    if (!skip && i < sz - 1)
      buff[i] = ch;
  }
}

#ifdef DEBUG_RESPONSE
void printData(char *d, int sz, int offset) {
  char t1[32];
  sprintf(t1,"\r\n%d %04x[->", sz, offset);
  Serial.print(t1);
  char tmp[4] = {0};
  for (int i = 0; i < sz; ++i) {
    if (d[i] >= 32 && d[i] <= 127)
      tmp[0] = d[i];
    else
      tmp[0] = '?';
    Serial.print(tmp);
  }
  Serial.println("<-]");
}
#endif

int ParseResponse::readChunkedData(int timeout) {

  char snum[16];

  if (bufferPos == kBufferSize || (lastRead > 0 && bufferPos == lastRead)) {
    for (int i = 0; i < timeout && !client->available(); ++i) {
        delay(1);
    }

    if (responseLength <= 0) {
      readLine(snum, sizeof(snum));
      readLine(snum, sizeof(snum));
      char *tmp;
#ifdef DEBUG_RESPONSE
      Serial.println("");
      Serial.print("Next chunk:");
      Serial.println(snum);
#endif
      responseLength = strtol(snum, &tmp, 16); 
      if (!responseLength)
        return -1;
    }
    if (client->available()) {
      int to_read = responseLength > kBufferSize ? kBufferSize : responseLength;
      bufferPos = 0;
      int sz = client->read((uint8_t *)chunkedBuffer, to_read);
      lastRead = sz;
#ifdef DEBUG_RESPONSE
      Serial.println();
      Serial.print("Read: ");
      Serial.println(sz);
      Serial.println();
      Serial.print("bufferPos: ");
      Serial.println(bufferPos);
      Serial.println();
      Serial.print("to_read: ");
      Serial.println(to_read);
      Serial.println();
      Serial.print("responseLength: ");
      Serial.println(responseLength);
      printData(chunkedBuffer, sz, bufferPos);
#endif
      if (sz <= 0)
        return -1;
      responseLength -= sz;
    }
  }
  int ch = chunkedBuffer[bufferPos++];
  return ch;
}

int ParseResponse::getErrorCode() {
  return getInt("code");
}

const char* ParseResponse::getJSONBody() {
  read();
  return buf;
}

const char* ParseResponse::getString(const char* key) {
  read();
  if (!tmpBuf) {
    tmpBuf = new char[64];
  }
  memset(tmpBuf, 0, 64);
  ParseUtils::getStringFromJSON(buf, key, tmpBuf, 64);
  return tmpBuf;
}

int ParseResponse::getInt(const char* key) {
  read();
  return ParseUtils::getIntFromJSON(buf, key);
}

double ParseResponse::getDouble(const char* key) {
  read();
  return ParseUtils::getFloatFromJSON(buf, key);
}

bool ParseResponse::getBoolean(const char* key) {
  read();
  return ParseUtils::getBooleanFromJSON(buf, key);
}

void ParseResponse::readWithTimeout(int maxSec) {
  while((!available()) && (maxSec--)) { // wait till response
    delay(1000);
  }
  read();
}

bool ParseResponse::nextObject() {
  if(resultCount <= 0) {
    count();
  }

  if(resultCount <= 0) {
    return false;
  }

  if (firstObject) {
    firstObject = false;
    return true;
  } else {
    return readJson(buf, bufSize);
  }
}

int ParseResponse::count() {
  if (resultCount != -1)
    return resultCount;
  char buff[128];

  resultCount = 0;

  bool first_line = true;
  bool ok = false;
  bool done = false;
  char *ptr;
  long len = 0;
  while (client->connected() && !done) {
    delay(1);
    while (client->available()) {
      readLine(buff, sizeof(buff));
      if (first_line) {
        if (!strcmp(kHttpOK, buff))
          ok = true;
        first_line = false;
      }
#ifdef DEBUG_RESPONSE
      Serial.print("H->");
      Serial.println(buff);
#endif
      if (!strcmp(kChunkedEncoding, buff)) {
        isChunked = true;
      } else if (!strncmp(kContentLength, buff, sizeof(kContentLength))) {
        responseLength = strtol(buff + sizeof(kContentLength), &ptr, 10);
      } else if (!buff[0]) {
        if (isChunked && client->available()) {
          readLine(buff, sizeof(buff));
          responseLength = strtol(buff, &ptr, 16);
#ifdef DEBUG_RESPONSE
          Serial.print("First chunk->");
          Serial.println(buff);
#endif
        }
        done = true;
        break;
      }
    }
  }
  long persistentResponseLength = responseLength; // responseLength is modified by calls to readChunkedData
#ifdef DEBUG_RESPONSE
  sprintf(buff, "Ok:%s Length:%d Chunked:%s", ok ? "y" : "n", responseLength, isChunked ? "y" : "n");
  Serial.println(buff);
#endif
  done = false;
  freeBuffer();
  setBuffer(new char[kJsonResponseMaxSize], kJsonResponseMaxSize);
  dataDone = true;

  for (int i = 0; i < sizeof(kResultsStart) - 1; ++i) {
    char c = readChunkedData(kQueryTimeout);
    if (c != kResultsStart[i]) {
#ifdef DEBUG_RESPONSE
      Serial.println("Malformed response!");
#endif
      resultCount = -1;
      return -1;
    }
  }

  if (!readJson(buf, bufSize)) {
#ifdef DEBUG_RESPONSE
    Serial.println("no results");
#endif
    resultCount = 0;
    return 0;
  }

  int tmplen = strlen(buf);
  firstObject = true;
  if (tmplen > 0)
    resultCount = persistentResponseLength / tmplen;
  if (isChunked)
    resultCount *= 2;
  if (!resultCount)
    resultCount = 1;

  return resultCount;
}

void ParseResponse::freeBuffer() {
  if (!isUserBuffer) { // only free non-user buffer
    delete[] buf;
    buf = NULL;
  }
  if (tmpBuf) {
    delete[] tmpBuf;
    tmpBuf = NULL;
  }
}

void ParseResponse::close() {
  freeBuffer();
}

