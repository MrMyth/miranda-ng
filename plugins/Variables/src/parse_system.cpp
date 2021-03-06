/*
    Variables Plugin for Miranda-IM (www.miranda-im.org)
    Copyright 2003-2006 P. Boon

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "stdafx.h"

static wchar_t *parseComputerName(ARGUMENTSINFO *ai)
{
	if (ai->argc != 1)
		return NULL;

	ai->flags |= AIF_DONTPARSE;
	DWORD len = MAX_COMPUTERNAME_LENGTH;
	wchar_t *res = (wchar_t*)mir_calloc((len + 1) * sizeof(wchar_t));
	if (res == NULL)
		return NULL;

	if (!GetComputerName(res, &len)) {
		mir_free(res);
		return NULL;
	}
	return res;
}

#include <pdh.h>
#include <pdhmsg.h>

static wchar_t *parseCpuLoad(ARGUMENTSINFO *ai)
{

	HQUERY hQuery;
	PDH_FMT_COUNTERVALUE cValue;
	HCOUNTER hCounter;
	wchar_t *szCounter, szVal[32];

	if (ai->argc != 2)
		return NULL;

	if (mir_wstrlen(ai->targv[1]) == 0)
		szCounter = mir_wstrdup(L"\\Processor(_Total)\\% Processor Time");
	else {
		int size = (int)mir_wstrlen(ai->targv[1]) + 32;
		szCounter = (wchar_t *)mir_alloc(size * sizeof(wchar_t));
		if (szCounter == NULL)
			return NULL;

		mir_snwprintf(szCounter, size, L"\\Process(%s)\\%% Processor Time", ai->targv[1]);
	}
	PDH_STATUS pdhStatus = PdhValidatePath(szCounter);
	if (pdhStatus != ERROR_SUCCESS) {
		mir_free(szCounter);
		return NULL;
	}
	pdhStatus = PdhOpenQuery(NULL, 0, &hQuery);
	if (pdhStatus != ERROR_SUCCESS) {
		mir_free(szCounter);
		return NULL;
	}
	pdhStatus = PdhAddCounter(hQuery, szCounter, 0, &hCounter);
	if (pdhStatus != ERROR_SUCCESS) {
		mir_free(szCounter);
		pdhStatus = PdhCloseQuery(hQuery);
		return NULL;
	}
	pdhStatus = PdhCollectQueryData(hQuery);
	if (pdhStatus != ERROR_SUCCESS) {
		mir_free(szCounter);
		PdhRemoveCounter(hCounter);
		pdhStatus = PdhCloseQuery(hQuery);
		return NULL;
	}
	Sleep(100);
	pdhStatus = PdhCollectQueryData(hQuery);
	if (pdhStatus != ERROR_SUCCESS) {
		mir_free(szCounter);
		PdhRemoveCounter(hCounter);
		pdhStatus = PdhCloseQuery(hQuery);
		return NULL;
	}
	pdhStatus = PdhGetFormattedCounterValue(hCounter, PDH_FMT_DOUBLE, (LPDWORD)NULL, &cValue);
	if (pdhStatus != ERROR_SUCCESS) {
		mir_free(szCounter);
		PdhRemoveCounter(hCounter);
		pdhStatus = PdhCloseQuery(hQuery);
		return NULL;
	}
	if (cValue.CStatus != ERROR_SUCCESS) {
		mir_free(szCounter);
		PdhRemoveCounter(hCounter);
		pdhStatus = PdhCloseQuery(hQuery);
		return NULL;
	}
	mir_snwprintf(szVal, L"%.0f", cValue.doubleValue);
	//PdhRemoveCounter(*hCounter);
	PdhCloseQuery(hQuery);
	mir_free(szCounter);

	return mir_wstrdup(szVal);
}

static wchar_t *parseCurrentDate(ARGUMENTSINFO *ai)
{
	wchar_t *szFormat;
	if (ai->argc == 1 || (ai->argc > 1 && mir_wstrlen(ai->targv[1]) == 0))
		szFormat = NULL;
	else
		szFormat = ai->targv[1];

	int len = GetDateFormat(LOCALE_USER_DEFAULT, 0, NULL, szFormat, NULL, 0);
	wchar_t *res = (wchar_t*)mir_alloc((len + 1)*sizeof(wchar_t));
	if (res == NULL)
		return NULL;

	if (GetDateFormat(LOCALE_USER_DEFAULT, 0, NULL, szFormat, res, len) == 0) {
		mir_free(res);
		return NULL;
	}

	return res;
}

static wchar_t *parseCurrentTime(ARGUMENTSINFO *ai)
{
	wchar_t *szFormat;
	if (ai->argc == 1 || (ai->argc > 1) && (mir_wstrlen(ai->targv[1]) == 0))
		szFormat = NULL;
	else
		szFormat = ai->targv[1];

	int len = GetTimeFormat(LOCALE_USER_DEFAULT, 0, NULL, szFormat, NULL, 0);
	wchar_t *res = (wchar_t*)mir_alloc((len + 1)*sizeof(wchar_t));
	if (res == NULL)
		return NULL;

	if (GetTimeFormat(LOCALE_USER_DEFAULT, 0, NULL, szFormat, res, len) == 0) {
		mir_free(res);
		return NULL;
	}

	return res;
}

static wchar_t *parseDirectory(ARGUMENTSINFO *ai)
{
	if (ai->argc < 2 || ai->argc > 3)
		return NULL;

	int depth = 0;
	if (ai->argc == 3)
		depth = ttoi(ai->targv[2]);

	if (depth <= 0)
		return mir_wstrdup(ai->targv[1]);

	size_t bi, ei;
	for (ei = 0; ei < mir_wstrlen(ai->targv[1]); ei++) {
		if (ai->targv[1][ei] == '\\')
			depth--;
		if (!depth)
			break;
	}
	if (ei >= mir_wstrlen(ai->targv[1]))
		return ai->targv[1];

	for (bi = ei - 1; bi > 0; bi--)
	if (ai->targv[1][bi - 1] == '\\')
		break;

	wchar_t *res = (wchar_t*)mir_alloc((ei - bi + 1) * sizeof(wchar_t));
	wcsncpy(res, ai->targv[1] + bi, ei - bi);
	res[ei - bi] = 0;
	return res;
}

/*
	path, depth
	returns complete path up to "path depth - depth"
	*/
