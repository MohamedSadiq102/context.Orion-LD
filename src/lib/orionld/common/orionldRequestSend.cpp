/*
*
* Copyright 2018 Telefonica Investigacion y Desarrollo, S.A.U
*
* This file is part of Orion Context Broker.
*
* Orion Context Broker is free software: you can redistribute it and/or
* modify it under the terms of the GNU Affero General Public License as
* published by the Free Software Foundation, either version 3 of the
* License, or (at your option) any later version.
*
* Orion Context Broker is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero
* General Public License for more details.
*
* You should have received a copy of the GNU Affero General Public License
* along with Orion Context Broker. If not, see http://www.gnu.org/licenses/.
*
* For those usages not covered by this license please contact with
* iot_support at tid dot es
*
* Author: Ken Zangelin
*/
#include <curl/curl.h>

#include "logMsg/logMsg.h"
#include "logMsg/traceLevels.h"

#include "orionld/common/urlParse.h"                      // urlParse
#include "orionld/common/orionldRequestSend.h"            // Own interface



// -----------------------------------------------------------------------------
//
// writeCallback -
//
static size_t writeCallback(void* contents, size_t size, size_t members, void* userP)
{
  size_t                  realSize = size * members;
  OrionldResponseBuffer*  rBuf     = (OrionldResponseBuffer*) userP;

  LM_T(LmtWriteCallback, ("Got a chunk of %d bytes: %s", realSize, (char*) contents));

  if (realSize + rBuf->used > rBuf->size)
  {
    rBuf->buf = (char*) realloc(rBuf->buf, rBuf->size + realSize + 1);
    if (rBuf->buf == NULL)
    {
      LM_E(("Runtime Error (out of memory)"));
      return 0;
    }
  }

  memcpy(&rBuf->buf[rBuf->used], contents, realSize);
  rBuf->used += realSize;
  rBuf->size += realSize;
  rBuf->buf[rBuf->used] = 0;

  return realSize;
}



// -----------------------------------------------------------------------------
//
// orionldRequestSend - send a request and await its response
//
bool orionldRequestSend(OrionldResponseBuffer* rBufP, const char* url, int tmoInMilliSeconds, char** detailsPP)
{
  CURLcode             cCode;
  struct curl_context  cc;

  char   protocol[16];
  char   ip[256];
  short  port    = 0;
  char*  urlPath = NULL;
  
  if (urlParse(url, protocol, sizeof(protocol), ip, sizeof(ip), &port, &urlPath, detailsPP) == false)
  {
    // urlParse sets *detailsPP
    LM_E(("urlParse failed for url '%s': %s", url, *detailsPP));
    return false;
  }

  LM_T(LmtRequestSend, ("protocol: %s", protocol));
  LM_T(LmtRequestSend, ("IP:       %s", ip));
  LM_T(LmtRequestSend, ("URL Path: %s", urlPath));

  get_curl_context(ip, &cc);
  if (cc.curl == NULL)
  {
    *detailsPP = (char*) "Unable to obtain CURL context";
    LM_E((*detailsPP));
    return false;
  }

  
  //
  // Prepare the CURL handle
  //
  curl_easy_setopt(cc.curl, CURLOPT_URL, url);                             // Set the URL Path
  curl_easy_setopt(cc.curl, CURLOPT_CUSTOMREQUEST, "GET");                 // Set the HTTP verb
  curl_easy_setopt(cc.curl, CURLOPT_FOLLOWLOCATION, 1L);                   // Allow redirection
  curl_easy_setopt(cc.curl, CURLOPT_WRITEFUNCTION, writeCallback);         // Callback function for writes
  curl_easy_setopt(cc.curl, CURLOPT_WRITEDATA, rBufP);                     // Custom data for response handling
  curl_easy_setopt(cc.curl, CURLOPT_TIMEOUT_MS, tmoInMilliSeconds);        // Timeout

#if 0
  curl_easy_setopt(cc.curl, CURLOPT_HEADER, 1);                         // Activate include the header in the body output
  curl_easy_setopt(cc.curl, CURLOPT_HTTPHEADER, headers);               // Put headers in place
#endif

  LM_T(LmtRequestSend, ("Calling curl_easy_perform for GET %s", url));
  cCode = curl_easy_perform(cc.curl);
  LM_T(LmtRequestSend, ("curl_easy_perform returned %d", cCode));
  if (cCode != CURLE_OK)
  {
    release_curl_context(&cc);
    LM_E(("curl_easy_perform error %d", cCode));
    free(rBufP->buf);
    *detailsPP = (char*) "internal error at sending of curl request";
    return false;
  }

  // The downloaded buffer is in rBufP->buf
  LM_T(LmtRequestSend, ("Got response: %s", rBufP->buf));

  release_curl_context(&cc);

  return true;
}