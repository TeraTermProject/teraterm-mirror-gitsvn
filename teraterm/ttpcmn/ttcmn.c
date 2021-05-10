/*
 * Copyright (C) 1994-1998 T. Teranishi
 * (C) 2004- TeraTerm Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* TTCMN.DLL, main */
#include <direct.h>
#include <string.h>
#include <stdio.h>
#include <windows.h>
#include <setupapi.h>
#include <locale.h>
#include <htmlhelp.h>
#include <assert.h>
#include <crtdbg.h>

#define DllExport __declspec(dllexport)
#include "language.h"
#undef DllExport

#include "teraterm.h"
#include "tttypes.h"
#include "ttlib.h"
#include "tt_res.h"
#include "codeconv.h"
#include "compat_win.h"

#define DllExport __declspec(dllexport)
#include "ttcommon.h"
#include "layer_for_unicode.h"

/* shared memory */
typedef struct {
	size_t size_tmap;		/* sizeof TMap */
	size_t size_tttset;		/* sizeof TTTSet */
	/* Setup information from "teraterm.ini" */
	TTTSet ts;
	// Window list
	int NWin;
	HWND WinList[MAXNWIN];
	/* COM port use flag
	 *           bit 8  7  6  5  4  3  2  1
	 * char[0] : COM 8  7  6  5  4  3  2  1
	 * char[1] : COM16 15 14 13 12 11 10  9 ...
	 */
	unsigned char ComFlag[(MAXCOMPORT-1)/CHAR_BIT+1];
	/* Previous window rect (Tera Term 4.78 or later) */
	WINDOWPLACEMENT WinPrevRect[MAXNWIN];
	BOOL WinUndoFlag;
	int WinUndoStyle;
} TMap;
typedef TMap *PMap;

// TMap を格納するファイルマッピングオブジェクト(共有メモリ)の名前
// TMap(とそのメンバ)の更新時は旧バージョンとの同時起動の為に変える必要があるが
// 連番からバージョン番号を使うように変更した為、通常は手動で変更する必要は無い
#define TT_FILEMAPNAME "ttset_memfilemap_" TT_VERSION_STR("_")

/* first instance flag */
static BOOL FirstInstance = TRUE;

static HINSTANCE hInst;

static PMap pm;

static HANDLE HMap = NULL;
#define VTCLASSNAME _T("VTWin32")
#define TEKCLASSNAME _T("TEKWin32")

enum window_style {
	WIN_CASCADE,
	WIN_STACKED,
	WIN_SIDEBYSIDE,
};


void WINAPI CopyShmemToTTSet(PTTSet ts)
{
	// 現在の設定を共有メモリからコピーする
	memcpy(ts, &pm->ts, sizeof(TTTSet));
}

void WINAPI CopyTTSetToShmem(PTTSet ts)
{
	// 現在の設定を共有メモリへコピーする
	memcpy(&pm->ts, ts, sizeof(TTTSet));
}


BOOL WINAPI StartTeraTerm(PTTSet ts)
{
	if (FirstInstance) {
		// init window list
		pm->NWin = 0;
	}
	else {
		/* only the first instance uses saved position */
		pm->ts.VTPos.x = CW_USEDEFAULT;
		pm->ts.VTPos.y = CW_USEDEFAULT;
		pm->ts.TEKPos.x = CW_USEDEFAULT;
		pm->ts.TEKPos.y = CW_USEDEFAULT;
	}

	memcpy(ts,&(pm->ts),sizeof(TTTSet));

	// if (FirstInstance) { の部分から移動 (2008.3.13 maya)
	// 起動時には、共有メモリの HomeDir と SetupFName は空になる
	/* Get home directory */
	GetHomeDir(hInst, ts->HomeDir, sizeof(ts->HomeDir));
	_chdir(ts->HomeDir);
	GetDefaultSetupFName(ts->HomeDir, ts->SetupFName, sizeof(ts->SetupFName));

	strncpy_s(ts->KeyCnfFN, sizeof(ts->KeyCnfFN), ts->HomeDir, _TRUNCATE);
	AppendSlash(ts->KeyCnfFN, sizeof(ts->KeyCnfFN));
	strncat_s(ts->KeyCnfFN, sizeof(ts->KeyCnfFN), "KEYBOARD.CNF", _TRUNCATE);

	if (FirstInstance) {
		FirstInstance = FALSE;
		return TRUE;
	}
	else {
		return FALSE;
	}
}

// 設定ファイルをディスクに保存し、Tera Term本体を再起動する。
// (2012.4.30 yutaka)
void WINAPI RestartTeraTerm(HWND hwnd, PTTSet ts)
{
	char path[1024];
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	int ret;

	static const TTMessageBoxInfoW info = {
		"Tera Term",
		NULL, L"Tera Term: Configuration Warning",
		"MSG_TT_TAKE_EFFECT",
		L"This option takes effect the next time a session is started.\n"
		L"Are you sure that you want to relaunch Tera Term?"
	};
	ret = TTMessageBoxW(hwnd, &info, MB_YESNO | MB_ICONEXCLAMATION | MB_DEFBUTTON2, ts->UILanguageFile);
	if (ret != IDYES)
		return;

	SendMessage(hwnd, WM_COMMAND, ID_SETUP_SAVE, 0);
	// ID_FILE_EXIT メッセージではアプリが落ちることがあるため、WM_QUIT をポストする。
	//PostMessage(hwnd, WM_COMMAND, ID_FILE_EXIT, 0);
	PostQuitMessage(0);

	// 自プロセスの再起動。
	if (GetModuleFileName(NULL, path, sizeof(path)) == 0) {
		return;
	}
	memset(&si, 0, sizeof(si));
	GetStartupInfo(&si);
	memset(&pi, 0, sizeof(pi));
	if (CreateProcess(NULL, path, NULL, NULL, FALSE, 0,
	                  NULL, NULL, &si, &pi) == 0) {
	}
}

void WINAPI SetCOMFlag(int Com)
{
	pm->ComFlag[(Com-1)/CHAR_BIT] |= 1 << ((Com-1)%CHAR_BIT);
}

void WINAPI ClearCOMFlag(int Com)
{
	pm->ComFlag[(Com-1)/CHAR_BIT] &= ~(1 << ((Com-1)%CHAR_BIT));
}

int WINAPI CheckCOMFlag(int Com)
{
	return ((pm->ComFlag[(Com-1)/CHAR_BIT] & 1 << (Com-1)%CHAR_BIT) > 0);
}

int WINAPI RegWin(HWND HWinVT, HWND HWinTEK)
{
	int i, j;

	if (pm->NWin>=MAXNWIN)
		return 0;
	if (HWinVT==NULL)
		return 0;
	if (HWinTEK!=NULL) {
		i = 0;
		while ((i<pm->NWin) && (pm->WinList[i]!=HWinVT))
			i++;
		if (i>=pm->NWin)
			return 0;
		for (j=pm->NWin-1 ; j>i ; j--)
			pm->WinList[j+1] = pm->WinList[j];
		pm->WinList[i+1] = HWinTEK;
		pm->NWin++;
		return 0;
	}
	pm->WinList[pm->NWin++] = HWinVT;
	memset(&pm->WinPrevRect[pm->NWin - 1], 0, sizeof(pm->WinPrevRect[pm->NWin - 1])); // RECT clear
	if (pm->NWin==1) {
		return 1;
	}
	else {
		return (int)(SendMessage(pm->WinList[pm->NWin-2],
		                         WM_USER_GETSERIALNO,0,0)+1);
	}
}

void WINAPI UnregWin(HWND HWin)
{
	int i, j;

	i = 0;
	while ((i<pm->NWin) && (pm->WinList[i]!=HWin)) {
		i++;
	}
	if (pm->WinList[i]!=HWin) {
		return;
	}
	for (j=i ; j<pm->NWin-1 ; j++) {
		pm->WinList[j] = pm->WinList[j+1];
		pm->WinPrevRect[j] = pm->WinPrevRect[j+1];  // RECT shift
	}
	if (pm->NWin>0) {
		pm->NWin--;
	}
}

