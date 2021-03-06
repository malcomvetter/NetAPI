#include "pch.h"
#include <string>
using std::string;
using std::wstring;
#include <vector>
using std::vector;
#include <iostream>
#include <windows.h>
#include <stdio.h>
#include <assert.h>
#include <lm.h>
#pragma comment(lib, "netapi32.lib")
#include <sddl.h> //for ConvertSidToStringSid function
#pragma comment(lib, "advapi32.lib")

class User
{
public:
	wstring name, sid, fullname, comment;
	bool accountEnabled;
	int accountExpires, passwordAge, lastLogon;
	vector<wstring> localGroups;
	vector<wstring> globalGroups;
};

class Group
{
public:
	wstring name, comment;
	vector<wstring> members;
};

User enumUserInfo(wstring username, wstring server = L"\\\\.")
{
	User _user;
	DWORD dwLevel = 4;
	LPUSER_INFO_0 pBuf = NULL;
	LPUSER_INFO_4 pBuf4 = NULL;
	NET_API_STATUS nStatus;
	LPTSTR sStringSid = NULL;

	nStatus = NetUserGetInfo(server.c_str(), username.c_str(), dwLevel, (LPBYTE *)& pBuf);

	if (nStatus == NERR_Success)
	{
		if (pBuf != NULL)
		{
			pBuf4 = (LPUSER_INFO_4)pBuf;
			_user.name = pBuf4->usri4_name;
			if (ConvertSidToStringSid(pBuf4->usri4_user_sid, &sStringSid))
			{
				_user.sid = sStringSid;
				LocalFree(sStringSid);
			}
			_user.fullname = pBuf4->usri4_full_name;
			_user.comment = pBuf4->usri4_comment;
			_user.accountEnabled = true;
			if (pBuf4->usri4_flags & UF_ACCOUNTDISABLE) 
			{
				_user.accountEnabled = false;
			}
			_user.accountExpires = pBuf4->usri4_acct_expires;
			_user.passwordAge = pBuf4->usri4_password_age;
			_user.lastLogon = pBuf4->usri4_last_logon;
		}
	}
	//cleanup:
	if (pBuf != NULL)
		NetApiBufferFree(pBuf);
	return _user;
}


vector<wstring> enumUsers(wstring server = L"\\\\.")
{
	vector<wstring> users;
	LPUSER_INFO_0 pBuf = NULL;
	LPUSER_INFO_0 pTmpBuf;
	DWORD dwLevel = 0;
	DWORD dwPrefMaxLen = MAX_PREFERRED_LENGTH;
	DWORD dwEntriesRead = 0;
	DWORD dwTotalEntries = 0;
	DWORD dwResumeHandle = 0;
	DWORD i;
	DWORD dwTotalCount = 0;
	NET_API_STATUS nStatus;
	LPCWSTR pszServerName = server.c_str();

	do
	{
		// Call the NetUserEnum function, specifying level 0 - global user account types only.
		nStatus = NetUserEnum(pszServerName,
			dwLevel,
			FILTER_NORMAL_ACCOUNT, // global users
			(LPBYTE*)&pBuf,
			dwPrefMaxLen,
			&dwEntriesRead,
			&dwTotalEntries,
			&dwResumeHandle);
		if ((nStatus == NERR_Success) || (nStatus == ERROR_MORE_DATA))
		{
			if ((pTmpBuf = pBuf) != NULL)
			{
				for (i = 0; (i < dwEntriesRead); i++)
				{
					assert(pTmpBuf != NULL);

					if (pTmpBuf == NULL) //access violation
					{
						break;
					}
					std::wstring username(pTmpBuf->usri0_name);
					users.push_back(username);
					pTmpBuf++;
					dwTotalCount++;
				}
			}
		}
		//cleanup:
		if (pBuf != NULL)
		{
			NetApiBufferFree(pBuf);
			pBuf = NULL;
		}
	} while (nStatus == ERROR_MORE_DATA);
	if (pBuf != NULL)
		NetApiBufferFree(pBuf);
	return users;
}

vector<wstring> enumUserLocalGroups(wstring user)
{
	vector<wstring> _groups;
	LPBYTE buffer;
	DWORD entries, total_entries;
	NetUserGetLocalGroups(NULL, user.c_str(), 0, LG_INCLUDE_INDIRECT, &buffer, MAX_PREFERRED_LENGTH, &entries, &total_entries);
	LOCALGROUP_USERS_INFO_0 *groups = (LOCALGROUP_USERS_INFO_0*)buffer;
	for (int i = 0; i < entries; i++)
	{
		wstring _g(groups[i].lgrui0_name);
		_groups.push_back(_g);
	}
	NetApiBufferFree(buffer);
	return _groups;
}

