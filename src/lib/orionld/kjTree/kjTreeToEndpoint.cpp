/*
*
* Copyright 2019 Telefonica Investigacion y Desarrollo, S.A.U
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
#include <string>                                              // std::string
#include <vector>                                              // std::vector

extern "C"
{
#include "kjson/KjNode.h"                                      // KjNode
}

#include "rest/ConnectionInfo.h"                               // ConnectionInfo
#include "apiTypesV2/HttpInfo.h"                               // HttpInfo

#include "orionld/common/CHECK.h"                              // CHECKx()
#include "orionld/common/SCOMPARE.h"                           // SCOMPAREx
#include "orionld/common/orionldState.h"                       // orionldState
#include "orionld/common/orionldErrorResponse.h"               // orionldErrorResponseCreate
#include "orionld/common/urlCheck.h"                           // urlCheck
#include "orionld/common/urnCheck.h"                           // urnCheck
#include "orionld/kjTree/kjTreeToEndpoint.h"                   // Own interface



// -----------------------------------------------------------------------------
//
// kjTreeToEndpoint -
//
bool kjTreeToEndpoint(ConnectionInfo* ciP, KjNode* kNodeP, ngsiv2::HttpInfo* httpInfoP)
{
  char* uriP    = NULL;
  char* acceptP = NULL;
  char* details;

  // Set default values
  httpInfoP->mimeType = JSON;

  for (KjNode* itemP = kNodeP->value.firstChildP; itemP != NULL; itemP = itemP->next)
  {
    if (SCOMPARE4(itemP->name, 'u', 'r', 'i', 0))
    {
      DUPLICATE_CHECK(uriP, "Endpoint::uri", itemP->value.s);
      STRING_CHECK(itemP, "Endpoint::uri");

      if (!urlCheck(uriP, &details) && !urnCheck(uriP, &details))
      {
        orionldErrorResponseCreate(ciP, OrionldBadRequestData, "Invalid Endpoint::uri", "Not a URL nor a URN", OrionldDetailsString);
        return false;
      }

      httpInfoP->url = uriP;
    }
    else if (SCOMPARE7(itemP->name, 'a', 'c', 'c', 'e', 'p', 't', 0))
    {
      DUPLICATE_CHECK(acceptP, "Endpoint::accept", itemP->value.s);
      STRING_CHECK(itemP, "Endpoint::accept");
      char* mimeType = itemP->value.s;

      if (!SCOMPARE12(mimeType, 'a', 'p', 'p', 'l', 'i', 'c', 'a', 't', 'i', 'o', 'n', '/'))
      {
        orionldErrorResponseCreate(ciP, OrionldBadRequestData, "Invalid Endpoint::accept value", mimeType, OrionldDetailsString);
        return false;
      }

      // Already accepted the first 12 chars (application/), step over them
      mimeType = &mimeType[12];
      if (SCOMPARE5(mimeType, 'j', 's', 'o', 'n', 0))
        httpInfoP->mimeType = JSON;
      else if (SCOMPARE8(mimeType, 'l', 'd', '+', 'j', 's', 'o', 'n', 0))
        httpInfoP->mimeType = JSONLD;
      else
      {
        orionldErrorResponseCreate(ciP, OrionldBadRequestData, "Invalid Endpoint::accept value", itemP->value.s, OrionldDetailsString);
        return false;
      }
    }
    else
    {
      orionldErrorResponseCreate(ciP, OrionldBadRequestData, "Unrecognized field in Endpoint", itemP->name, OrionldDetailsString);
      return false;
    }
  }

  if (uriP == NULL)
  {
    orionldErrorResponseCreate(ciP, OrionldBadRequestData, "Mandatory field missing", "Endpoint::uri", OrionldDetailsString);
    return false;
  }

  return true;
}