char GetWindowTypeChar(HWND Hw, HWND HWin)
{
#if 0
	if (HWin == Hw)
		return '*';
	else if (!IsWindowVisible(Hw))
#else
	if (!IsWindowVisible(Hw))
#endif
		return '#';
	else if (IsIconic(Hw))
		return '-';
	else if (IsZoomed(Hw))
		return '@';
	else
		return '+';
}

void WINAPI SetWinMenu(HMENU menu, PCHAR buf, int buflen, PCHAR langFile, int VTFlag)
{
	int i;
	char Temp[MAXPATHLEN];
	HWND Hw;
	wchar_t uimsg[MAX_UIMSG];

	// delete all items in Window menu
	i = GetMenuItemCount(menu);
	if (i>0)
		do {
			i--;
			RemoveMenu(menu,i,MF_BYPOSITION);
		} while (i>0);

	i = 0;
	while (i<pm->NWin) {
		Hw = pm->WinList[i]; // get window handle
		if ((GetClassName(Hw,Temp,sizeof(Temp))>0) &&
		    ((strcmp(Temp,VTCLASSNAME)==0) ||
		     (strcmp(Temp,TEKCLASSNAME)==0))) {
			Temp[0] = '&';
			Temp[1] = (char)(0x31 + i);
			Temp[2] = ' ';
			Temp[3] = GetWindowTypeChar(Hw, NULL);
			Temp[4] = ' ';
			GetWindowText(Hw,&Temp[5],sizeof(Temp)-6);
			AppendMenu(menu,MF_ENABLED | MF_STRING,ID_WINDOW_1+i,Temp);
			i++;
			if (i>8) {
				i = pm->NWin;
			}
		}
		else {
			UnregWin(Hw);
		}
	}
	if (VTFlag == 1) {
		static const DlgTextInfo MenuTextInfo[] = {
			{ ID_WINDOW_WINDOW, "MENU_WINDOW_WINDOW" },
			{ ID_WINDOW_MINIMIZEALL, "MENU_WINDOW_MINIMIZEALL" },
			{ ID_WINDOW_RESTOREALL, "MENU_WINDOW_RESTOREALL" },
			{ ID_WINDOW_CASCADEALL, "MENU_WINDOW_CASCADE" },
			{ ID_WINDOW_STACKED, "MENU_WINDOW_STACKED" },
			{ ID_WINDOW_SIDEBYSIDE, "MENU_WINDOW_SIDEBYSIDE" },
		};

		AppendMenu(menu, MF_SEPARATOR, 0, NULL);
		AppendMenu(menu, MF_ENABLED | MF_STRING, ID_WINDOW_WINDOW, "&Window");
		AppendMenu(menu, MF_ENABLED | MF_STRING, ID_WINDOW_MINIMIZEALL, "&Minimize All");
		AppendMenu(menu, MF_ENABLED | MF_STRING, ID_WINDOW_RESTOREALL, "&Restore All");
		AppendMenu(menu, MF_ENABLED | MF_STRING, ID_WINDOW_CASCADEALL, "&Cascade");
		AppendMenu(menu, MF_ENABLED | MF_STRING, ID_WINDOW_STACKED, "&Stacked");
		AppendMenu(menu, MF_ENABLED | MF_STRING, ID_WINDOW_SIDEBYSIDE, "Side &by Side");

		SetI18nMenuStrs("Tera Term", menu, MenuTextInfo, _countof(MenuTextInfo), langFile);

		if (pm->WinUndoFlag) {
			if (pm->WinUndoStyle == WIN_CASCADE)
				get_lang_msgW("MENU_WINDOW_CASCADE_UNDO", uimsg, _countof(uimsg), L"&Undo - Cascade", langFile);
			else if (pm->WinUndoStyle == WIN_STACKED)
				get_lang_msgW("MENU_WINDOW_STACKED_UNDO", uimsg, _countof(uimsg), L"&Undo - Stacked", langFile);
			else
				get_lang_msgW("MENU_WINDOW_SIDEBYSIDE_UNDO", uimsg, _countof(uimsg), L"&Undo - Side by Side", langFile);
			_AppendMenuW(menu, MF_ENABLED | MF_STRING, ID_WINDOW_UNDO, uimsg);		// TODO UNICODE
		}

	}
	else {
		get_lang_msgW("MENU_WINDOW_WINDOW", uimsg, _countof(uimsg), L"&Window", langFile);
		_AppendMenuW(menu,MF_ENABLED | MF_STRING,ID_TEKWINDOW_WINDOW, uimsg);
	}
}

void WINAPI SetWinList(HWND HWin, HWND HDlg, int IList)
{
	int i;
	char Temp[MAXPATHLEN];
	HWND Hw;

	for (i=0; i<pm->NWin; i++) {
		Hw = pm->WinList[i]; // get window handle
		if ((GetClassName(Hw,Temp,sizeof(Temp))>0) &&
		    ((strcmp(Temp,VTCLASSNAME)==0) ||
		     (strcmp(Temp,TEKCLASSNAME)==0))) {
			Temp[0] = GetWindowTypeChar(Hw, HWin);
			Temp[1] = ' ';
			GetWindowText(Hw,&Temp[2],sizeof(Temp)-3);
			SendDlgItemMessage(HDlg, IList, LB_ADDSTRING,
			                   0, (LPARAM)Temp);
			if (Hw==HWin) {
				SendDlgItemMessage(HDlg, IList, LB_SETCURSEL, i,0);
			}
		}
		else {
			UnregWin(Hw);
		}
	}
}

void WINAPI SelectWin(int WinId)
{
	if ((WinId>=0) && (WinId<pm->NWin)) {
		/* ウィンドウが最大化および最小化されていた場合、その状態を維持できるように、
		 * SW_SHOWNORMAL から SW_SHOW へ変更した。
		 * (2009.11.8 yutaka)
		 * ウィンドウが最小化されているときは元のサイズに戻す(SW_RESTORE)ようにした。
		 * (2009.11.9 maya)
		 */
		if (IsIconic(pm->WinList[WinId])) {
			ShowWindow(pm->WinList[WinId],SW_RESTORE);
		}
		else {
			ShowWindow(pm->WinList[WinId],SW_SHOW);
		}
		SetForegroundWindow(pm->WinList[WinId]);
	}
}

void WINAPI SelectNextWin(HWND HWin, int Next, BOOL SkipIconic)
{
	int i;

	i = 0;
	while ((i < pm->NWin) && (pm->WinList[i]!=HWin)) {
		i++;
	}
	if (pm->WinList[i]!=HWin) {
		return;
	}

	do {
		i += Next;
		if (i >= pm->NWin) {
			i = 0;
		}
		else if (i < 0) {
			i = pm->NWin-1;
		}

		if (pm->WinList[i] == HWin) {
			break;
		}
	} while ((SkipIconic && IsIconic(pm->WinList[i])) || !IsWindowVisible(pm->WinList[i]));

	SelectWin(i);
}

void WINAPI ShowAllWin(int stat) {
	int i;

	for (i=0; i < pm->NWin; i++) {
		ShowWindow(pm->WinList[i], stat);
	}
}

