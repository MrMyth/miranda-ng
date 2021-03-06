#include "stdafx.h"

MCONTACT CToxProto::GetChatRoom(int groupNumber)
{
	MCONTACT hContact = NULL;
	for (hContact = db_find_first(m_szModuleName); hContact; hContact = db_find_next(hContact, m_szModuleName))
	{
		if (!isChatRoom(hContact))
		{
			continue;
		}
		int chatRoumNumber = getWord(hContact, TOX_SETTINGS_CHAT_ID, TOX_ERROR);
		if (groupNumber == chatRoumNumber)
		{
			break;
		}
	}
	return hContact;
}

MCONTACT CToxProto::AddChatRoom(int groupNumber)
{
	MCONTACT hContact = GetChatRoom(groupNumber);
	if (!hContact)
	{
		hContact = db_add_contact();
		Proto_AddToContact(hContact, m_szModuleName);

		setWord(hContact, TOX_SETTINGS_CHAT_ID, groupNumber);

		wchar_t title[MAX_PATH];
		mir_snwprintf(title, L"%s #%d", TranslateT("Group chat"), groupNumber);
		setWString(hContact, "Nick", title);

		DBVARIANT dbv;
		if (!db_get_s(NULL, "Chat", "AddToGroup", &dbv, DBVT_WCHAR))
		{
			db_set_ws(hContact, "CList", "Group", dbv.ptszVal);
			db_free(&dbv);
		}

		setByte(hContact, "ChatRoom", 1);
	}
	return hContact;
}

void CToxProto::LoadChatRoomList(void*)
{
	uint32_t count = tox_count_chatlist(toxThread->Tox());
	if (count == 0)
	{
		debugLogA(__FUNCTION__": your group chat list is empty");
		return;
	}
	int32_t *groupChats = (int32_t*)mir_alloc(count * sizeof(int32_t));
	tox_get_chatlist(toxThread->Tox(), groupChats, count);
	for (uint32_t i = 0; i < count; i++)
	{
		int32_t groupNumber = groupChats[i];
		int type = tox_group_get_type(toxThread->Tox(), groupNumber);
		if (type == TOX_GROUPCHAT_TYPE_AV)
		{
			continue;
		}
		MCONTACT hContact = AddChatRoom(groupNumber);
		if (hContact)
		{
			uint8_t title[TOX_MAX_NAME_LENGTH] = { 0 };
			tox_group_get_title(toxThread->Tox(), groupNumber, title, TOX_MAX_NAME_LENGTH);
			setWString(hContact, "Nick", ptrW(mir_utf8decodeW((char*)title)));
		}
	}
	mir_free(groupChats);
}

int CToxProto::OnGroupChatEventHook(WPARAM, LPARAM lParam)
{
	GCHOOK *gch = (GCHOOK*)lParam;
	if (!gch)
	{
		return 1;
	}
	else 
		return 0;
}

int CToxProto::OnGroupChatMenuHook(WPARAM, LPARAM)
{
	return 0;
}

INT_PTR CToxProto::OnJoinChatRoom(WPARAM, LPARAM)
{
	return 0;
}

INT_PTR CToxProto::OnLeaveChatRoom(WPARAM, LPARAM)
{
	return 0;
}

INT_PTR CToxProto::OnCreateChatRoom(WPARAM, LPARAM)
{
	ChatRoomInviteParam param = { this };

	if (DialogBoxParam(
		g_hInstance,
		MAKEINTRESOURCE(IDD_CHATROOM_INVITE),
		NULL,
		CToxProto::ChatRoomInviteProc,
		(LPARAM)&param) == IDOK && !param.invitedContacts.empty())
	{
		int groupNumber = tox_add_groupchat(toxThread->Tox());
		if (groupNumber == TOX_ERROR)
		{
			return 1;
		}
		for (std::vector<MCONTACT>::iterator it = param.invitedContacts.begin(); it != param.invitedContacts.end(); ++it)
		{
			int32_t friendNumber = GetToxFriendNumber(*it);
			if (friendNumber == TOX_ERROR || tox_invite_friend(toxThread->Tox(), friendNumber, groupNumber) == TOX_ERROR)
			{
				return 1;
			}
		}
		MCONTACT hContact = AddChatRoom(groupNumber);
		if (!hContact)
		{
			return 1;
		}
		return 0;
	}

	return 1;
}

void CToxProto::InitGroupChatModule()
{
	GCREGISTER gcr = {};
	gcr.iMaxText = 0;
	gcr.ptszDispName = this->m_tszUserName;
	gcr.pszModule = this->m_szModuleName;
	Chat_Register(&gcr);

	HookProtoEvent(ME_GC_EVENT, &CToxProto::OnGroupChatEventHook);
	HookProtoEvent(ME_GC_BUILDMENU, &CToxProto::OnGroupChatMenuHook);

	CreateProtoService(PS_JOINCHAT, &CToxProto::OnJoinChatRoom);
	CreateProtoService(PS_LEAVECHAT, &CToxProto::OnLeaveChatRoom);
}

void CToxProto::CloseAllChatChatSessions()
{
	GC_INFO gci = { 0 };
	gci.Flags = GCF_BYINDEX | GCF_ID;
	gci.pszModule = m_szModuleName;

	int count = pci->SM_GetCount(m_szModuleName);
	for (int i = 0; i < count; i++)
	{
		gci.iItem = i;
		if (!Chat_GetInfo(&gci))
		{
			Chat_Control(m_szModuleName, gci.pszID, SESSION_OFFLINE);
			Chat_Terminate(m_szModuleName, gci.pszID);
		}
	}
}