vector<wstring> enumUserGlobalGroups(wstring user)
{
	vector<wstring> _groups;
	LPBYTE buffer;
	DWORD entries, total_entries;
	NetUserGetGroups(NULL, user.c_str(), 0, &buffer, MAX_PREFERRED_LENGTH, &entries, &total_entries);
	GROUP_USERS_INFO_0 *ggroups = (GROUP_USERS_INFO_0*)buffer;
	for (int i = 0; i < entries; i++) {
		wstring _g(ggroups[i].grui0_name);
		if (_g != L"None") _groups.push_back(_g);
	}
	NetApiBufferFree(buffer);
	return _groups;
}

bool createUser(wstring username, wstring password, wstring server = L"\\\\.")
{
	USER_INFO_1 ui;
	DWORD dwLevel = 1;
	DWORD dwError = 0;
	NET_API_STATUS nStatus;

	ui.usri1_name = (LPWSTR) username.c_str();
	ui.usri1_password = (LPWSTR) password.c_str();
	ui.usri1_priv = USER_PRIV_USER;
	ui.usri1_home_dir = NULL;
	ui.usri1_comment = NULL;
	ui.usri1_flags = UF_SCRIPT;
	ui.usri1_script_path = NULL;
	nStatus = NetUserAdd(server.c_str(), dwLevel, (LPBYTE)&ui, &dwError);
	return (nStatus == NERR_Success);
}

PSID getUserSid(wstring username, wstring server = L"\\\\.")
{
	User _user;
	DWORD dwLevel = 4;
	LPUSER_INFO_0 pBuf = NULL;
	LPUSER_INFO_4 pBuf4 = NULL;
	NET_API_STATUS nStatus;
	PSID sid = NULL;

	nStatus = NetUserGetInfo(server.c_str(), username.c_str(), dwLevel, (LPBYTE *)& pBuf);
	if (nStatus == NERR_Success)
	{
		if (pBuf != NULL)
		{
			pBuf4 = (LPUSER_INFO_4)pBuf;
			sid = pBuf4->usri4_user_sid;
		}
	}
	//cleanup:
	if (pBuf != NULL)
		NetApiBufferFree(pBuf);
	return sid;
}

bool changePassword(wstring username, wstring oldPassword, wstring newPassword, wstring server = L"\\\\.")
{
	NET_API_STATUS nStatus;
	nStatus = NetUserChangePassword(server.c_str(), username.c_str(), oldPassword.c_str(), newPassword.c_str());
	return (nStatus == NERR_Success);
}

bool deleteUser(wstring username, wstring server = L"\\\\.")
{
	NET_API_STATUS nStatus;
	nStatus = NetUserDel(server.c_str(), username.c_str());
	return (nStatus == NERR_Success);
}

bool addUserToGroup(wstring username, wstring groupname, wstring server = L"\\\\.")
{
	NET_API_STATUS nStatus;
	auto sid = getUserSid(username, server);
	nStatus = NetLocalGroupAddMember(server.c_str(), groupname.c_str(), sid);
	if (nStatus == NERR_Success)
	{
		return true;
	}
	return false;
}

bool removeUserFromGroup(wstring username, wstring groupname, wstring server = L"\\\\.")
{
	NET_API_STATUS nStatus;
	auto sid = getUserSid(username, server);
	nStatus = NetLocalGroupDelMember(server.c_str(), groupname.c_str(), sid);
	if (nStatus == NERR_Success)
	{
		return true;
	}
	return false;
}

void displayUser(User user)
{
	std::wcout << L"Name: " << user.name << std::endl;
	std::wcout << L"SID: " << user.sid << std::endl;
	std::wcout << L"Full Name: " << user.fullname << std::endl;
	std::wcout << L"Comment: " << user.comment << std::endl;
	std::wcout << L"Account Enabled: " << user.accountEnabled << std::endl;
	std::wcout << L"Account Expires: " << user.accountExpires << std::endl;
	std::wcout << L"Password Age: " << user.passwordAge << std::endl;
	std::wcout << L"Last Logon: " << user.lastLogon << std::endl;
	for (auto group : user.localGroups)
	{
		std::wcout << L"Local Group: " << group << std::endl;
	}
	for (auto group : user.globalGroups)
	{
		std::wcout << L"Global Group: " << group << L"" << std::endl;
	}
	std::wcout << L"--------------------------------" << std::endl;
}