void WINAPI UndoAllWin(void) {
	int i;
	WINDOWPLACEMENT rc0;
	RECT rc;
	int stat = SW_RESTORE;
	int multi_mon = 0;

	if (HasMultiMonitorSupport()) {
		multi_mon = 1;
	}

	// 一度、復元したらフラグは落とす。
	pm->WinUndoFlag = FALSE;

	memset(&rc0, 0, sizeof(rc0));

	for (i=0; i < pm->NWin; i++) {
		// 復元指定で、前回の状態が残っている場合は、ウィンドウの状態を元に戻す。
		if (stat == SW_RESTORE && memcmp(&pm->WinPrevRect[i], &rc0, sizeof(rc0)) != 0) {
			rc = pm->WinPrevRect[i].rcNormalPosition;

			// NT4.0, 95 はマルチモニタAPIに非対応
			if (multi_mon) {
				// 対象モニタの情報を取得
				HMONITOR hMonitor;
				MONITORINFO mi;
				hMonitor = pMonitorFromRect(&rc, MONITOR_DEFAULTTONEAREST);
				mi.cbSize = sizeof(MONITORINFO);
				pGetMonitorInfoA(hMonitor, &mi);

				// 位置補正（復元前後で解像度が変わっている場合への対策）
				if (rc.right > mi.rcMonitor.right) {
					rc.left -= rc.right - mi.rcMonitor.right;
					rc.right = mi.rcMonitor.right;
				}
				if (rc.left < mi.rcMonitor.left) {
					rc.right += mi.rcMonitor.left - rc.left;
					rc.left = mi.rcMonitor.left;
				}
				if (rc.bottom > mi.rcMonitor.bottom) {
					rc.top -= rc.bottom - mi.rcMonitor.bottom;
					rc.bottom = mi.rcMonitor.bottom;
				}
				if (rc.top < mi.rcMonitor.top) {
					rc.bottom += mi.rcMonitor.top - rc.top;
					rc.top = mi.rcMonitor.top;
				}
			}

			// ウィンドウ位置復元
			SetWindowPos(
				pm->WinList[i], NULL,
				rc.left,
				rc.top,
				rc.right - rc.left,
				rc.bottom - rc.top,
				SWP_NOZORDER);

			// ウィンドウの状態復元
			ShowWindow(pm->WinList[i], pm->WinPrevRect[i].showCmd);

		} else {
			ShowWindow(pm->WinList[i], stat);
		}
	}
}

void WINAPI OpenHelp(UINT Command, DWORD Data, char *UILanguageFile)
{
	char HomeDir[MAX_PATH];
	char Temp[MAX_PATH];
	HWND HWin;
	wchar_t HelpFN[MAX_PATH];
	wchar_t uimsg[MAX_UIMSG];
	wchar_t *HomeDirW;

	 /* Get home directory */
	if (GetModuleFileNameA(NULL,Temp,_countof(Temp)) == 0) {
		return;
	}
	ExtractDirName(Temp, HomeDir);
	HomeDirW = ToWcharA(HomeDir);
	get_lang_msgW("HELPFILE", uimsg, _countof(uimsg), L"teraterm.chm", UILanguageFile);
	_snwprintf_s(HelpFN, _countof(HelpFN), _TRUNCATE, L"%s\\%s", HomeDirW, uimsg);
	free(HomeDirW);

	// ヘルプのオーナーは常にデスクトップになる (2007.5.12 maya)
	HWin = GetDesktopWindow();
	if (_HtmlHelpW(HWin, HelpFN, Command, Data) == NULL) {
		// ヘルプが開けなかった
		static const TTMessageBoxInfoW info = {
			"Tera Term",
			NULL, L"Tera Term: HTML help",
			"MSG_OPENHELP_ERROR", L"Can't open HTML help file(%s)." };
		TTMessageBoxW(HWin, &info, MB_OK | MB_ICONERROR, UILanguageFile, HelpFN);
		return;
	}
}

HWND WINAPI GetNthWin(int n)
{
	if (n<pm->NWin) {
		return pm->WinList[n];
	}
	else {
		return NULL;
	}
}

int WINAPI GetRegisteredWindowCount(void)
{
	return (pm->NWin);
}

// 有効なウィンドウを探し、現在位置を記憶させておく。
static void get_valid_window_and_memorize_rect(HWND myhwnd, HWND hwnd[], int *num, int style)
{
	int i, n;
	WINDOWPLACEMENT wndPlace;

	// 元に戻す(Undo)メニューは一度だけ表示させる。
	if (pm->WinUndoFlag == FALSE) {
		pm->WinUndoFlag = TRUE;
	} else {
		// すでにメニューが表示されていて、かつ以前と同じスタイルが選択されたら、
		// メニューを消す。
		// Windows8では、さらに連続して同じスタイルを選択してもメニューが消えたままだが、
		// Tera Termではメニューが表示されるため、動作が異なる。
		if (pm->WinUndoStyle == style)
			pm->WinUndoFlag = FALSE;
	}
	pm->WinUndoStyle = style;

	n = 0;
	for (i = 0 ; i < pm->NWin ; i++) {
		// 現在位置を覚えておく。
		wndPlace.length = sizeof(WINDOWPLACEMENT);
		GetWindowPlacement(pm->WinList[i], &wndPlace);
		pm->WinPrevRect[i] = wndPlace;

		// 自分自身は先頭にする。
		if (pm->WinList[i] == myhwnd) {
			hwnd[n] = hwnd[0];
			hwnd[0] = myhwnd;
		} else {
			hwnd[n] = pm->WinList[i];
		}
		n++;
	}
	*num = n;
}

// ウィンドウを左右に並べて表示する(Show Windows Side by Side)
void WINAPI ShowAllWinSidebySide(HWND myhwnd)
{
	int n;
	HWND hwnd[MAXNWIN];

	get_valid_window_and_memorize_rect(myhwnd, hwnd, &n, WIN_SIDEBYSIDE);
	TileWindows(NULL, MDITILE_VERTICAL, NULL, n, hwnd);
}

// ウィンドウを上下に並べて表示する(Show Windows Stacked)
void WINAPI ShowAllWinStacked(HWND myhwnd)
{
	int n;
	HWND hwnd[MAXNWIN];

	get_valid_window_and_memorize_rect(myhwnd, hwnd, &n, WIN_STACKED);
	TileWindows(NULL, MDITILE_HORIZONTAL, NULL, n, hwnd);
}

// ウィンドウを重ねて表示する(Cascade)
void WINAPI ShowAllWinCascade(HWND myhwnd)
{
	int n;
	HWND hwnd[MAXNWIN];

	get_valid_window_and_memorize_rect(myhwnd, hwnd, &n, WIN_CASCADE);
	CascadeWindows(NULL, MDITILE_SKIPDISABLED, NULL, n, hwnd);
}

// 全Tera Termに終了指示を出す。
void WINAPI BroadcastClosingMessage(HWND myhwnd)
{
	int i, max;
	HWND hwnd[MAXNWIN];

	// Tera Termを終了させると、共有メモリが変化するため、
	// いったんバッファにコピーしておく。
	max = pm->NWin;
	for (i = 0 ; i < pm->NWin ; i++) {
		hwnd[i] = pm->WinList[i];
	}

	for (i = 0 ; i < max ; i++) {
		// 自分自身は最後にする。
		if (hwnd[i] == myhwnd)
			continue;

		PostMessage(hwnd[i], WM_USER_NONCONFIRM_CLOSE, 0, 0);
	}
	PostMessage(myhwnd, WM_USER_NONCONFIRM_CLOSE, 0, 0);
}


int WINAPI CommReadRawByte(PComVar cv, LPBYTE b)
{
	if ( ! cv->Ready ) {
		return 0;
	}

	if ( cv->InBuffCount>0 ) {
		*b = cv->InBuff[cv->InPtr];
		cv->InPtr++;
		cv->InBuffCount--;
		if ( cv->InBuffCount==0 ) {
			cv->InPtr = 0;
		}
		return 1;
	}
	else {
		cv->InPtr = 0;
		return 0;
	}
}

static void LogBinSkip(PComVar cv, int add)
{
	if (cv->LogBinSkip != NULL) {
		cv->LogBinSkip(add);
	}
}

