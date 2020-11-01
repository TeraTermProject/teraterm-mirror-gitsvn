/*
 * Copyright (C) 1994-1998 T. Teranishi
 * (C) 2006-2020 TeraTerm Project
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

/* useful routines */

#pragma once

#include "i18n.h"

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(DllExport)
#define DllExport __declspec(dllexport)
#endif

BOOL GetFileNamePos(const char *PathName, int *DirLen, int *FNPos);
DllExport BOOL ExtractFileName(PCHAR PathName, PCHAR FileName, int destlen);
DllExport BOOL ExtractDirName(PCHAR PathName, PCHAR DirName);
void FitFileName(PCHAR FileName, int destlen, const char *DefExt);
void AppendSlash(PCHAR Path, int destlen);
void AppendSlashW(wchar_t *Path, size_t destlen);
void DeleteSlash(PCHAR Path);
void Str2Hex(PCHAR Str, PCHAR Hex, int Len, int MaxHexLen, BOOL ConvSP);
BYTE ConvHexChar(BYTE b);
int Hex2Str(PCHAR Hex, PCHAR Str, int MaxLen);
BOOL DoesFileExist(PCHAR FName);
BOOL DoesFolderExist(PCHAR FName);
long GetFSize(PCHAR FName);
unsigned long long GetFSize64H(HANDLE hFile);
unsigned long long GetFSize64W(const wchar_t *FName);
unsigned long long GetFSize64A(const char *FName);
long GetFMtime(PCHAR FName);
BOOL SetFMtime(PCHAR FName, DWORD mtime);
void uint2str(UINT i, PCHAR Str, int destlen, int len);
#ifdef WIN32
void QuoteFName(PCHAR FName);
#endif
int isInvalidFileNameChar(const char *FName);
#define deleteInvalidFileNameChar(name) replaceInvalidFileNameChar(name, 0)
DllExport void replaceInvalidFileNameChar(PCHAR FName, unsigned char c);
int isInvalidStrftimeChar(PCHAR FName);
void deleteInvalidStrftimeChar(PCHAR FName);
void ParseStrftimeFileName(PCHAR FName, int destlen);
void ConvFName(const char *HomeDir, PCHAR Temp, int templen, const char *DefExt, PCHAR FName, int destlen);
void RestoreNewLine(PCHAR Text);
size_t RestoreNewLineW(wchar_t *Text);
BOOL GetNthString(PCHAR Source, int Nth, int Size, PCHAR Dest);
void GetNthNum(PCHAR Source, int Nth, int far *Num);
int GetNthNum2(PCHAR Source, int Nth, int defval);
void GetDownloadFolder(char *dest, int destlen);
void GetHomeDir(HINSTANCE hInst, char *HomeDir, size_t HomeDirLen);
void GetDefaultFName(const char *home, const char *file, char *dest, int destlen);
void GetDefaultSetupFName(const char *home, char *dest, int destlen);
void GetUILanguageFile(char *buf, int buflen);
void GetUILanguageFileFull(const char *HomeDir, const char *UILanguageFileRel,
						   char *UILanguageFileFull, size_t UILanguageFileFullLen);
void GetOnOffEntryInifile(char *entry, char *buf, int buflen);
void get_lang_msg(const char *key, PCHAR buf, int buf_len, const char *def, const char *iniFile);
void get_lang_msgW(const char *key, wchar_t *buf, int buf_len, const wchar_t *def, const char *iniFile);
int get_lang_font(const char *key, HWND dlg, PLOGFONT logfont, HFONT *font, const char *iniFile);
DllExport BOOL doSelectFolder(HWND hWnd, char *path, int pathlen, const char *def, const char *msg);
BOOL doSelectFolderW(HWND hWnd, wchar_t *path, int pathlen, const wchar_t *def, const wchar_t *msg);
DllExport void OutputDebugPrintf(const char *fmt, ...);
void OutputDebugPrintfW(const wchar_t *fmt, ...);
void OutputDebugHexDump(const void *data, size_t len);
DllExport DWORD get_OPENFILENAME_SIZEA();
DllExport DWORD get_OPENFILENAME_SIZEW();
DllExport BOOL IsWindows95();
DllExport BOOL IsWindowsMe();
DllExport BOOL IsWindowsNT4();
DllExport BOOL IsWindowsNTKernel();
DllExport BOOL IsWindows2000();
DllExport BOOL IsWindows2000OrLater();
DllExport BOOL IsWindowsVistaOrLater();
DllExport BOOL IsWindows7OrLater();
DllExport BOOL HasMultiMonitorSupport();
DllExport BOOL HasGetAdaptersAddresses();
DllExport BOOL HasDnsQuery();
DllExport BOOL HasBalloonTipSupport();
int KanjiCodeTranslate(int lang, int kcode);
DllExport char *mctimelocal(char *format, BOOL utc_flag);
char *strelapsed(DWORD start_time);