void CToxProto::OnGroupChatInvite(Tox *tox, int32_t friendNumber, uint8_t type, const uint8_t *data, uint16_t length, void *arg)
{
	CToxProto *proto = (CToxProto*)arg;

	if (type == TOX_GROUPCHAT_TYPE_AV)
	{
		Netlib_Logf(proto->m_hNetlibUser, __FUNCTION__": audio chat is not supported yet");
		return;
	}

	int groupNumber = tox_join_groupchat(tox, friendNumber, data, length);
	if (groupNumber == TOX_ERROR)
	{
		Netlib_Logf(proto->m_hNetlibUser, __FUNCTION__": failed to join to group chat");
		return;
	}

	MCONTACT hContact = proto->AddChatRoom(groupNumber);
	if (!hContact)
	{
		Netlib_Logf(proto->m_hNetlibUser, __FUNCTION__": failed to create group chat");
	}
}

void CToxProto::ChatValidateContact(HWND hwndList, const std::vector<MCONTACT> &contacts, MCONTACT hContact)
{
	bool isProtoContact = mir_strcmpi(GetContactProto(hContact), m_szModuleName) == 0;
	if (isProtoContact && !isChatRoom(hContact))
	{
		if (std::find(contacts.begin(), contacts.end(), hContact) != contacts.end())
		{
			SendMessage(hwndList, CLM_DELETEITEM, (WPARAM)hContact, 0);
		}
		return;
	}
	SendMessage(hwndList, CLM_DELETEITEM, (WPARAM)hContact, 0);
}

void CToxProto::ChatPrepare(HWND hwndList, const std::vector<MCONTACT> &contacts, MCONTACT hContact)
{
	if (hContact == NULL)
	{
		hContact = (MCONTACT)SendMessage(hwndList, CLM_GETNEXTITEM, CLGN_ROOT, 0);
	}
	while (hContact)
	{
		if (IsHContactGroup(hContact))
		{
			MCONTACT hSubContact = (MCONTACT)SendMessage(hwndList, CLM_GETNEXTITEM, CLGN_CHILD, (LPARAM)hContact);
			if (hSubContact)
			{
				ChatPrepare(hwndList, contacts, hSubContact);
			}
		}
		else if (IsHContactContact(hContact))
		{
			ChatValidateContact(hwndList, contacts, hContact);
		}
		hContact = (MCONTACT)SendMessage(hwndList, CLM_GETNEXTITEM, CLGN_NEXT, (LPARAM)hContact);
	}
}

std::vector<MCONTACT> CToxProto::GetInvitedContacts(HWND hwndList, MCONTACT hContact)
{
	std::vector<MCONTACT> contacts;
	if (hContact == NULL)
	{
		hContact = (MCONTACT)SendMessage(hwndList, CLM_GETNEXTITEM, CLGN_ROOT, 0);
	}
	while (hContact)
	{
		if (IsHContactGroup(hContact))
		{
			MCONTACT hSubContact = (MCONTACT)SendMessage(hwndList, CLM_GETNEXTITEM, CLGN_CHILD, (LPARAM)hContact);
			if (hSubContact)
			{
				std::vector<MCONTACT> subContacts = GetInvitedContacts(hwndList, hSubContact);
				contacts.insert(contacts.end(), subContacts.begin(), subContacts.end());
			}
		}
		else
		{
			int cheked = SendMessage(hwndList, CLM_GETCHECKMARK, (WPARAM)hContact, 0);
			if (cheked)
			{
				contacts.push_back(hContact);
			}
		}
		hContact = (MCONTACT)SendMessage(hwndList, CLM_GETNEXTITEM, CLGN_NEXT, (LPARAM)hContact);
	}
	return contacts;
}

INT_PTR CALLBACK CToxProto::ChatRoomInviteProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	HWND hwndList = GetDlgItem(hwndDlg, IDC_CCLIST);
	ChatRoomInviteParam *param = (ChatRoomInviteParam*)GetWindowLongPtr(hwndDlg, GWLP_USERDATA);

	switch (msg) {
	case WM_INITDIALOG:
		TranslateDialogDefault(hwndDlg);
		{
			param = (ChatRoomInviteParam*)lParam;
			SetWindowLongPtr(hwndDlg, GWLP_USERDATA, lParam);
			{
				//HWND hwndClist = GetDlgItem(hwndDlg, IDC_CCLIST);
				//SetWindowLongPtr(hwndClist, GWL_STYLE, GetWindowLongPtr(hwndClist, GWL_STYLE) & ~CLS_HIDEOFFLINE);
			}
		}
		break;

	case WM_CLOSE:
		EndDialog(hwndDlg, 0);
		break;

	case WM_NOTIFY:
	{
		NMCLISTCONTROL *nmc = (NMCLISTCONTROL*)lParam;
		if (nmc->hdr.idFrom == IDC_CCLIST)
		{
			switch (nmc->hdr.code)
			{
			case CLN_NEWCONTACT:
				if ((nmc->flags & (CLNF_ISGROUP | CLNF_ISINFO)) == 0)
				{
					param->proto->ChatValidateContact(nmc->hdr.hwndFrom, param->invitedContacts, (UINT_PTR)nmc->hItem);
				}
				break;

			case CLN_LISTREBUILT:
			{
				param->proto->ChatPrepare(nmc->hdr.hwndFrom, param->invitedContacts);
			}
				break;
			}
		}
	}
	break;

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			//SetWindowLongPtr(hwndDlg, GWLP_USERDATA, lParam);
			param->invitedContacts = param->proto->GetInvitedContacts(hwndList);
			EndDialog(hwndDlg, IDOK);
			break;

		case IDCANCEL:
			EndDialog(hwndDlg, IDCANCEL);
			break;
		}
		break;
	}
	return FALSE;
}