void WINAPI CommInsert1Byte(PComVar cv, BYTE b)
{
	if ( ! cv->Ready ) {
		return;
	}

	if (cv->InPtr == 0) {
		memmove(&(cv->InBuff[1]),&(cv->InBuff[0]),cv->InBuffCount);
	}
	else {
		cv->InPtr--;
	}
	cv->InBuff[cv->InPtr] = b;
	cv->InBuffCount++;

	LogBinSkip(cv, 1);
}

static void Log1Bin(PComVar cv, BYTE b)
{
	if (cv->Log1Bin != NULL) {
		cv->Log1Bin(b);
	}
}

int WINAPI CommRead1Byte(PComVar cv, LPBYTE b)
{
	int c;

	if ( ! cv->Ready ) {
		return 0;
	}

	if ( cv->TelMode ) {
		c = 0;
	}
	else {
		c = CommReadRawByte(cv,b);
	}

	if ((c==1) && cv->TelCRFlag) {
		cv->TelCRFlag = FALSE;
		if (*b==0) {
			c = 0;
		}
	}

	if ( c==1 ) {
		if ( cv->IACFlag ) {
			cv->IACFlag = FALSE;
			if ( *b != 0xFF ) {
				cv->TelMode = TRUE;
				CommInsert1Byte(cv,*b);
				LogBinSkip(cv, -1);
				c = 0;
			}
		}
		else if ((cv->PortType==IdTCPIP) && (*b==0xFF)) {
			if (!cv->TelFlag && cv->TelAutoDetect) { /* TTPLUG */
				cv->TelFlag = TRUE;
			}
			if (cv->TelFlag) {
				cv->IACFlag = TRUE;
				c = 0;
			}
		}
		else if (cv->TelFlag && ! cv->TelBinRecv && (*b==0x0D)) {
			cv->TelCRFlag = TRUE;
		}
	}

	if (c == 1) {
		Log1Bin(cv, *b);
	}

	return c;
}

int WINAPI CommRawOut(PComVar cv, /*const*/ PCHAR B, int C)
{
	int a;

	if ( ! cv->Ready ) {
		return C;
	}

	if (C > OutBuffSize - cv->OutBuffCount) {
		a = OutBuffSize - cv->OutBuffCount;
	}
	else {
		a = C;
	}
	if ( cv->OutPtr > 0 ) {
		memmove(&(cv->OutBuff[0]),&(cv->OutBuff[cv->OutPtr]),cv->OutBuffCount);
		cv->OutPtr = 0;
	}
	memcpy(&(cv->OutBuff[cv->OutBuffCount]),B,a);
	cv->OutBuffCount = cv->OutBuffCount + a;
	return a;
}

int WINAPI CommBinaryOut(PComVar cv, PCHAR B, int C)
{
	int a, i, Len;
	char d[3];

	if ( ! cv->Ready ) {
		return C;
	}

	i = 0;
	a = 1;
	while ((a>0) && (i<C)) {
		Len = 0;

		d[Len] = B[i];
		Len++;

		if ( cv->TelFlag && (B[i]=='\x0d') && ! cv->TelBinSend ) {
			d[Len++] = '\x00';
		}
		else if ( cv->TelFlag && (B[i]=='\xff') ) {
			d[Len++] = '\xff';
		}

		if ( OutBuffSize - cv->OutBuffCount - Len >= 0 ) {
			CommRawOut(cv, d, Len);
			a = 1;
		}
		else {
			a = 0;
		}

		i += a;
	}
	return i;
}

/**
 *	データ(文字列)を出力バッファへ書き込む
 *
 *	指定データがすべて書き込めない場合は書き込まない
 *	CommRawOut() は書き込める分だけ書き込む
 *
 *	@retval	TRUE	出力できた
 *	@retval	FALSE	出力できなかった(buffer full)
 */
static BOOL WriteOutBuff(PComVar cv, const char *TempStr, int TempLen)
{
	BOOL output;

	if (TempLen == 0) {
		// 長さ0で書き込みに来る場合あり
		return TRUE;
	}

	output = FALSE;
	if (cv->TelLineMode) {
		const BOOL Full = OutBuffSize - cv->LineModeBuffCount - TempLen < 0;
		if (!Full) {
			output = TRUE;
			memcpy(&(cv->LineModeBuff[cv->LineModeBuffCount]), TempStr, TempLen);
			cv->LineModeBuffCount += TempLen;
			if (cv->Flush) {
				cv->FlushLen = cv->LineModeBuffCount;
			}
		}
		if (cv->FlushLen > 0) {
			const int OutLen = CommRawOut(cv, cv->LineModeBuff, cv->FlushLen);
			cv->FlushLen -= OutLen;
			cv->LineModeBuffCount -= OutLen;
			memmove(cv->LineModeBuff, &(cv->LineModeBuff[OutLen]), cv->LineModeBuffCount);
		}
		cv->Flush = FALSE;
	}
	else {
		const BOOL Full = OutBuffSize-cv->OutBuffCount-TempLen < 0;
		if (! Full) {
			output = TRUE;
			CommRawOut(cv, (char *)TempStr, TempLen);
		}
	}
	return output;
}

/**
 *	データ(文字列)を入力バッファへ書き込む
 *	入力バッファへ入れる -> エコーされる
 *
 *	@retval	TRUE	出力できた
 *	@retval	FALSE	出力できなかった(buffer full)
 */
static BOOL WriteInBuff(PComVar cv, const char *TempStr, int TempLen)
{
	BOOL Full;

	if (TempLen == 0) {
		return TRUE;
	}

	Full = InBuffSize-cv->InBuffCount-TempLen < 0;
	if (! Full) {
		memcpy(&(cv->InBuff[cv->InBuffCount]),TempStr,TempLen);
		cv->InBuffCount = cv->InBuffCount + TempLen;
		return TRUE;
	}
	return FALSE;
}

/**
 *	入力バッファの先頭に空きがあったら詰める
 */
static void PackInBuff(PComVar cv)
{
	if ( (cv->InPtr>0) && (cv->InBuffCount>0) ) {
		memmove(cv->InBuff,&(cv->InBuff[cv->InPtr]),cv->InBuffCount);
		cv->InPtr = 0;
	}
}

int WINAPI CommBinaryBuffOut(PComVar cv, PCHAR B, int C)
{
	int a, i, Len;
	char d[3];

	if ( ! cv->Ready ) {
		return C;
	}

	i = 0;
	a = 1;
	while ((a>0) && (i<C)) {
		Len = 0;

		d[Len] = B[i];
		Len++;

		if (B[i] == CR) {
			if ( cv->TelFlag && ! cv->TelBinSend ) {
				d[Len++] = '\x00';
			}
			if (cv->TelLineMode) {
				cv->Flush = TRUE;
			}
		}
		else if ( cv->TelFlag && (B[i]=='\xff') ) {
			d[Len++] = '\xff';
		}

		if (WriteOutBuff(cv, d, Len)) {
			a = 1;
			i++;
		} else {
			a = 0;
		}
	}
	return i;
}

// 内部コード(CodePage)をUTF-8へ出力する
static int OutputTextUTF8(WORD K, char *TempStr, PComVar cv)
{
	int CodePage = *cv->CodePage;
	unsigned int code;
	int outlen;

	code = MBCP_UTF32(K, CodePage);
	if (code == 0) {
		// 変換失敗
		code = 0xfffd; // U+FFFD: Replacement Character
	}
	outlen = UTF32ToUTF8(code, TempStr, 4);
	return outlen;
}

static int __isleadbyte_l(int d, PComVar cv)
{
	int CodePage = *cv->CodePage;
	return IsDBCSLeadByteEx(CodePage, d);
}