static wchar_t *parseDirectory2(ARGUMENTSINFO *ai)
{
	if (ai->argc < 2 || ai->argc > 3)
		return NULL;

	int depth = 1;
	if (ai->argc == 3)
		depth = ttoi(ai->targv[2]);

	if (depth <= 0)
		return NULL;

	wchar_t *ecur = ai->targv[1] + mir_wstrlen(ai->targv[1]);
	while (depth > 0) {
		while ((*ecur != '\\') && (ecur > ai->targv[1]))
			ecur--;

		if (*ecur != '\\')
			return NULL;

		depth -= 1;
		ecur--;
	}
	wchar_t *res = (wchar_t*)mir_calloc((ecur - ai->targv[1] + 2) * sizeof(wchar_t));
	if (res == NULL)
		return NULL;

	wcsncpy(res, ai->targv[1], (ecur - ai->targv[1]) + 1);
	return res;
}

static int getTime(wchar_t *szTime, struct tm *date)
{
	// do some extra checks here
	wchar_t *cur = szTime;
	if (cur >= szTime + mir_wstrlen(szTime))
		return -1;

	date->tm_mon = wcstoul(cur, &cur, 10) - 1;
	cur++;
	if (cur >= szTime + mir_wstrlen(szTime))
		return -1;

	date->tm_mday = wcstoul(cur, &cur, 10);
	cur++;
	if (cur >= szTime + mir_wstrlen(szTime))
		return -1;

	date->tm_year = wcstoul(cur, &cur, 10);
	if (date->tm_year > 2000)
		date->tm_year -= 2000;
	else if (date->tm_year > 1900)
		date->tm_year -= 1900;

	date->tm_year = date->tm_year < 38 ? date->tm_year + 100 : date->tm_year;
	cur++;
	if (cur >= szTime + mir_wstrlen(szTime))
		return -1;

	date->tm_hour = wcstoul(cur, &cur, 10);
	cur++;
	if (cur >= szTime + mir_wstrlen(szTime))
		return -1;

	date->tm_min = wcstoul(cur, &cur, 10);
	cur++;
	if (cur >= szTime + mir_wstrlen(szTime))
		return -1;

	date->tm_sec = wcstoul(cur, &cur, 10);
	return 0;
}