void displayGroup(Group group)
{
	std::wcout << L"Name: " << group.name << std::endl;
	std::wcout << L"Comment: " << group.comment << std::endl;
	for (auto member : group.members)
	{
		std::wcout << L"Member: " << member << std::endl;
	}
	std::wcout << L"--------------------------------" << std::endl;
}


Group getMembers(Group group, wstring server = L"\\\\.")
{
	DWORD dwLevel = 2;
	DWORD dwRead, dwTotal;
	LOCALGROUP_MEMBERS_INFO_1 *members;
	NET_API_STATUS nStatus;

	nStatus = NetLocalGroupGetMembers(
		server.c_str(), group.name.c_str(), dwLevel, 
		(LPBYTE *)& members, MAX_PREFERRED_LENGTH, &dwRead, &dwTotal, NULL);

	if (nStatus == NERR_Success)
	{
		for (int i = 0; i < dwRead; i++)
		{
			group.members.push_back(members[i].lgrmi1_name);
		}
		NetApiBufferFree(members);
	}
	return group;
}


vector<Group> enumGroups(wstring server = L"\\\\.")
{
	vector<Group> groups;
	LPGROUP_INFO_1 pBuf = NULL;
	LPGROUP_INFO_1 pTmpBuf;
	DWORD dwLevel = 1;
	DWORD dwPrefMaxLen = MAX_PREFERRED_LENGTH;
	DWORD dwEntriesRead = 0;
	DWORD dwTotalEntries = 0;
	PDWORD_PTR dwResumeHandle = 0;
	DWORD i;
	DWORD dwTotalCount = 0;
	NET_API_STATUS nStatus;
	LPCWSTR pszServerName = server.c_str();

	do
	{
		// Call the NetUserEnum function, specifying level 0 - global user account types only.
		nStatus = NetLocalGroupEnum(pszServerName,
			dwLevel,
			(LPBYTE*)&pBuf,
			dwPrefMaxLen,
			&dwEntriesRead,
			&dwTotalEntries,
			dwResumeHandle);
		if ((nStatus == NERR_Success) || (nStatus == ERROR_MORE_DATA))
		{
			if ((pTmpBuf = pBuf) != NULL)
			{
				for (i = 0; (i < dwEntriesRead); i++)
				{
					assert(pTmpBuf != NULL);

					if (pTmpBuf == NULL) //access violation
					{
						break;
					}
					Group _group;
					_group.name = pTmpBuf->grpi1_name;
					_group.comment = pTmpBuf->grpi1_comment;
					groups.push_back(_group);
					pTmpBuf++;
					dwTotalCount++;
				}
			}
		}
		//cleanup:
		if (pBuf != NULL)
		{
			NetApiBufferFree(pBuf);
			pBuf = NULL;
		}
	} while (nStatus == ERROR_MORE_DATA);
	if (pBuf != NULL)
		NetApiBufferFree(pBuf);
	return groups;
}


int main() {
	auto groups = enumGroups();
	for (auto group : groups)
	{
		group = getMembers(group);
		displayGroup(group);
	}
	auto users = enumUsers();
	for (auto user : users)
	{
		auto u = enumUserInfo(user);
		u.localGroups = enumUserLocalGroups(user);
		u.globalGroups = enumUserGlobalGroups(user);
		displayUser(u);
	}
	auto user1 = L"test";
	auto password1 = L"th!s!s$trongEnouf";
	auto password2 = L"ThsiZ$trawnguh";

	if (createUser(user1, password1))
	{
		std::wcout << L"Account Created." << std::endl;
	}
	else {
		std::wcout << L"Could not create account, check permissions." << std::endl;
	}
	if (addUserToGroup(user1, L"Administrators"))
	{
		std::wcout << L"Added to administrators" << std::endl;
	}
	if (changePassword(user1, password1, password2)) {
		std::wcout << "Changed Password." << std::endl;
	}
	if (removeUserFromGroup(user1, L"Administrators"))
	{
		std::wcout << L"Removed from the Administrators group." << std::endl;
	}
	if (deleteUser(user1)) {
		std::wcout << "Deleted user." << std::endl;
	}

	return 0;
}