//
// MBCSから各種漢字コードへ変換して出力する。
//
static int TextOutMBCS(PComVar cv, PCHAR B, int C)
{
	int i, TempLen;
	WORD K;
	char TempStr[12];
	BYTE d;
	BOOL Full;
	int SendCodeNew;	// 送信コード
	BOOL KanjiFlagNew;	// TRUE=次の文字と合わせて漢字とする

	Full = FALSE;
	i = 0;
	while (! Full && (i < C)) {
		TempLen = 0;
		d = (BYTE)B[i];
		SendCodeNew = cv->SendCode;
		KanjiFlagNew = FALSE;

		if (cv->SendKanjiFlag) {
			SendCodeNew = IdKanji;

			K = (cv->SendKanjiFirst << 8) + d;

			// UTF-8への変換を行う。1〜3バイトまでの対応なので注意。
			if (cv->KanjiCodeSend == IdUTF8 || cv->Language == IdUtf8) {
				TempLen += OutputTextUTF8(K, TempStr, cv);
			}
			else {
				switch (cv->Language) {
				  case IdJapanese:
					switch (cv->KanjiCodeSend) {
					  case IdEUC:
						K = SJIS2EUC(K);
						break;
					  case IdJIS:
						K = SJIS2JIS(K);
						if ((cv->SendCode==IdKatakana) &&
						    (cv->JIS7KatakanaSend==1)) {
							TempStr[TempLen++] = SI;
						}
						break;
					  case IdSJIS:
						/* nothing to do */
						break;
					}
					break;
				  case IdKorean:
					break;
				}
				TempStr[TempLen++] = HIBYTE(K);
				TempStr[TempLen++] = LOBYTE(K);
			}
		}
		else if (__isleadbyte_l(d, cv)) {
			KanjiFlagNew = TRUE;
			cv->SendKanjiFirst = d;
			SendCodeNew = IdKanji;

			if (cv->Language == IdJapanese) {
				if ((cv->SendCode!=IdKanji) && (cv->KanjiCodeSend==IdJIS)) {
					TempStr[0] = 0x1B;
					TempStr[1] = '$';
					if (cv->KanjiIn == IdKanjiInB) {
						TempStr[2] = 'B';
					}
					else {
						TempStr[2] = '@';
					}
					TempLen = 3;
				}
			}
		}
		else {
			if (cv->Language == IdJapanese) {
				if ((cv->SendCode==IdKanji) && (cv->KanjiCodeSend==IdJIS)) {
					TempStr[0] = 0x1B;
					TempStr[1] = '(';
					switch (cv->KanjiOut) {
					  case IdKanjiOutJ:
						TempStr[2] = 'J';
						break;
					  case IdKanjiOutH:
						TempStr[2] = 'H';
						break;
					  default:
						TempStr[2] = 'B';
					}
					TempLen = 3;
				}

				if ((0xa0<d) && (d<0xe0)) {
					SendCodeNew = IdKatakana;
					if ((cv->SendCode!=IdKatakana) &&
					    (cv->KanjiCodeSend==IdJIS) &&
					    (cv->JIS7KatakanaSend==1)) {
						TempStr[TempLen++] = SO;
					}
				}
				else {
					SendCodeNew = IdASCII;
					if ((cv->SendCode==IdKatakana) &&
					    (cv->KanjiCodeSend==IdJIS) &&
					    (cv->JIS7KatakanaSend==1)) {
						TempStr[TempLen++] = SI;
					}
				}
			}

			if (d==CR) {
				TempStr[TempLen++] = 0x0d;
				if (cv->CRSend==IdCRLF) {
					TempStr[TempLen++] = 0x0a;
				}
				else if ((cv->CRSend==IdCR) &&
				          cv->TelFlag && ! cv->TelBinSend) {
					TempStr[TempLen++] = 0;
				}
				else if (cv->CRSend == IdLF) {
					TempStr[TempLen-1] = 0x0a;
				}
				if (cv->TelLineMode) {
					cv->Flush = TRUE;
				}
			}
			else if (d==BS) {
				if (cv->TelLineMode) {
					if (cv->FlushLen < cv->LineModeBuffCount) {
						cv->LineModeBuffCount--;
					}
				}
				else {
					TempStr[TempLen++] = d;
				}
			}
			else if (d==0x15) { // Ctrl-U
				if (cv->TelLineMode) {
					cv->LineModeBuffCount = cv->FlushLen;
				}
				else {
					TempStr[TempLen++] = d;
				}
			}
			else if ((d>=0x80) && (cv->KanjiCodeSend==IdUTF8 || cv->Language==IdUtf8)) {
				TempLen += OutputTextUTF8((WORD)d, TempStr, cv);
			}
			else if ((d>=0xa1) && (d<=0xe0) && (cv->Language == IdJapanese)) {
				/* Katakana */
				if (cv->KanjiCodeSend==IdEUC) {
					TempStr[TempLen++] = (char)SS2;
				}
				if ((cv->KanjiCodeSend==IdJIS) &&
					(cv->JIS7KatakanaSend==1)) {
					TempStr[TempLen++] = d & 0x7f;
				}
				else {
					TempStr[TempLen++] = d;
				}
			}
			else {
				TempStr[TempLen++] = d;
				if (cv->TelFlag && (d==0xff)) {
					TempStr[TempLen++] = (char)0xff;
				}
			}
		} // if (cv->SendKanjiFlag) else if ... else ... end

		if (WriteOutBuff(cv, TempStr, TempLen)) {
			i++;	// 1文字処理した
			// 漢字の状態を保存する
			cv->SendCode = SendCodeNew;
			cv->SendKanjiFlag = KanjiFlagNew;
		} else {
			Full = TRUE;
		}

	} // end of "while {}"

	return i;
}

int WINAPI CommTextOut(PComVar cv, PCHAR B, int C)
{
	int i, TempLen;
	char TempStr[12];
	BYTE d;
	BOOL Full;

	if (! cv->Ready ) {
		return C;
	}

	switch (cv->Language) {
	  case IdUtf8:
	  case IdJapanese:
	  case IdKorean:
		return TextOutMBCS(cv, B, C);
		break;
	}

	Full = FALSE;
	i = 0;
	while (! Full && (i < C)) {
		TempLen = 0;
		d = (BYTE)B[i];

		switch (d) {
		  case CR:
			TempStr[TempLen] = 0x0d;
			TempLen++;
			if (cv->CRSend==IdCRLF) {
				TempStr[TempLen++] = 0x0a;
			}
			else if (cv->CRSend==IdCR && cv->TelFlag && ! cv->TelBinSend) {
				TempStr[TempLen++] = 0;
			}
			else if (cv->CRSend == IdLF) {
				TempStr[TempLen-1] = 0x0a;
			}
			if (cv->TelLineMode) {
				cv->Flush = TRUE;
			}
			break;

		  case BS:
			if (cv->TelLineMode) {
				if (cv->FlushLen < cv->LineModeBuffCount) {
					cv->LineModeBuffCount--;
				}
			}
			else {
				TempStr[TempLen++] = d;
			}
			break;

		  case 0x15: // Ctrl-U
			if (cv->TelLineMode) {
				cv->LineModeBuffCount = cv->FlushLen;
			}
			else {
				TempStr[TempLen++] = d;
			}
			break;

		  default:
			if ((cv->Language==IdRussian) && (d>=128)) {
				d = RussConv(cv->RussClient, cv->RussHost, d);
			}
			TempStr[TempLen++] = d;
			if (cv->TelFlag && (d==0xff)) {
				TempStr[TempLen++] = (char)0xff;
			}
		}

		if (WriteOutBuff(cv, TempStr, TempLen)) {
			i++;	// 1文字処理した
		} else {
			Full = TRUE;
		}

	} // end of while {}

	return i;
}

/**
 *	@retval	true	日本語の半角カタカナ
 *	@retval	false	その他
 */