void b64encode(PCHAR dst, int dsize, PCHAR src, int len);
DllExport int b64decode(PCHAR dst, int dsize, PCHAR src);

DllExport PCHAR PASCAL GetParam(PCHAR buff, int size, PCHAR param);
DllExport void PASCAL DequoteParam(PCHAR dest, int dest_len, PCHAR src);
void PASCAL DeleteComment(PCHAR dest, int dest_size, PCHAR src);

void split_buffer(char *buffer, int delimiter, char **head, char **body);
BOOL GetPositionOnWindow(
	HWND hWnd, const POINT *point,
	BOOL *InWindow, BOOL *InClient, BOOL *InTitleBar);
DllExport void GetMessageboxFont(LOGFONTA *logfont);
void GetDesktopRect(HWND hWnd, RECT *rect);
void CenterWindow(HWND hWnd, HWND hWndParent);
void MoveWindowToDisplay(HWND hWnd);

#define CheckFlag(var, flag)	(((var) & (flag)) != 0)

int SetDlgTexts(HWND hDlgWnd, const DlgTextInfo *infos, int infoCount, const char *UILanguageFile);
void SetDlgMenuTexts(HMENU hMenu, const DlgTextInfo *infos, int infoCount, const char *UILanguageFile);
int GetMonitorDpiFromWindow(HWND hWnd);

#define	get_OPENFILENAME_SIZE() get_OPENFILENAME_SIZEA()

#if defined(_UNICODE)
#define	get_lang_msgT(p1, p2, p3, p4, p5) get_lang_msgW(p1, p2, p3, p4, p5)
#else
#define	get_lang_msgT(p1, p2, p3, p4, p5) get_lang_msg(p1, p2, p3, p4, p5)
#endif

/*
 * シリアルポート関連の設定定義
 */
enum serial_port_conf {
	COM_DATABIT,
	COM_PARITY,
	COM_STOPBIT,
	COM_FLOWCTRL,
};

/*
 *	ttlib_static
 */
typedef struct {
	const char *section;			// セクション名
	const char *title_key;			// タイトル(NULLのとき、title_default を常に使用)
	const wchar_t *title_default;	//   lng ファイルに見つからなかったとき使用
	const char *message_key;		// メッセージ
	const wchar_t *message_default;	//   lng ファイルに見つからなかったとき使用
} TTMessageBoxInfoW;

int TTMessageBoxW(HWND hWnd, const TTMessageBoxInfoW *info, UINT uType, const char *UILanguageFile, ...);
wchar_t *TTGetLangStrW(const char *section, const char *key, const wchar_t *def, const char *UILanguageFile);
wchar_t *GetClipboardTextW(HWND hWnd, BOOL empty);
char *GetClipboardTextA(HWND hWnd, BOOL empty);
BOOL CBSetTextW(HWND hWnd, const wchar_t *str_w, size_t str_len);
void TTInsertMenuItemA(HMENU hMenu, UINT targetItemID, UINT flags, UINT newItemID, const char *text, BOOL before);
BOOL IsTextW(const wchar_t *str, size_t len);
wchar_t *NormalizeLineBreakCR(const wchar_t *src, size_t *len);
wchar_t *NormalizeLineBreakCRLF(const wchar_t *src_);

#ifdef __cplusplus
}
#endif
