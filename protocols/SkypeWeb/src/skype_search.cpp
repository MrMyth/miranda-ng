/*
Copyright (c) 2015 Miranda NG project (http://miranda-ng.org)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation version 2
of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "common.h"

HANDLE CSkypeProto::SearchBasic(const PROTOCHAR* id)
{
	ForkThread(&CSkypeProto::SearchBasicThread, (void *)id);
	return (HANDLE)1;
}

void CSkypeProto::SearchBasicThread(void* id)
{
	debugLogA("CSkypeProto::OnSearchBasicThread");
	if (!IsOnline())
		return;
	ptrA token(getStringA("TokenSecret"));
	//need to convert search string
	char *string = mir_t2a((TCHAR*)id);
	PushRequest(new GetSearchRequest(token, (char*)string), &CSkypeProto::OnSearch);
}

void CSkypeProto::OnSearch(const NETLIBHTTPREQUEST *response)
{
	if (response == NULL)
		return;

	JSONROOT root(response->pData);
	if (root == NULL)
		return;

	/*PROTOSEARCHBYNAME *pParam = (PROTOSEARCHBYNAME *)pReq->pUserInfo;
	debugLogA("CSkypeProto::OnSearch %d", reply->resultCode);
	if (reply->resultCode != 200) {
		if (pParam) {
			mir_free(pParam->pszFirstName);
			mir_free(pParam->pszLastName);
			mir_free(pParam->pszNick);
			delete pParam;
		}
		ProtoBroadcastAck(0, ACKTYPE_SEARCH, ACKRESULT_SUCCESS, (HANDLE)1, 0);
		return;
	}

	JSONROOT pRoot;
	JSONNODE *pResponse = CheckJsonResponse(pReq, reply, pRoot);
	if (pResponse == NULL) {
		if (pParam) {
			mir_free(pParam->pszFirstName);
			mir_free(pParam->pszLastName);
			mir_free(pParam->pszNick);
			delete pParam;
		}
		ProtoBroadcastAck(0, ACKTYPE_SEARCH, ACKRESULT_SUCCESS, (HANDLE)1, 0);
		return;
	}

	int iCount = json_as_int(json_get(pResponse, "count"));
	JSONNODE *pItems = json_get(pResponse, "items");
	if (!pItems) {
		pItems = pResponse;
		iCount = 1;
	} 

	for (int i = 0; i<iCount; i++) {
		JSONNODE *pRecord = json_at(pItems, i);
		if (pRecord == NULL)
			break;
		
		PROTOSEARCHRESULT psr = { sizeof(psr) };
		psr.flags = PSR_TCHAR;

		ptrT pId(json_as_string(json_get(pRecord, "id")));
		ptrT pFirstName(json_as_string(json_get(pRecord, "first_name")));
		ptrT pLastName(json_as_string(json_get(pRecord, "last_name")));
		ptrT pNick(json_as_string(json_get(pRecord, "nickname")));
		ptrT pDomain(json_as_string(json_get(pRecord, "domain")));

		psr.id = mir_wstrdup(pId);
		psr.firstName = mir_wstrdup(pFirstName);
		psr.lastName = mir_wstrdup(pLastName);
		psr.nick = mir_wstrdup(pNick);
		if (!psr.nick || !psr.nick[0])
			psr.nick = mir_wstrdup(pDomain);
		
		bool filter = true;
		if (pParam) {
			if (psr.firstName&&pParam->pszFirstName)
				filter = tlstrstr(psr.firstName, pParam->pszFirstName) && filter;
			if (psr.lastName&&pParam->pszLastName)
				filter = tlstrstr(psr.lastName, pParam->pszLastName) && filter;
			if (psr.nick&&pParam->pszNick)
				filter = tlstrstr(psr.nick, pParam->pszNick) && filter;
		}

		if (filter)
			ProtoBroadcastAck(0, ACKTYPE_SEARCH, ACKRESULT_DATA, (HANDLE)1, (LPARAM)&psr);
	}

	ProtoBroadcastAck(0, ACKTYPE_SEARCH, ACKRESULT_SUCCESS, (HANDLE)1, 0);
	if (pParam) {
		mir_free(pParam->pszFirstName);
		mir_free(pParam->pszLastName);
		mir_free(pParam->pszNick);
		delete pParam;
	}*/
}