static BOOL IsHalfWidthKatakana(unsigned int u32)
{
	// Halfwidth CJK punctuation (U+FF61〜FF64)
	// Halfwidth Katakana variants (U+FF65〜FF9F)
	return (0xff61 <= u32 && u32 <= 0xff9f);
}

/**
 *	出力用、 TODO echo用を作る
 *	@param	cv
 *	@param	u32			入力文字
 *	@param	check_only	TRUEで処理は行わず、
 *	@param	TempStr		出力文字数
 *	@param	StrLen		TempStrへの出力文字数
 *	@retval	処理を行った
 */
static BOOL OutControl(PComVar cv, unsigned int u32, BOOL check_only, char *TempStr, size_t *StrLen)
{
	const wchar_t d = u32;
	size_t TempLen = 0;
	BOOL retval = FALSE;
	if (check_only == TRUE) {
		/* チェックのみ */
		if (d == CR || d == BS || d == 0x15/*ctrl-u*/) {
			return TRUE;
		} else {
			return FALSE;
		}
	}
	if (d==CR) {
		TempStr[TempLen++] = 0x0d;
		if (cv->CRSend==IdCRLF) {
			TempStr[TempLen++] = 0x0a;
		}
		else if ((cv->CRSend ==IdCR) &&
				 cv->TelFlag && ! cv->TelBinSend) {
			TempStr[TempLen++] = 0;
		}
		else if (cv->CRSend == IdLF) {
			TempStr[TempLen-1] = 0x0a;
		}
		if (cv->TelLineMode) {
			cv->Flush = TRUE;
		}
		retval = TRUE;
	}
	else if (d== BS) {
		if (cv->TelLineMode) {
			if (cv->FlushLen < cv->LineModeBuffCount) {
				cv->LineModeBuffCount--;
			}
		}
		else {
			TempStr[TempLen++] = BS;
		}
		retval = TRUE;
	}
	else if (d==0x15) { // ctrl-u
		if (cv->TelLineMode) {
			cv->LineModeBuffCount = cv->FlushLen;
		}
		else {
			TempStr[TempLen++] = 0x15;
		}
		retval = TRUE;
	}
	*StrLen = TempLen;
	return retval;
}
static BOOL ControlEcho(PComVar cv, unsigned int u32, BOOL check_only, char *TempStr, size_t *StrLen)
{
	const wchar_t d = u32;
	size_t TempLen = 0;
	BOOL retval = FALSE;
	if (check_only == TRUE) {
		/* チェックのみ */
		if (d == CR || (d == 0x15/*ctrl-u*/ && cv->TelLineMode)) {
			return TRUE;
		} else {
			return FALSE;
		}
	}
	if (d==CR) {
		TempStr[TempLen++] = 0x0d;
		if (cv->CRSend==IdCRLF) {
			TempStr[TempLen++] = 0x0a;
		}
		else if ((cv->CRSend ==IdCR) && cv->TelFlag && ! cv->TelBinSend) {
			TempStr[TempLen++] = 0;
		}
		else if (cv->CRSend == IdLF) {
			TempStr[TempLen-1] = 0x0a;
		}
		retval = TRUE;
	}
	else if (d==0x15/*ctrl-u*/ && cv->TelLineMode) {
		// Move to top of line (CHA "\033[G") and erase line (EL "\033[K")
		memcpy(TempStr, "\033[G\033[K", 6);
		TempLen += 6;
		retval = TRUE;
	}
	*StrLen = TempLen;
	return retval;
}

/**
 * 出力用文字列を作成する
 *
 *	@retval	消費した文字数
 */
typedef struct {
	int KanjiCode;		// [in]出力文字コード(sjis,jisなど)
	BOOL (*ControlOut)(PComVar cv, unsigned int u32, BOOL check_only, char *TempStr, size_t *StrLen);
	// stateを保存する必要がある文字コードで使用
	BOOL JIS7Katakana;	// [in](Kanji JIS)kana
	int SendCode;		// [in,out](Kanji JIS)直前の送信コード Ascii/Kana/Kanji
	BOOL KanjiFlag;		// [in,out](MBCS)直前の1byteが漢字だったか?(2byte文字だったか?)
	BYTE KanjiFirst;	// [in,out](MBCS)直前の1byte
} OutputCharState;

/**
 *	unicode(UTF-16)からunicode(UTF-32)を1文字取り出して
 *	出力データ(TempStr)を作成する
 */
static size_t MakeOutputString(PComVar cv, OutputCharState *states,
							   const wchar_t *B, int C,
							   char *TempStr, int *TempLen_)
{
	BOOL (*ControlOut)(PComVar cv, unsigned int u32, BOOL check_only, char *TempStr, size_t *StrLen)
		= states->ControlOut;
//
	int TempLen = 0;
	size_t TempLen2;
	size_t output_char_count;	// 消費した文字数

	// UTF-32 を1文字取り出す
	unsigned int u32;
	size_t u16_len = UTF16ToUTF32(B, C, &u32);
	if (u16_len == 0) {
		// デコードできない? あり得ないのでは?
		assert(FALSE);
		u32 = '?';
		u16_len = 1;
	}
	output_char_count = u16_len;

	// 各種シフト状態を通常に戻す
	if (u32 < 0x100 || ControlOut(cv, u32, TRUE, NULL, NULL)) {
		if (cv->Language == IdJapanese && states->KanjiCode == IdJIS) {
			// 今のところ、日本語,JISしかない
			if (cv->SendCode == IdKanji) {
				// 漢字ではないので、漢字OUT
				TempStr[TempLen++] = 0x1B;
				TempStr[TempLen++] = '(';
				switch (cv->KanjiOut) {
				case IdKanjiOutJ:
					TempStr[TempLen++] = 'J';
					break;
				case IdKanjiOutH:
					TempStr[TempLen++] = 'H';
					break;
				default:
					TempStr[TempLen++] = 'B';
				}
			}

			if (states->JIS7Katakana == 1) {
				if (cv->SendCode == IdKatakana) {
					TempStr[TempLen++] = SO;
				}
			}

			states->SendCode = IdASCII;
		}
	}

	// 1文字処理する
	if (ControlOut(cv, u32, FALSE, TempStr, &TempLen2)) {
		// 特別な文字を処理した
		TempLen += TempLen2;
		output_char_count = 1;
	} else if (cv->Language == IdUtf8 ||
			   (cv->Language == IdJapanese && states->KanjiCode == IdUTF8) ||
			   (cv->Language == IdKorean && states->KanjiCode == IdUTF8))
	{
		// UTF-8 で出力
		size_t utf8_len = sizeof(TempStr);
		utf8_len = UTF32ToUTF8(u32, TempStr, utf8_len);
		TempLen += utf8_len;
	} else if (cv->Language == IdJapanese && *cv->CodePage == 932) {
		// 日本語
		// まず CP932(SJIS) に変換してから出力
		char mb_char[2];
		size_t mb_len = sizeof(mb_char);
		mb_len = UTF32ToMBCP(u32, 932, mb_char, mb_len);
		if (mb_len == 0) {
			// SJISに変換できない
			TempStr[TempLen++] = '?';
		} else {
			switch (states->KanjiCode) {
			case IdEUC:
				// TODO 半角カナ
				if (mb_len == 1) {
					TempStr[TempLen++] = mb_char[0];
				} else {
					WORD K;
					K = (((WORD)(unsigned char)mb_char[0]) << 8) +
						(WORD)(unsigned char)mb_char[1];
					K = SJIS2EUC(K);
					TempStr[TempLen++] = HIBYTE(K);
					TempStr[TempLen++] = LOBYTE(K);
				}
				break;
			case IdJIS:
				if (u32 < 0x100) {
					// ASCII
					TempStr[TempLen++] = mb_char[0];
					states->SendCode = IdASCII;
				} else if (IsHalfWidthKatakana(u32)) {
					// 半角カタカナ
					if (states->JIS7Katakana==1) {
						if (cv->SendCode != IdKatakana) {
							TempStr[TempLen++] = SI;
						}
						TempStr[TempLen++] = mb_char[0] & 0x7f;
					} else {
						TempStr[TempLen++] = mb_char[0];
					}
					states->SendCode = IdKatakana;
				} else {
					// 漢字
					WORD K;
					K = (((WORD)(unsigned char)mb_char[0]) << 8) +
						(WORD)(unsigned char)mb_char[1];
					K = SJIS2JIS(K);
					if (states->SendCode != IdKanji) {
						// 漢字IN
						TempStr[TempLen++] = 0x1B;
						TempStr[TempLen++] = '$';
						if (cv->KanjiIn == IdKanjiInB) {
							TempStr[TempLen++] = 'B';
						}
						else {
							TempStr[TempLen++] = '@';
						}
						states->SendCode = IdKanji;
					}
					TempStr[TempLen++] = HIBYTE(K);
					TempStr[TempLen++] = LOBYTE(K);
				}
				break;
			case IdSJIS:
				if (mb_len == 1) {
					TempStr[TempLen++] = mb_char[0];
				} else {
					TempStr[TempLen++] = mb_char[0];
					TempStr[TempLen++] = mb_char[1];
				}
				break;
			default:
				assert(FALSE);
				break;
			}
		}
	} else if (cv->Language == IdRussian) {
		/* まずCP1251に変換して出力 */
		char mb_char[2];
		size_t mb_len = sizeof(mb_char);
		BYTE b;
		mb_len = UTF32ToMBCP(u32, 1251, mb_char, mb_len);
		if (mb_len != 1) {
			b = '?';
		} else {
			b = RussConv(IdWindows, cv->RussHost, mb_char[0]);
		}
		TempStr[TempLen++] = b;
	} else if (cv->Language == IdKorean && *cv->CodePage == 51949) {
		/* CP51949に変換して出力 */
		char mb_char[2];
		size_t mb_len = sizeof(mb_char);
		mb_len = UTF32ToMBCP(u32, 51949, mb_char, mb_len);
		if (mb_len == 0) {
			TempStr[TempLen++] = '?';
		}
		else if (mb_len == 1) {
			TempStr[TempLen++] = mb_char[0];
		} else  {
			TempStr[TempLen++] = mb_char[0];
			TempStr[TempLen++] = mb_char[1];
		}
	} else if (cv->Language == IdEnglish) {
		TempStr[TempLen++] = u32;
	} else {
		// CodePageで変換
		char mb_char[2];
		size_t mb_len = sizeof(mb_char);
		mb_len = UTF32ToMBCP(u32, *cv->CodePage, mb_char, mb_len);
		if (mb_len == 0) {
			TempStr[TempLen++] = '?';
		}
		else if (mb_len == 1) {
			TempStr[TempLen++] = mb_char[0];
		} else  {
			TempStr[TempLen++] = mb_char[0];
			TempStr[TempLen++] = mb_char[1];
		}
	}

	*TempLen_ = TempLen;
	return output_char_count;
}