static wchar_t *parseDiffTime(ARGUMENTSINFO *ai)
{
	struct tm t0, t1;
	wchar_t szTime[32];
	double diff;

	if (ai->argc != 3)
		return NULL;

	memset(&t0, 0, sizeof(t0));
	memset(&t1, 0, sizeof(t1));
	if (getTime(ai->targv[1], &t0) != 0)
		return NULL;

	if (getTime(ai->targv[2], &t1) != 0)
		return NULL;

	diff = difftime(mktime(&t1), mktime(&t0));
	mir_snwprintf(szTime, L"%.0f", diff);

	return mir_wstrdup(szTime);
}

static wchar_t *parseDirExists(ARGUMENTSINFO *ai)
{
	if (ai->argc != 2)
		return NULL;

	HANDLE hFile = CreateFile(ai->targv[1], GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		ai->flags |= AIF_FALSE;
	else
		CloseHandle(hFile);

	return mir_wstrdup(L"");
}

static wchar_t *parseEnvironmentVariable(ARGUMENTSINFO *ai)
{
	if (ai->argc != 2)
		return NULL;

	DWORD len = ExpandEnvironmentStrings(ai->targv[1], NULL, 0);
	if (len <= 0)
		return NULL;

	wchar_t *res = (wchar_t*)mir_alloc((len + 1)*sizeof(wchar_t));
	if (res == NULL)
		return NULL;

	memset(res, 0, ((len + 1) * sizeof(wchar_t)));
	if (ExpandEnvironmentStrings(ai->targv[1], res, len) == 0) {
		mir_free(res);
		return NULL;
	}
	return res;
}

static wchar_t *parseFileExists(ARGUMENTSINFO *ai)
{
	if (ai->argc != 2)
		return NULL;

	HANDLE hFile = CreateFile(ai->targv[1], GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		ai->flags |= AIF_FALSE;
	else
		CloseHandle(hFile);

	return mir_wstrdup(L"");
}

static wchar_t *parseFindWindow(ARGUMENTSINFO *ai)
{
	if (ai->argc != 2)
		return NULL;

	HWND hWin = FindWindow(ai->targv[1], NULL);
	if (hWin == NULL)
		return NULL;

	int len = GetWindowTextLength(hWin);
	if (len == 0)
		return NULL;

	wchar_t *res = (wchar_t*)mir_alloc((len + 1)*sizeof(wchar_t));
	memset(res, 0, ((len + 1) * sizeof(wchar_t)));
	GetWindowText(hWin, res, len + 1);
	return res;
}

// 1 = dir
// 2 = filter
// 3 = sperator
// 4 = [f]iles, [d]irs
static wchar_t *parseListDir(ARGUMENTSINFO *ai)
{
	if (ai->argc < 2)
		return NULL;

	wchar_t tszFirst[MAX_PATH], *tszRes, *tszSeperator, *tszFilter;
	tszFirst[0] = 0;
	tszSeperator = L"\r\n";
	tszFilter = L"*";
	tszRes = NULL;

	if (ai->argc > 1)
		wcsncpy(tszFirst, ai->targv[1], _countof(tszFirst) - 1);

	if (ai->argc > 2)
		tszFilter = ai->targv[2];

	if (ai->argc > 3)
		tszSeperator = ai->targv[3];

	BOOL bFiles = TRUE, bDirs = TRUE;
	if (ai->argc > 4) {
		if (*ai->targv[4] == 'f')
			bDirs = FALSE;
		if (*ai->targv[4] == 'd')
			bFiles = FALSE;
	}
	if (tszFirst[mir_wstrlen(tszFirst) - 1] == '\\')
		mir_wstrncat(tszFirst, tszFilter, _countof(tszFirst) - mir_wstrlen(tszFirst) - 1);
	else {
		mir_wstrncat(tszFirst, L"\\", _countof(tszFirst) - mir_wstrlen(tszFirst) - 1);
		mir_wstrncat(tszFirst, tszFilter, _countof(tszFirst) - mir_wstrlen(tszFirst) - 1);
	}

	WIN32_FIND_DATA ffd;
	HANDLE hFind = FindFirstFile(tszFirst, &ffd);
	if (hFind == INVALID_HANDLE_VALUE) {
		return NULL;
	}
	if (((ffd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) && (bDirs)) || ((!(ffd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)) && (bFiles))) {
		tszRes = (wchar_t*)mir_alloc((mir_wstrlen(ffd.cFileName) + mir_wstrlen(tszSeperator) + 1)*sizeof(wchar_t));
		mir_wstrcpy(tszRes, ffd.cFileName);
	}
	while (FindNextFile(hFind, &ffd) != 0) {
		if (((ffd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) && (bDirs)) || ((!(ffd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)) && (bFiles))) {
			if (tszRes != NULL) {
				mir_wstrcat(tszRes, tszSeperator);
				tszRes = (wchar_t*)mir_realloc(tszRes, (mir_wstrlen(tszRes) + mir_wstrlen(ffd.cFileName) + mir_wstrlen(tszSeperator) + 1)*sizeof(wchar_t));
			}
			else {
				tszRes = (wchar_t*)mir_alloc((mir_wstrlen(ffd.cFileName) + mir_wstrlen(tszSeperator) + 1)*sizeof(wchar_t));
				mir_wstrcpy(tszRes, L"");
			}
			mir_wstrcat(tszRes, ffd.cFileName);
		}
	}
	FindClose(hFind);

	return tszRes;
}

#ifndef WINE
static BOOL CALLBACK MyProcessEnumerator(DWORD, WORD, char *szProcess, LPARAM lParam)
{
	char **szProc = (char **)lParam;
	if ((*szProc != NULL) && (!_stricmp(*szProc, szProcess)))
		*szProc = NULL;

	return TRUE;
}

static wchar_t *parseProcessRunning(ARGUMENTSINFO *ai)
{
	if (ai->argc != 2)
		return NULL;

	char *szProc, *ref;
	szProc = ref = mir_u2a(ai->targv[1]);

	EnumProcs(MyProcessEnumerator, (LPARAM)&szProc);
	if (szProc != NULL)
		ai->flags |= AIF_FALSE;

	mir_free(ref);
	return mir_wstrdup(L"");
}
#endif

static wchar_t *parseRegistryValue(ARGUMENTSINFO *ai)
{
	if (ai->argc != 3)
		return NULL;

	DWORD len, type;

	wchar_t *key = mir_wstrdup(ai->targv[1]);
	if (key == NULL)
		return NULL;

	wchar_t *cur = wcschr(key, '\\');
	if (cur == NULL) {
		mir_free(key);
		return NULL;
	}

	*cur = 0;
	HKEY hKey;
	if (!mir_wstrcmp(key, L"HKEY_CLASSES_ROOT"))
		hKey = HKEY_CLASSES_ROOT;
	else if (!mir_wstrcmp(key, L"HKEY_CURRENT_USER"))
		hKey = HKEY_CURRENT_USER;
	else if (!mir_wstrcmp(key, L"HKEY_LOCAL_MACHINE"))
		hKey = HKEY_LOCAL_MACHINE;
	else if (!mir_wstrcmp(key, L"HKEY_USERS"))
		hKey = HKEY_USERS;
	else {
		mir_free(key);
		return NULL;
	}
	wchar_t *subKey = cur + 1;
	if (RegOpenKeyEx(hKey, subKey, 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
		mir_free(key);
		return NULL;
	}
	mir_free(key);
	len = MAX_REGVALUE_LENGTH + 1;
	wchar_t *res = (wchar_t*)mir_alloc(len*sizeof(wchar_t));
	if (res == NULL)
		return NULL;

	memset(res, 0, (len * sizeof(wchar_t)));
	int err = RegQueryValueEx(hKey, ai->targv[2], NULL, &type, (BYTE*)res, &len);
	if ((err != ERROR_SUCCESS) || (type != REG_SZ)) {
		RegCloseKey(hKey);
		mir_free(res);
		return NULL;
	}
	RegCloseKey(hKey);

	return res;
}

static int TsToSystemTime(SYSTEMTIME *sysTime, time_t timestamp)
{
	struct tm *pTime = localtime(&timestamp);
	if (pTime == NULL)
		return -1;

	memset(sysTime, 0, sizeof(SYSTEMTIME));
	sysTime->wDay = pTime->tm_mday;
	sysTime->wDayOfWeek = pTime->tm_wday;
	sysTime->wHour = pTime->tm_hour;
	sysTime->wMinute = pTime->tm_min;
	sysTime->wMonth = pTime->tm_mon + 1;
	sysTime->wSecond = pTime->tm_sec;
	sysTime->wYear = pTime->tm_year + 1900;

	return 0;
}

static wchar_t *parseTimestamp2Date(ARGUMENTSINFO *ai)
{
	if (ai->argc <= 1)
		return NULL;

	SYSTEMTIME sysTime;
	wchar_t *szFormat;
	time_t timestamp = ttoi(ai->targv[1]);
	if (timestamp == 0)
		return NULL;

	if ((ai->argc == 2) || ((ai->argc > 2) && (mir_wstrlen(ai->targv[2]) == 0)))
		szFormat = NULL;
	else
		szFormat = ai->targv[2];

	if (TsToSystemTime(&sysTime, timestamp) != 0)
		return NULL;

	int len = GetDateFormat(LOCALE_USER_DEFAULT, 0, &sysTime, szFormat, NULL, 0);
	wchar_t *res = (wchar_t*)mir_calloc((len + 1) * sizeof(wchar_t));
	if (res == NULL)
		return NULL;

	if (GetDateFormat(LOCALE_USER_DEFAULT, 0, &sysTime, szFormat, res, len) == 0) {
		mir_free(res);
		return NULL;
	}

	return res;
}

static wchar_t *parseTimestamp2Time(ARGUMENTSINFO *ai)
{
	if (ai->argc <= 1)
		return NULL;

	SYSTEMTIME sysTime;
	time_t timestamp = ttoi(ai->targv[1]);
	if (timestamp == 0)
		return NULL;

	wchar_t *szFormat;
	if ((ai->argc == 2) || ((ai->argc > 2) && (mir_wstrlen(ai->targv[2]) == 0)))
		szFormat = NULL;
	else
		szFormat = ai->targv[2];

	if (TsToSystemTime(&sysTime, timestamp) != 0)
		return NULL;

	int len = GetTimeFormat(LOCALE_USER_DEFAULT, 0, &sysTime, szFormat, NULL, 0);
	wchar_t *res = (wchar_t*)mir_alloc((len + 1)*sizeof(wchar_t));
	if (res == NULL)
		return NULL;

	if (GetTimeFormat(LOCALE_USER_DEFAULT, 0, &sysTime, szFormat, res, len) == 0) {
		mir_free(res);
		return NULL;
	}

	return res;
}

static wchar_t *parseTextFile(ARGUMENTSINFO *ai)
{
	if (ai->argc != 3)
		return NULL;

	HANDLE hFile = CreateFile(ai->targv[1], GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return NULL;

	DWORD fileSz = GetFileSize(hFile, NULL);
	if (fileSz == INVALID_FILE_SIZE) {
		CloseHandle(hFile);
		return NULL;
	}

	int lineNo = ttoi(ai->targv[2]);
	int lineCount, csz;
	unsigned int icur, bufSz;
	DWORD readSz, totalReadSz;
	unsigned long linePos;
	wchar_t tUC, *res;
	BYTE *pBuf, *pCur;
	ReadFile(hFile, &tUC, sizeof(wchar_t), &readSz, NULL);
	if (tUC != (wchar_t)0xFEFF) {
		tUC = 0;
		csz = sizeof(char);
		SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
	}
	else csz = sizeof(wchar_t);

	totalReadSz = 0;
	lineCount = 1;
	if (*ai->targv[2] == '0') {
		// complete file
		bufSz = fileSz + csz;
		pBuf = (PBYTE)mir_calloc(bufSz);
		if (pBuf == NULL) {
			CloseHandle(hFile);
			return NULL;
		}

		if (ReadFile(hFile, pBuf, bufSz - csz, &readSz, NULL) == 0) {
			CloseHandle(hFile);
			mir_free(pBuf);
			return NULL;
		}
		CloseHandle(hFile);

		if (tUC)
			res = (wchar_t*)pBuf;
		else {
			res = mir_a2u((char *)pBuf);
			mir_free(pBuf);
		}


		return res;
	}
	bufSz = TXTFILEBUFSZ*csz;
	pBuf = (PBYTE)mir_calloc(bufSz);
	if (pBuf == NULL) {
		CloseHandle(hFile);
		return NULL;
	}

	// count number of lines
	do {
		memset(pBuf, 0, bufSz);
		if (ReadFile(hFile, pBuf, bufSz - csz, &readSz, NULL) == 0) {
			CloseHandle(hFile);
			mir_free(pBuf);
			return NULL;
		}
		totalReadSz += readSz;
		for (pCur = pBuf; *pCur != '\0'; pCur += csz) {
			if (tUC) {
				if (!wcsncmp((wchar_t*)pCur, L"\r\n", 2)) {
					lineCount++;
					pCur += csz;
				}
				else if (*(wchar_t*)pCur == '\n')
					lineCount++;
			}
			else {
				if (!strncmp((char *)pCur, "\r\n", 2)) {
					lineCount++;
					pCur += csz;
				}
				else if (*(char *)pCur == '\n')
					lineCount++;
			}
		}
	}
	while ((totalReadSz < fileSz) && (readSz > 0));

	if (lineNo < 0)
		lineNo = lineCount + lineNo + 1;
	else if (*ai->targv[2] == 'r')
		lineNo = (rand() % lineCount) + 1;

	totalReadSz = 0;
	lineCount = 1;
	linePos = 0xFFFFFFFF;
	SetFilePointer(hFile, tUC ? csz : 0, NULL, FILE_BEGIN);
	// find the position in the file where the requested line starts
	do {
		if (ReadFile(hFile, pBuf, bufSz - csz, &readSz, NULL) == 0) {
			CloseHandle(hFile);
			return NULL;
		}
		totalReadSz += readSz;
		for (pCur = pBuf; ((pCur < pBuf + bufSz) && (linePos == 0xFFFFFFFF)); pCur += csz) {
			if (lineCount == lineNo)
				linePos = (tUC ? csz : 0) + totalReadSz - readSz + pCur - pBuf;

			if (tUC) {
				if (!wcsncmp((wchar_t*)pCur, L"\r\n", 2)) {
					lineCount++;
					pCur += csz;
				}
				else if (*(wchar_t*)pCur == '\n')
					lineCount++;
			}
			else {
				if (!strncmp((char *)pCur, "\r\n", 2)) {
					lineCount++;
					pCur += csz;
				}
				else if (*(char *)pCur == '\n')
					lineCount++;
			}
		}
		if (((tUC) && (*(wchar_t*)pCur == '\r')) || ((!tUC) && (*(char *)pCur == '\r'))) {
			// in case the \r was at the end of the buffer, \n could be next
			SetFilePointer(hFile, -1 * csz, NULL, FILE_CURRENT);
			totalReadSz -= csz;
		}
	}
	while ((totalReadSz < fileSz) && (readSz > 0));

	if (SetFilePointer(hFile, linePos, NULL, FILE_BEGIN) != linePos) {
		CloseHandle(hFile);
		mir_free(pBuf);
		return NULL;
	}
	memset(pBuf, 0, bufSz);
	pCur = pBuf;
	do {
		icur = 0;
		if (ReadFile(hFile, pBuf, bufSz - csz, &readSz, NULL) == 0) {
			mir_free(pBuf);
			CloseHandle(hFile);
			return NULL;
		}
		for (pCur = pBuf; (pCur < pBuf + readSz); pCur += csz) {
			if ((tUC) && ((!wcsncmp((wchar_t*)pCur, L"\r\n", 2)) || (*(wchar_t*)pCur == '\n')) ||
				((!tUC) && (((!strncmp((char *)pCur, "\r\n", 2)) || (*(char *)pCur == '\n'))))) {
				CloseHandle(hFile);
				if (tUC)
					*(wchar_t*)pCur = 0;
				else
					*(char *)pCur = '\0';

				if (tUC)
					res = (wchar_t*)pBuf;
				else {
					res = mir_a2u((char *)pBuf);
					mir_free(pBuf);
				}

				return res;
			}
		}
		if (((DWORD)(linePos + (pCur - pBuf)) == fileSz)) { // eof
			CloseHandle(hFile);

			if (tUC) {
				res = (wchar_t*)pBuf;
			}
			else {
				res = mir_a2u((char *)pBuf);
				mir_free(pBuf);
			}

			return res;
		}
		if (readSz == bufSz - csz) {
			// buffer must be increased
			bufSz += TXTFILEBUFSZ*csz;
			if (((tUC) && (*(wchar_t*)pCur == '\r')) || ((!tUC) && (*(char *)pCur == '\r'))) {
				pCur -= csz;
			}
			icur = pCur - pBuf;
			pBuf = (PBYTE)mir_realloc(pBuf, bufSz);
			pCur = pBuf + icur;
			memset((pCur + 1), 0, (TXTFILEBUFSZ * csz));
		}
	}
	while (readSz > 0);
	CloseHandle(hFile);

	return NULL;
}

static wchar_t *parseUpTime(ARGUMENTSINFO *ai)
{
	if (ai->argc != 1)
		return NULL;

	HQUERY hQuery;
	PDH_STATUS pdhStatus = PdhOpenQuery(NULL, 0, &hQuery);
	if (pdhStatus != ERROR_SUCCESS)
		return NULL;

	HCOUNTER hCounter;
	pdhStatus = PdhAddCounter(hQuery, L"\\System\\System Up Time", 0, &hCounter);
	if (pdhStatus != ERROR_SUCCESS) {
		PdhCloseQuery(hQuery);
		return NULL;
	}
	pdhStatus = PdhCollectQueryData(hQuery);
	if (pdhStatus != ERROR_SUCCESS) {
		PdhRemoveCounter(hCounter);
		PdhCloseQuery(hQuery);
		return NULL;
	}

	PDH_FMT_COUNTERVALUE cValue;
	pdhStatus = PdhGetFormattedCounterValue(hCounter, PDH_FMT_LARGE, (LPDWORD)NULL, &cValue);
	if (pdhStatus != ERROR_SUCCESS) {
		PdhRemoveCounter(hCounter);
		PdhCloseQuery(hQuery);
		return NULL;
	}

	wchar_t szVal[32];
	mir_snwprintf(szVal, L"%u", cValue.largeValue);
	PdhRemoveCounter(hCounter);
	PdhCloseQuery(hQuery);
	return mir_wstrdup(szVal);
}

static wchar_t *parseUserName(ARGUMENTSINFO *ai)
{
	if (ai->argc != 1)
		return NULL;

	ai->flags |= AIF_DONTPARSE;
	DWORD len = UNLEN;
	wchar_t *res = (wchar_t*)mir_alloc(len + 1);
	if (res == NULL)
		return NULL;

	memset(res, 0, (len + 1));
	if (!GetUserName(res, &len)) {
		mir_free(res);
		return NULL;
	}
	return res;
}

// clipboard support
static wchar_t *parseClipboard(ARGUMENTSINFO *ai)
{
	if (ai->argc != 1)
		return NULL;

	ai->flags |= AIF_DONTPARSE;

	wchar_t *res = NULL;

	if (IsClipboardFormatAvailable(CF_TEXT)) {
		if (OpenClipboard(NULL)) {
			HANDLE hData = GetClipboardData(CF_UNICODETEXT);
			if (hData != NULL) {
				wchar_t *tszText = (wchar_t*)GlobalLock(hData);
				size_t len = mir_wstrlen(tszText);
				res = (wchar_t*)mir_alloc((len + 1) * sizeof(wchar_t));
				mir_wstrcpy(res, tszText);
				res[len] = 0;
				GlobalUnlock(hData);
			}
			CloseClipboard();
		}
	}

	return res;
}

void registerSystemTokens()
{
	registerIntToken(COMPUTERNAME, parseComputerName, TRF_FIELD, LPGEN("System Functions") "\t" LPGEN("computer name"));
	registerIntToken(CPULOAD, parseCpuLoad, TRF_FUNCTION, LPGEN("System Functions") "\t(x)\t" LPGEN("CPU load of process x (without extension) (x is optional)"));
	registerIntToken(CDATE, parseCurrentDate, TRF_FUNCTION, LPGEN("System Functions") "\t(y)\t" LPGEN("current date in format y (y is optional)"));
	registerIntToken(CTIME, parseCurrentTime, TRF_FUNCTION, LPGEN("System Functions") "\t(y)\t" LPGEN("current time in format y (y is optional)"));
	registerIntToken(DIRECTORY, parseDirectory, TRF_FUNCTION, LPGEN("System Functions") "\t(x,y)\t" LPGEN("the directory y directories above x"));
	registerIntToken(DIRECTORY2, parseDirectory2, TRF_FUNCTION, LPGEN("System Functions") "\t(x,y)\t" LPGEN("strips y directories from x"));
	registerIntToken(DIFFTIME, parseDiffTime, TRF_FUNCTION, LPGEN("System Functions") "\t(x,y)\t" LPGEN("number of seconds between date x and y (x and y in format: M/d/yy H:m:s)"));
	registerIntToken(DIREXISTS, parseDirExists, TRF_FUNCTION, LPGEN("System Functions") "\t(x)\t" LPGEN("TRUE if directory x exists"));
	registerIntToken(FILEEXISTS, parseFileExists, TRF_FUNCTION, LPGEN("System Functions") "\t(x)\t" LPGEN("TRUE if file x exists"));
	registerIntToken(FINDWINDOW, parseFindWindow, TRF_FUNCTION, LPGEN("System Functions") "\t(x)\t" LPGEN("window title of first window of class x"));
	registerIntToken(LISTDIR, parseListDir, TRF_FUNCTION, LPGEN("System Functions") "\t(x,y,z)\t" LPGEN("shows files and directories of directory x, with filter y, separated by z (y and z optional)"));
#ifndef WINE
	registerIntToken(PROCESSRUNNING, parseProcessRunning, TRF_FUNCTION, LPGEN("System Functions") "\t(x)\t" LPGEN("TRUE if process x is running"));
#endif
	registerIntToken(REGISTRYVALUE, parseRegistryValue, TRF_FUNCTION, LPGEN("System Functions") "\t(x,y)\t" LPGEN("value y from registry key x (REG_SZ (string) values only)"));
	registerIntToken(TIMESTAMP2DATE, parseTimestamp2Date, TRF_FUNCTION, LPGEN("System Functions") "\t(x,y)\t" LPGEN("formats timestamp x (seconds since 1/1/1970) in date format y"));
	registerIntToken(TIMESTAMP2TIME, parseTimestamp2Time, TRF_FUNCTION, LPGEN("System Functions") "\t(x,y)\t" LPGEN("formats timestamp x (seconds since 1/1/1970) in time format y"));
	registerIntToken(TXTFILE, parseTextFile, TRF_FUNCTION, LPGEN("System Functions") "\t(x,y)\t" LPGEN("y > 0: line number y from file x, y = 0: the whole file, y < 0: line y counted from the end, y = r: random line"));
	registerIntToken(UPTIME, parseUpTime, TRF_FIELD, LPGEN("System Functions") "\t" LPGEN("uptime in seconds"));

	registerIntToken(ENVIRONMENTVARIABLE, parseEnvironmentVariable, TRF_FUNCTION, LPGEN("System Functions") "\t(x)\t" LPGEN("expand environment variable x"));
	registerIntToken(USERNAME, parseUserName, TRF_FIELD, LPGEN("System Functions") "\t" LPGEN("user name"));

	srand((unsigned int)GetTickCount());

	registerIntToken(CLIPBOARD, parseClipboard, TRF_FIELD, LPGEN("System Functions") "\t" LPGEN("text from clipboard"));
}