/**
 * CommTextOut() の wchar_t 版
 *
 *	@retval		出力文字数(wchar_t単位)
 */
int WINAPI CommTextOutW(PComVar cv, const wchar_t *B, int C)
{
	char TempStr[12];
	BOOL Full = FALSE;
	int i = 0;
	while (! Full && (i < C)) {
		// 出力用データを作成
		int TempLen = 0;
		size_t output_char_count;	// 消費した文字数
		OutputCharState state;
		state.KanjiCode = cv->KanjiCodeSend;
		state.ControlOut = OutControl;
		state.SendCode = cv->SendCode;
		state.JIS7Katakana = cv->JIS7KatakanaSend;
		output_char_count = MakeOutputString(cv, &state, &B[i], C-i, TempStr, &TempLen);

		// データを出力バッファへ
		if (WriteOutBuff(cv, TempStr, TempLen)) {
			i += output_char_count;		// output_char_count 文字数 処理した
			// 漢字の状態を保存する
			cv->SendCode = state.SendCode;
		} else {
			Full = TRUE;
		}
	} // end of "while {}"
	_CrtCheckMemory();
	return i;
}

/**
 * CommTextEcho() の wchar_t 版
 *
 *	@retval		出力文字数(wchar_t単位)
 */
int WINAPI CommTextEchoW(PComVar cv, const wchar_t *B, int C)
{
	char TempStr[12];
	BOOL Full = FALSE;
	int i = 0;
	while (! Full && (i < C)) {
		// 出力用データを作成
		int TempLen = 0;
		size_t output_char_count;	// 消費した文字数
		OutputCharState state;
		state.KanjiCode = cv->KanjiCodeEcho;
		state.ControlOut = ControlEcho;
		state.SendCode = cv->EchoCode;
		state.JIS7Katakana = cv->JIS7KatakanaEcho;
		output_char_count = MakeOutputString(cv, &state, &B[i], C-i, TempStr, &TempLen);

		// データを出力バッファへ
		if (WriteInBuff(cv, TempStr, TempLen)) {
			i += output_char_count;		// output_char_count 文字数 処理した
			// 漢字の状態を保存する
			cv->EchoCode = state.SendCode;
		} else {
			Full = TRUE;
		}
	} // end of "while {}"
	_CrtCheckMemory();
	return i;
}

int WINAPI CommBinaryEcho(PComVar cv, PCHAR B, int C)
{
	int a, i, Len;
	char d[3];

	if ( ! cv->Ready )
		return C;

	PackInBuff(cv);

	i = 0;
	a = 1;
	while ((a>0) && (i<C)) {
		Len = 0;

		d[Len] = B[i];
		Len++;

		if ( cv->TelFlag && (B[i]=='\x0d') &&
		     ! cv->TelBinSend ) {
			d[Len] = 0x00;
			Len++;
		}

		if ( cv->TelFlag && (B[i]=='\xff') ) {
			d[Len] = '\xff';
			Len++;
		}

		if (WriteInBuff(cv, d, Len)) {
			a = 1;
			i++;
		} else {
			a = 0;
		}
	}
	return i;
}

static int WINAPI TextEchoMBCS(PComVar cv, PCHAR B, int C)
{
	int i, TempLen;
	WORD K;
	char TempStr[12];
	int EchoCodeNew;
	BYTE d;
	BOOL Full, KanjiFlagNew;

	Full = FALSE;
	i = 0;
	while (! Full && (i < C)) {
		TempLen = 0;
		d = (BYTE)B[i];
		EchoCodeNew = cv->EchoCode;
		KanjiFlagNew = FALSE;

		if (cv->EchoKanjiFlag) {
			EchoCodeNew = IdKanji;

			K = (cv->EchoKanjiFirst << 8) + d;

			// UTF-8への変換を行う。1〜3バイトまでの対応なので注意。
			if (cv->KanjiCodeEcho == IdUTF8 || cv->Language==IdUtf8) {
				TempLen += OutputTextUTF8(K, TempStr, cv);
			}
			else {
				switch (cv->Language) {
				  case IdJapanese:
					switch (cv->KanjiCodeEcho) {
					  case IdEUC:
						K = SJIS2EUC(K);
						break;
					  case IdJIS:
						K = SJIS2JIS(K);
						if ((cv->EchoCode==IdKatakana) &&
						    (cv->JIS7KatakanaEcho==1)) {
							TempStr[TempLen++] = SI;
						}
						break;
					  case IdSJIS:
						/* nothing to do */
						break;
					}
					break;
				  case IdKorean:
					break;
				}
				TempStr[TempLen++] = HIBYTE(K);
				TempStr[TempLen++] = LOBYTE(K);
			}
		}
		else if (__isleadbyte_l(d, cv)) {
			KanjiFlagNew = TRUE;
			cv->EchoKanjiFirst = d;
			EchoCodeNew = IdKanji;

			if (cv->Language == IdJapanese) {
				if ((cv->EchoCode!=IdKanji) && (cv->KanjiCodeEcho==IdJIS)) {
					TempStr[0] = 0x1B;
					TempStr[1] = '$';
					if (cv->KanjiIn == IdKanjiInB) {
						TempStr[2] = 'B';
					}
					else {
						TempStr[2] = '@';
					}
					TempLen = 3;
				}
			}
		}
		else {
			if (cv->Language == IdJapanese) {
				if ((cv->EchoCode==IdKanji) && (cv->KanjiCodeEcho==IdJIS)) {
					TempStr[0] = 0x1B;
					TempStr[1] = '(';
					switch (cv->KanjiOut) {
					  case IdKanjiOutJ:
						TempStr[2] = 'J';
						break;
					  case IdKanjiOutH:
						TempStr[2] = 'H';
						break;
					  default:
						TempStr[2] = 'B';
					}
					TempLen = 3;
				}

				if ((0xa0<d) && (d<0xe0)) {
					EchoCodeNew = IdKatakana;
					if ((cv->EchoCode!=IdKatakana) &&
					    (cv->KanjiCodeEcho==IdJIS) &&
					    (cv->JIS7KatakanaEcho==1)) {
						TempStr[TempLen++] = SO;
					}
				}
				else {
					EchoCodeNew = IdASCII;
					if ((cv->EchoCode==IdKatakana) &&
					    (cv->KanjiCodeEcho==IdJIS) &&
					    (cv->JIS7KatakanaEcho==1)) {
						TempStr[TempLen++] = SI;
					}
				}
			}

			if (d==CR) {
				TempStr[TempLen++] = 0x0d;
				if (cv->CRSend==IdCRLF) {
					TempStr[TempLen++] = 0x0a;
				}
				else if ((cv->CRSend==IdCR) &&
				          cv->TelFlag && ! cv->TelBinSend) {
					TempStr[TempLen++] = 0;
				}
				else if (cv->CRSend == IdLF) {
					TempStr[TempLen-1] = 0x0a;
				}
			}
			else if (d==0x15) { // Ctrl-U
				if (cv->TelLineMode) {
					// Move to top of line (CHA "\033[G") and erase line (EL "\033[K")
					strncpy_s(TempStr, sizeof(TempStr), "\033[G\033[K", _TRUNCATE);
					TempLen += 6;
				}
				else {
					TempStr[TempLen++] = d;
				}
			}
			else if ((d>=0x80) && (cv->KanjiCodeEcho==IdUTF8 || cv->Language==IdUtf8)) {
				TempLen += OutputTextUTF8((WORD)d, TempStr, cv);
			}
			else if ((d>=0xa1) && (d<=0xe0) && (cv->Language == IdJapanese)) {
				/* Katakana */
				if (cv->KanjiCodeEcho==IdEUC) {
					TempStr[TempLen++] = (char)SS2;
				}
				if ((cv->KanjiCodeEcho==IdJIS) &&
					(cv->JIS7KatakanaEcho==1)) {
					TempStr[TempLen++] = d & 0x7f;
				}
				else {
					TempStr[TempLen++] = d;
				}
			}
			else {
				TempStr[TempLen++] = d;
				if (cv->TelFlag && (d==0xff)) {
					TempStr[TempLen++] = (char)0xff;
				}
			}
		} // if (cv->EchoKanjiFlag) else if ... else ... end

		if (WriteInBuff(cv, TempStr, TempLen)) {
			i++;
			cv->EchoCode = EchoCodeNew;
			cv->EchoKanjiFlag = KanjiFlagNew;
		} else {
			Full = FALSE;
		}

	} // end of "while {}"

	return i;
}

int WINAPI CommTextEcho(PComVar cv, PCHAR B, int C)
{
	int i, TempLen;
	char TempStr[11];
	BYTE d;
	BOOL Full;

	if ( ! cv->Ready ) {
		return C;
	}

	PackInBuff(cv);

	switch (cv->Language) {
	  case IdUtf8:
	  case IdJapanese:
	  case IdKorean:
		return TextEchoMBCS(cv,B,C);
		break;
	}

	Full = FALSE;
	i = 0;
	while (! Full && (i < C)) {
		TempLen = 0;
		d = (BYTE)B[i];

		switch (d) {
		  case CR:
			TempStr[TempLen] = 0x0d;
			TempLen++;
			if (cv->CRSend==IdCRLF) {
				TempStr[TempLen++] = 0x0a;
			}
			else if (cv->CRSend==IdCR && cv->TelFlag && ! cv->TelBinSend) {
				TempStr[TempLen++] = 0;
			}
			else if (cv->CRSend == IdLF) {
				TempStr[TempLen-1] = 0x0a;
			}
			break;

		  case 0x15: // Ctrl-U
			if (cv->TelLineMode) {
				// Move to top of line (CHA "\033[G") and erase line (EL "\033[K")
				strncpy_s(TempStr, sizeof(TempStr), "\033[G\033[K", _TRUNCATE);
				TempLen += 6;
			}
			else {
				TempStr[TempLen++] = d;
			}
			break;

		  default:
			if ((cv->Language==IdRussian) && (d>=128)) {
				d = RussConv(cv->RussClient,cv->RussHost,d);
			}
			TempStr[TempLen++] = d;
			if (cv->TelFlag && (d==0xff)) {
				TempStr[TempLen++] = (char)0xff;
			}
		}

		if(WriteInBuff(cv, TempStr,TempLen)) {
			i++;
		} else {
			Full = TRUE;
		}
	} // end of while {}

	return i;
}

/*
 *	@return		エラーが有る場合 FALSE
 *	@param[in]	BOOL first_instance
 */
static BOOL OpenSharedMemory(BOOL *first_instance_)
{
	int i;
	HMap = NULL;
	pm = NULL;
	for (i = 0; i < 100; i++) {
		char tmp[32];
		HANDLE hMap;
		BOOL first_instance;
		TMap *map;
		_snprintf_s(tmp, sizeof(tmp), _TRUNCATE, i == 0 ? "%s" : "%s_%d", TT_FILEMAPNAME, i);
		hMap = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
								 0, sizeof(TMap), tmp);
		if (hMap == NULL) {
			return FALSE;
		}

		first_instance = (GetLastError() != ERROR_ALREADY_EXISTS);

		map = (TMap *)MapViewOfFile(hMap,FILE_MAP_WRITE,0,0,0);
		if (map == NULL) {
			return FALSE;
		}

		if (first_instance ||
			(map->size_tmap == sizeof(TMap) &&
			 map->size_tttset == sizeof(TTTSet)))
		{
			map->size_tmap = sizeof(TMap);
			map->size_tttset = sizeof(TTTSet);
			HMap = hMap;
			pm = map;
			*first_instance_ = first_instance;
			return TRUE;
		}

		// next try
		UnmapViewOfFile(map);
		CloseHandle(hMap);
	}
	return FALSE;
}

BOOL WINAPI DllMain(HANDLE hInstance,
                    ULONG ul_reason_for_call,
                    LPVOID lpReserved)
{
	switch( ul_reason_for_call ) {
		case DLL_THREAD_ATTACH:
			/* do thread initialization */
			break;
		case DLL_THREAD_DETACH:
			/* do thread cleanup */
			break;
		case DLL_PROCESS_ATTACH:
			/* do process initialization */
			hInst = hInstance;
			if (OpenSharedMemory(&FirstInstance) == FALSE) {
				// dllロード失敗、teratermが起動しない
				return FALSE;
			}
			WinCompatInit();
			break;
		case DLL_PROCESS_DETACH:
			/* do process cleanup */
			UnmapViewOfFile(pm);
			CloseHandle(HMap);
			break;
	}
	return TRUE;
}
