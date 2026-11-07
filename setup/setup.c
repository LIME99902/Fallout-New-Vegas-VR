/*
 * FNVVRMC-Setup - one window that installs Fallout: New Vegas VR (FNVVRMC) and everything it needs.
 * Copyright (C) LIME99902 and contributors. SPDX-License-Identifier: GPL-3.0-or-later
 *
 * What it does, in order:
 *   1. finds your Fallout: New Vegas folder (Steam / registry, or asks)
 *   2. xNVSE ............ downloaded from its official GitHub releases and installed
 *   3. Nexus-only parts . (4GB Patch, JIP LN, JohnnyGuitar, ShowOff, kNVSE, NVTF) - opens each one's Nexus download page;
 *                         you click download, this window sees the file arrive in your Downloads folder and installs it.
 *                         Anything already installed is skipped.
 *   4. the 4GB patch is applied (its patcher is run once)
 *   5. FNVVRMC .......... latest release downloaded from GitHub and installed
 *   6. checks SteamVR, tells you how to start
 * Nothing is redistributed: every file comes from its author's own page. A log is written next to this program.
 */
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <winhttp.h>
#include <shlobj.h>
#include <shellapi.h>
#include <commdlg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <conio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <ctype.h>

#define SETUP_VERSION L"1.0"
static HANDLE g_con; static FILE *g_log;
static bool g_testMode = false;
static wchar_t g_game[MAX_PATH], g_tmp[MAX_PATH], g_dl[MAX_PATH], g_self[MAX_PATH];

/* ---------- output ---------- */
enum { C_NORM = 7, C_HEAD = 11, C_OK = 10, C_WARN = 14, C_ERR = 12, C_DIM = 8 };
static void Out(int col, const wchar_t *fmt, ...)
{
    wchar_t buf[2048]; va_list ap; va_start(ap, fmt); _vsnwprintf(buf, 2047, fmt, ap); va_end(ap); buf[2047] = 0;
    SetConsoleTextAttribute(g_con, (WORD)col); DWORD w; WriteConsoleW(g_con, buf, (DWORD)wcslen(buf), &w, NULL); SetConsoleTextAttribute(g_con, C_NORM);
    if (g_log) { char u[4096]; WideCharToMultiByte(CP_UTF8, 0, buf, -1, u, sizeof u, NULL, NULL); fputs(u, g_log); fflush(g_log); }
}
static void Pause(void) { Out(C_DIM, L"\nPress any key to close this window."); _getch(); }
static bool Exists(const wchar_t *p) { return GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES; }
static void Join(wchar_t *out, const wchar_t *a, const wchar_t *b) { _snwprintf(out, MAX_PATH - 1, L"%s\\%s", a, b); out[MAX_PATH - 1] = 0; }
static bool InGame(const wchar_t *rel) { wchar_t p[MAX_PATH]; Join(p, g_game, rel); return Exists(p); }
static void MakeDirs(const wchar_t *path)
{
    wchar_t p[MAX_PATH]; wcsncpy(p, path, MAX_PATH - 1); p[MAX_PATH - 1] = 0;
    for (wchar_t *c = p + 3; *c; c++) if (*c == L'\\') { *c = 0; CreateDirectoryW(p, NULL); *c = L'\\'; }
    CreateDirectoryW(p, NULL);
}
static void RemoveTree(const wchar_t *dir)
{
    wchar_t pat[MAX_PATH]; Join(pat, dir, L"*"); WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW(pat, &fd);
    if (h != INVALID_HANDLE_VALUE) { do { if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) continue;
        wchar_t p[MAX_PATH]; Join(p, dir, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) RemoveTree(p); else { SetFileAttributesW(p, FILE_ATTRIBUTE_NORMAL); DeleteFileW(p); }
    } while (FindNextFileW(h, &fd)); FindClose(h); }
    RemoveDirectoryW(dir);
}

/* ---------- web ---------- */
static bool HttpGetWinHttp(const wchar_t *url, const wchar_t *toFile, char **toMem, DWORD *memLen, bool quiet)
{
    URL_COMPONENTSW uc; memset(&uc, 0, sizeof uc); uc.dwStructSize = sizeof uc;
    wchar_t host[256], path[2048]; uc.lpszHostName = host; uc.dwHostNameLength = 256; uc.lpszUrlPath = path; uc.dwUrlPathLength = 2048;
    wchar_t extra[1024]; uc.lpszExtraInfo = extra; uc.dwExtraInfoLength = 1024;
    if (!WinHttpCrackUrl(url, 0, 0, &uc)) { Out(C_DIM, L"   (web: bad address, error %lu)\n", GetLastError()); return false; }
    wcsncat(path, extra, 2047 - wcslen(path));
    HINTERNET s = WinHttpOpen(L"FNVVRMC-Setup/" SETUP_VERSION, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!s) s = WinHttpOpen(L"FNVVRMC-Setup/" SETUP_VERSION, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!s) { Out(C_DIM, L"   (web: WinHttpOpen error %lu)\n", GetLastError()); return false; }
    bool ok = false; HINTERNET c = WinHttpConnect(s, host, uc.nPort, 0), r = NULL;
    if (c) r = WinHttpOpenRequest(c, L"GET", path, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
    if (r) {
        const wchar_t *hdr = L"Accept: */*\r\n";
        if (!WinHttpSendRequest(r, hdr, (DWORD)-1, NULL, 0, 0, 0) || !WinHttpReceiveResponse(r, NULL)) Out(C_DIM, L"   (web: connection to %s failed, error %lu)\n", host, GetLastError());
        else {
            DWORD code = 0, sz = sizeof code; WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &code, &sz, NULL);
            DWORD total = 0; sz = sizeof total; WinHttpQueryHeaders(r, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER, NULL, &total, &sz, NULL);
            if (code == 200) {
                HANDLE f = INVALID_HANDLE_VALUE; char *mem = NULL; DWORD cap = 0, len = 0;
                if (toFile) f = CreateFileW(toFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
                ok = !toFile || f != INVALID_HANDLE_VALUE;
                DWORD got = 0, lastPct = 999; char buf[65536];
                while (ok) {
                    DWORD n = 0; if (!WinHttpReadData(r, buf, sizeof buf, &n)) { ok = false; break; }
                    if (!n) break;
                    if (toFile) { DWORD w; if (!WriteFile(f, buf, n, &w, NULL)) { ok = false; break; } }
                    else { if (len + n + 1 > cap) { cap = (len + n + 1) * 2; mem = (char *)realloc(mem, cap); } memcpy(mem + len, buf, n); len += n; }
                    got += n;
                    if (!quiet && total) { DWORD pct = (DWORD)((unsigned long long)got * 100 / total); if (pct / 10 != lastPct / 10) { lastPct = pct; Out(C_DIM, L" %u%%", pct); } }
                }
                if (f != INVALID_HANDLE_VALUE) CloseHandle(f);
                if (toMem) { if (mem) mem[len] = 0; *toMem = mem; if (memLen) *memLen = len; } else free(mem);
                if (!quiet && total) Out(C_DIM, L"\n");
            } else Out(C_DIM, L"   (web: %s answered %lu)\n", host, code);
        }
        WinHttpCloseHandle(r);
    }
    if (c) WinHttpCloseHandle(c);
    WinHttpCloseHandle(s);
    return ok;
}
/* Windows' own curl.exe (Windows 10 1803 and later) when WinHTTP can't get through */
static bool Run(const wchar_t *cmd, const wchar_t *dir, DWORD *exitCode, bool hide);
static bool HttpGetCurl(const wchar_t *url, const wchar_t *toFile, char **toMem, DWORD *memLen)
{
    wchar_t sys[MAX_PATH], tmpf[MAX_PATH], cmd[4096]; GetSystemDirectoryW(sys, MAX_PATH);
    if (toFile) wcsncpy(tmpf, toFile, MAX_PATH - 1); else Join(tmpf, g_tmp, L"web.tmp"); tmpf[MAX_PATH - 1] = 0;
    _snwprintf(cmd, 4095, L"\"%s\\curl.exe\" -L -f -s -S --retry 2 -A FNVVRMC-Setup/" SETUP_VERSION L" -o \"%s\" \"%s\"", sys, tmpf, url); cmd[4095] = 0;
    DWORD ec = 1; if (!Run(cmd, g_tmp, &ec, true) || ec != 0) { Out(C_DIM, L"   (web: curl couldn't get it either, code %lu)\n", ec); return false; }
    if (toMem) {
        FILE *f = _wfopen(tmpf, L"rb"); if (!f) return false;
        fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
        char *m = (char *)malloc((size_t)n + 1); size_t rd = m ? fread(m, 1, (size_t)n, f) : 0; fclose(f); DeleteFileW(tmpf);
        if (!m) return false; m[rd] = 0; *toMem = m; if (memLen) *memLen = (DWORD)rd;
    }
    return true;
}
static bool HttpGet(const wchar_t *url, const wchar_t *toFile, char **toMem, DWORD *memLen, bool quiet)
{
    if (HttpGetWinHttp(url, toFile, toMem, memLen, quiet)) return true;
    if (toMem && *toMem) { free(*toMem); *toMem = NULL; }
    if (!quiet) Out(C_DIM, L"   trying again with Windows' curl...\n");
    return HttpGetCurl(url, toFile, toMem, memLen);
}
/* the newest release of a GitHub project: the download link of the first file whose name ends with 'suffix' and contains 'need' */
static bool GitHubAsset(const char *repo, const char *need, const char *suffix, wchar_t *url, wchar_t *name)
{
    wchar_t api[512]; _snwprintf(api, 511, L"https://api.github.com/repos/%hs/releases?per_page=10", repo); api[511] = 0;
    char *js = NULL; DWORD n = 0;
    if (!HttpGet(api, NULL, &js, &n, true) || !js) return false;
    bool ok = false; const char *p = js;
    while ((p = strstr(p, "\"browser_download_url\""))) {
        p = strchr(p + 22, '"'); if (!p) break; p++;
        const char *e = strchr(p, '"'); if (!e) break;
        char u[1024]; size_t L = (size_t)(e - p); if (L >= sizeof u) L = sizeof u - 1; memcpy(u, p, L); u[L] = 0;
        const char *fn = strrchr(u, '/'); fn = fn ? fn + 1 : u;
        size_t fl = strlen(fn), sl = strlen(suffix);
        char lfn[260]; size_t q = 0; for (; fn[q] && q < sizeof lfn - 1; q++) lfn[q] = (char)tolower((unsigned char)fn[q]); lfn[q] = 0;
        bool junk = strstr(lfn, "pdb") || strstr(lfn, "source") || strstr(lfn, "debug") || strstr(lfn, "symbols");
        if (fl > sl && !_stricmp(fn + fl - sl, suffix) && (!need || strstr(fn, need)) && !junk) {
            MultiByteToWideChar(CP_UTF8, 0, u, -1, url, 1024); MultiByteToWideChar(CP_UTF8, 0, fn, -1, name, MAX_PATH);
            for (wchar_t *c = name; *c; c++) if (*c == L'%') *c = L'_';
            ok = true; break;
        }
        p = e;
    }
    free(js);
    return ok;
}

/* the same from the release web page itself (when the API is blocked or rate-limited) */
static bool GitHubAssetPage(const char *repo, const char *need, const char *suffix, wchar_t *url, wchar_t *name)
{
    wchar_t u[512]; _snwprintf(u, 511, L"https://github.com/%hs/releases/latest", repo); u[511] = 0;
    char *html = NULL; if (!HttpGet(u, NULL, &html, NULL, true) || !html) return false;
    char *ea = strstr(html, "releases/expanded_assets/"); char tag[128] = "";
    if (ea) { ea += 25; size_t i = 0; while (ea[i] && ea[i] != '"' && ea[i] != '?' && i < sizeof tag - 1) { tag[i] = ea[i]; i++; } tag[i] = 0; }
    free(html); if (!tag[0]) return false;
    _snwprintf(u, 511, L"https://github.com/%hs/releases/expanded_assets/%hs", repo, tag); u[511] = 0;
    if (!HttpGet(u, NULL, &html, NULL, true) || !html) return false;
    bool ok = false; const char *p = html;
    while ((p = strstr(p, "/releases/download/"))) {
        const char *st = p; while (st > html && *st != '"') st--; st++;
        const char *e = strchr(p, '"'); if (!e) break;
        char rel[600]; size_t L = (size_t)(e - st); if (L >= sizeof rel) L = sizeof rel - 1; memcpy(rel, st, L); rel[L] = 0;
        const char *fn = strrchr(rel, '/'); fn = fn ? fn + 1 : rel; size_t fl = strlen(fn), sl = strlen(suffix);
        if (fl > sl && !_stricmp(fn + fl - sl, suffix) && (!need || strstr(fn, need))) {
            wchar_t wr[600]; MultiByteToWideChar(CP_UTF8, 0, rel, -1, wr, 600);
            _snwprintf(url, 1023, L"https://github.com%s", wr); url[1023] = 0; MultiByteToWideChar(CP_UTF8, 0, fn, -1, name, MAX_PATH); ok = true; break;
        }
        p = e;
    }
    free(html);
    return ok;
}
/* ---------- archives ---------- */
static bool Run(const wchar_t *cmd, const wchar_t *dir, DWORD *exitCode, bool hide)
{
    wchar_t c[4096]; wcsncpy(c, cmd, 4095); c[4095] = 0;
    STARTUPINFOW si; memset(&si, 0, sizeof si); si.cb = sizeof si; if (hide) { si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE; }
    PROCESS_INFORMATION pi;
    if (!CreateProcessW(NULL, c, NULL, NULL, FALSE, hide ? CREATE_NO_WINDOW : 0, NULL, dir, &si, &pi)) return false;
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD ec = 1; GetExitCodeProcess(pi.hProcess, &ec); if (exitCode) *exitCode = ec;
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    return true;
}
static bool Get7z(wchar_t *exe)
{
    Join(exe, g_tmp, L"7zr.exe");
    if (Exists(exe)) return true;
    Out(C_DIM, L"   getting 7-Zip's small unpacker (7zr.exe, from 7-zip.org)...\n");
    return HttpGet(L"https://www.7-zip.org/a/7zr.exe", exe, NULL, NULL, true);
}
static bool Extract(const wchar_t *archive, const wchar_t *to)
{
    RemoveTree(to); MakeDirs(to);
    const wchar_t *ext = wcsrchr(archive, L'.'); wchar_t cmd[4096]; DWORD ec = 1;
    if (ext && !_wcsicmp(ext, L".7z")) {
        wchar_t z[MAX_PATH]; if (!Get7z(z)) { Out(C_ERR, L"   couldn't get 7zr.exe to unpack .7z files\n"); return false; }
        _snwprintf(cmd, 4095, L"\"%s\" x \"%s\" -o\"%s\" -y", z, archive, to);
    } else {   /* .zip / .rar: Windows' own tar (Windows 10 1803 and later) */
        wchar_t sys[MAX_PATH]; GetSystemDirectoryW(sys, MAX_PATH);
        _snwprintf(cmd, 4095, L"\"%s\\tar.exe\" -xf \"%s\" -C \"%s\"", sys, archive, to);
    }
    cmd[4095] = 0;
    return Run(cmd, g_tmp, &ec, true) && ec == 0;
}
/* copy what was unpacked into the game: anything under Data\ or nvse\ goes into the game's Data folder;
   loose files at the top (xNVSE's loader, the 4GB patcher) go next to the game when 'rootFiles' is set */
static int g_copied;
static void CopyInto(const wchar_t *dir, const wchar_t *rel, bool rootFiles)
{
    wchar_t pat[MAX_PATH]; Join(pat, dir, L"*"); WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) continue;
        wchar_t src[MAX_PATH], r2[MAX_PATH]; Join(src, dir, fd.cFileName);
        if (rel[0]) Join(r2, rel, fd.cFileName); else { wcsncpy(r2, fd.cFileName, MAX_PATH - 1); r2[MAX_PATH - 1] = 0; }
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) { CopyInto(src, r2, rootFiles); continue; }
        wchar_t lo[MAX_PATH]; wcsncpy(lo, r2, MAX_PATH - 1); lo[MAX_PATH - 1] = 0; _wcslwr(lo);
        wchar_t dst[MAX_PATH]; dst[0] = 0;
        const wchar_t *d = wcsstr(lo, L"data\\"); const wchar_t *nv = wcsstr(lo, L"nvse\\");
        if (d && (d == lo || d[-1] == L'\\')) Join(dst, g_game, r2 + (d - lo));
        else if (nv && (nv == lo || nv[-1] == L'\\')) { wchar_t t[MAX_PATH]; _snwprintf(t, MAX_PATH - 1, L"Data\\%s", r2 + (nv - lo)); t[MAX_PATH - 1] = 0; Join(dst, g_game, t); }
        else if (rootFiles && !wcschr(lo, L'\\')) { const wchar_t *e = wcsrchr(lo, L'.'); if (e && (!wcscmp(e, L".exe") || !wcscmp(e, L".dll"))) Join(dst, g_game, r2); }
        if (!dst[0]) continue;
        wchar_t dd[MAX_PATH]; wcsncpy(dd, dst, MAX_PATH - 1); dd[MAX_PATH - 1] = 0; wchar_t *sl = wcsrchr(dd, L'\\'); if (sl) { *sl = 0; MakeDirs(dd); }
        if (CopyFileW(src, dst, FALSE)) g_copied++; else Out(C_ERR, L"   couldn't write %s (error %lu)\n", dst, GetLastError());
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}
/* archives whose top level IS the Data folder (plugin .esp + meshes/textures/menus...): find that level (past any wrapper
   folder or FOMOD folder) and copy all of it into Data */
static bool LooksLikeData(const wchar_t *dir)
{
    static const wchar_t *const dn[] = { L"meshes", L"textures", L"menus", L"sound", L"nvse", L"config", L"uio", L"music", NULL };
    wchar_t pat[MAX_PATH]; Join(pat, dir, L"*"); WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW(pat, &fd); bool yes = false;
    if (h == INVALID_HANDLE_VALUE) return false;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) { for (int i = 0; dn[i]; i++) if (!_wcsicmp(fd.cFileName, dn[i])) yes = true; }
        else { const wchar_t *e = wcsrchr(fd.cFileName, L'.'); if (e && (!_wcsicmp(e, L".esp") || !_wcsicmp(e, L".esm") || !_wcsicmp(e, L".bsa"))) yes = true; }
    } while (!yes && FindNextFileW(h, &fd));
    FindClose(h);
    return yes;
}
static bool FindDataRoot(const wchar_t *dir, wchar_t *out, int depth)
{
    if (LooksLikeData(dir)) { wcsncpy(out, dir, MAX_PATH - 1); out[MAX_PATH - 1] = 0; return true; }
    if (depth > 3) return false;
    wchar_t pat[MAX_PATH]; Join(pat, dir, L"*"); WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW(pat, &fd); bool ok = false;
    if (h == INVALID_HANDLE_VALUE) return false;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || !wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..") || !_wcsicmp(fd.cFileName, L"fomod")) continue;
        wchar_t sub[MAX_PATH]; Join(sub, dir, fd.cFileName); ok = FindDataRoot(sub, out, depth + 1);
    } while (!ok && FindNextFileW(h, &fd));
    FindClose(h);
    return ok;
}
static void CopyAll(const wchar_t *from, const wchar_t *to)
{
    wchar_t pat[MAX_PATH]; Join(pat, from, L"*"); WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    MakeDirs(to);
    do {
        if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..") || !_wcsicmp(fd.cFileName, L"fomod")) continue;
        wchar_t s2[MAX_PATH], d2[MAX_PATH]; Join(s2, from, fd.cFileName); Join(d2, to, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) CopyAll(s2, d2);
        else if (CopyFileW(s2, d2, FALSE)) g_copied++; else Out(C_ERR, L"   couldn't write %s (error %lu)\n", d2, GetLastError());
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}
static bool InstallDataArchive(const wchar_t *archive)
{
    wchar_t to[MAX_PATH], root[MAX_PATH], data[MAX_PATH]; Join(to, g_tmp, L"unpack");
    if (!Extract(archive, to)) { Out(C_ERR, L"   couldn't unpack %s\n", archive); return false; }
    g_copied = 0;
    wchar_t dsub[MAX_PATH]; Join(dsub, to, L"Data");
    if (Exists(dsub)) CopyInto(to, L"", false);   /* (it already says Data\) */
    else if (FindDataRoot(to, root, 0)) { Join(data, g_game, L"Data"); CopyAll(root, data); }
    RemoveTree(to);
    return g_copied > 0;
}
/* switch a plugin (.esp/.esm) on in the game's load order (%LOCALAPPDATA%\FalloutNV\plugins.txt) */
static void EnablePlugin(const wchar_t *esp)
{
    if (g_testMode) { Out(C_DIM, L"   (test mode: would switch on %s in your load order)\n", esp); return; }
    PWSTR la = NULL; if (SHGetKnownFolderPath(&FOLDERID_LocalAppData, 0, NULL, &la) != S_OK) return;
    wchar_t dir[MAX_PATH], pt[MAX_PATH], lo[MAX_PATH]; Join(dir, la, L"FalloutNV"); CoTaskMemFree(la); MakeDirs(dir);
    Join(pt, dir, L"plugins.txt"); Join(lo, dir, L"loadorder.txt");
    char name[MAX_PATH]; WideCharToMultiByte(CP_UTF8, 0, esp, -1, name, sizeof name, NULL, NULL);
    const wchar_t *files[2] = { pt, lo };
    for (int i = 0; i < 2; i++) {
        bool exists = Exists(files[i]); if (i == 1 && !exists) continue;   /* (loadorder.txt only when a manager keeps one) */
        bool has = false;
        if (exists) { FILE *f = _wfopen(files[i], L"rb"); if (f) { char line[512];
            while (fgets(line, sizeof line, f)) { char *e = line + strlen(line); while (e > line && (e[-1] == '\n' || e[-1] == '\r' || e[-1] == ' ')) *--e = 0; char *b = line; if (*b == '*') b++; if (!_stricmp(b, name)) has = true; }
            fclose(f); } }
        if (has) continue;
        FILE *f = _wfopen(files[i], L"ab"); if (!f) continue;
        if (!exists) {   /* (a new list: the game and its DLC first) */
            static const char *const base[] = { "FalloutNV.esm", "DeadMoney.esm", "HonestHearts.esm", "OldWorldBlues.esm", "LonesomeRoad.esm", "GunRunnersArsenal.esm",
                "ClassicPack.esm", "MercenaryPack.esm", "TribalPack.esm", "CaravanPack.esm", NULL };
            for (int b = 0; base[b]; b++) { wchar_t w[MAX_PATH], rel[MAX_PATH]; MultiByteToWideChar(CP_UTF8, 0, base[b], -1, w, MAX_PATH); Join(rel, L"Data", w); if (InGame(rel)) fprintf(f, "%s\r\n", base[b]); }
        } else { fseek(f, 0, SEEK_END); long n = ftell(f); if (n > 0) { FILE *r = _wfopen(files[i], L"rb"); if (r) { fseek(r, -1, SEEK_END); int c = fgetc(r); fclose(r); if (c != '\n') fputs("\r\n", f); } } }
        fprintf(f, "%s\r\n", name); fclose(f);
    }
    Out(C_OK, L"   switched on in your load order: %s\n", esp);
}
static bool InstallArchive(const wchar_t *archive, bool rootFiles)
{
    wchar_t to[MAX_PATH]; Join(to, g_tmp, L"unpack");
    if (!Extract(archive, to)) { Out(C_ERR, L"   couldn't unpack %s\n", archive); return false; }
    g_copied = 0; CopyInto(to, L"", rootFiles); RemoveTree(to);
    return g_copied > 0;
}

/* ---------- the Downloads folder: wait for a Nexus file ("<name>-<mod id>-<version>.7z") ---------- */
static bool FindDownload(int modId, FILETIME after, bool anyAge, wchar_t *out)
{
    wchar_t pat[MAX_PATH], key[32]; Join(pat, g_dl, L"*"); _snwprintf(key, 31, L"-%d-", modId);
    WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW(pat, &fd); bool ok = false; FILETIME best = { 0, 0 };
    if (h == INVALID_HANDLE_VALUE) return false;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        const wchar_t *e = wcsrchr(fd.cFileName, L'.'); if (!e) continue;
        if (_wcsicmp(e, L".7z") && _wcsicmp(e, L".zip") && _wcsicmp(e, L".rar")) continue;
        if (!wcsstr(fd.cFileName, key)) continue;
        if (!anyAge && CompareFileTime(&fd.ftLastWriteTime, &after) < 0) continue;
        if (CompareFileTime(&fd.ftLastWriteTime, &best) > 0) { best = fd.ftLastWriteTime; Join(out, g_dl, fd.cFileName); ok = true; }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return ok;
}
static bool StableSize(const wchar_t *p)
{
    WIN32_FILE_ATTRIBUTE_DATA a, b;
    if (!GetFileAttributesExW(p, GetFileExInfoStandard, &a)) return false; Sleep(1500);
    if (!GetFileAttributesExW(p, GetFileExInfoStandard, &b)) return false;
    return a.nFileSizeLow == b.nFileSizeLow && a.nFileSizeHigh == b.nFileSizeHigh && (a.nFileSizeLow || a.nFileSizeHigh);
}
typedef struct { const wchar_t *name; int id; const wchar_t *check; const wchar_t *why; bool root; const wchar_t *esp; } NexusMod;   /* (esp: a Data-folder archive with this plugin to switch on) */
/* 0 installed, 1 skipped, -1 failed */
static int NexusStep(const NexusMod *m)
{
    Out(C_HEAD, L"\n%s\n", m->name); Out(C_DIM, L"   %s\n", m->why);
    if (m->check && InGame(m->check)) { Out(C_OK, L"   already installed - skipping\n"); if (m->esp && m->esp[0]) EnablePlugin(m->esp); return 0; }
    wchar_t file[MAX_PATH];
    FILETIME recent; { ULARGE_INTEGER t; FILETIME n; GetSystemTimeAsFileTime(&n); t.LowPart = n.dwLowDateTime; t.HighPart = n.dwHighDateTime;
        t.QuadPart -= 6ull * 3600ull * 10000000ull; recent.dwLowDateTime = t.LowPart; recent.dwHighDateTime = t.HighPart; }
    if (FindDownload(m->id, recent, false, file)) {   /* (only a download from the last few hours - an old copy may be an outdated version) */
        Out(C_NORM, L"   found it in your Downloads: %s\n", wcsrchr(file, L'\\') + 1);
    } else {
        wchar_t url[256]; _snwprintf(url, 255, L"https://www.nexusmods.com/newvegas/mods/%d?tab=files", m->id);
        Out(C_NORM, L"   Opening its Nexus page in your browser.\n");
        Out(C_WARN, L"   -> Click  MANUAL DOWNLOAD  on the main file (then  SLOW DOWNLOAD  if you're asked).\n");
        Out(C_DIM, L"      (A free Nexus account is enough. This window installs it as soon as it lands in your Downloads folder.)\n");
        Out(C_DIM, L"      Press S to skip, or B to browse for the file yourself.\n");
        FILETIME now; GetSystemTimeAsFileTime(&now);
        ShellExecuteW(NULL, L"open", url, NULL, NULL, SW_SHOWNORMAL);
        bool got = false; int dots = 0;
        for (;;) {
            if (FindDownload(m->id, now, false, file) && StableSize(file)) { got = true; break; }
            if (_kbhit()) { int k = _getch();
                if (k == 's' || k == 'S') { Out(C_WARN, L"\n   skipped - the mod may not work fully without it\n"); return 1; }
                if (k == 'b' || k == 'B') {
                    OPENFILENAMEW of; memset(&of, 0, sizeof of); file[0] = 0; of.lStructSize = sizeof of; of.lpstrFile = file; of.nMaxFile = MAX_PATH;
                    of.lpstrFilter = L"Mod archives\0*.7z;*.zip;*.rar\0All files\0*.*\0"; of.Flags = OFN_FILEMUSTEXIST;
                    if (GetOpenFileNameW(&of)) { got = true; break; }
                } }
            Sleep(1000); if (++dots % 5 == 0) Out(C_DIM, L".");
        }
        if (!got) return -1;
        Out(C_NORM, L"\n   downloaded: %s\n", wcsrchr(file, L'\\') ? wcsrchr(file, L'\\') + 1 : file);
    }
    if (!(m->esp ? InstallDataArchive(file) : InstallArchive(file, m->root))) { Out(C_ERR, L"   couldn't install it\n"); return -1; }
    if (m->esp && m->esp[0]) EnablePlugin(m->esp);
    Out(C_OK, L"   installed (%d files)\n", g_copied);
    return 0;
}

/* ---------- finding the game ---------- */
static bool GameHere(const wchar_t *dir) { wchar_t p[MAX_PATH]; Join(p, dir, L"FalloutNV.exe"); return Exists(p); }
static bool FindGame(void)
{
    HKEY k; DWORD sz = sizeof g_game;
    const wchar_t *keys[] = { L"SOFTWARE\\WOW6432Node\\Bethesda Softworks\\FalloutNV", L"SOFTWARE\\Bethesda Softworks\\FalloutNV" };
    for (int i = 0; i < 2; i++) if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, keys[i], 0, KEY_READ, &k) == ERROR_SUCCESS) {
        sz = sizeof g_game; if (RegQueryValueExW(k, L"Installed Path", NULL, NULL, (BYTE *)g_game, &sz) == ERROR_SUCCESS) {
            RegCloseKey(k); size_t L = wcslen(g_game); while (L && g_game[L - 1] == L'\\') g_game[--L] = 0;
            if (GameHere(g_game)) return true; }
        else RegCloseKey(k);
    }
    /* Steam libraries */
    wchar_t steam[MAX_PATH]; sz = sizeof steam;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", 0, KEY_READ, &k) == ERROR_SUCCESS) {
        if (RegQueryValueExW(k, L"SteamPath", NULL, NULL, (BYTE *)steam, &sz) == ERROR_SUCCESS) {
            for (wchar_t *c = steam; *c; c++) if (*c == L'/') *c = L'\\';
            wchar_t vdf[MAX_PATH]; Join(vdf, steam, L"steamapps\\libraryfolders.vdf");
            FILE *f = _wfopen(vdf, L"rb");
            if (f) { char line[1024];
                while (fgets(line, sizeof line, f)) { char *p = strstr(line, "\"path\""); if (!p) continue;
                    p = strchr(p + 6, '"'); if (!p) continue; p++; char *e = strrchr(p, '"'); if (!e) continue; *e = 0;
                    char lib[MAX_PATH]; int j = 0; for (char *c = p; *c && j < MAX_PATH - 1; c++) { if (c[0] == '\\' && c[1] == '\\') c++; lib[j++] = *c; } lib[j] = 0;
                    wchar_t wl[MAX_PATH]; MultiByteToWideChar(CP_UTF8, 0, lib, -1, wl, MAX_PATH);
                    _snwprintf(g_game, MAX_PATH - 1, L"%s\\steamapps\\common\\Fallout New Vegas", wl);
                    if (GameHere(g_game)) { fclose(f); RegCloseKey(k); return true; } }
                fclose(f); }
        }
        RegCloseKey(k);
    }
    return false;
}
static bool PickGame(void)
{
    Out(C_WARN, L"Couldn't find Fallout: New Vegas by itself - please pick its folder (the one with FalloutNV.exe).\n");
    BROWSEINFOW bi; memset(&bi, 0, sizeof bi); bi.lpszTitle = L"Pick your Fallout New Vegas folder (where FalloutNV.exe is)"; bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST il = SHBrowseForFolderW(&bi); if (!il) return false;
    bool ok = SHGetPathFromIDListW(il, g_game) && GameHere(g_game); CoTaskMemFree(il);
    return ok;
}
static bool CanWriteGame(void)
{
    wchar_t p[MAX_PATH]; Join(p, g_game, L"fnvvrmc_write_test.tmp");
    HANDLE f = CreateFileW(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_FLAG_DELETE_ON_CLOSE, NULL);
    if (f == INVALID_HANDLE_VALUE) return false; CloseHandle(f); return true;
}

int wmain(int argc, wchar_t **argv)
{
    g_con = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTitleW(L"Fallout: New Vegas VR (FNVVRMC) - Setup");
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    GetModuleFileNameW(NULL, g_self, MAX_PATH);
    { wchar_t lp[MAX_PATH]; wcsncpy(lp, g_self, MAX_PATH - 1); lp[MAX_PATH - 1] = 0; wchar_t *s = wcsrchr(lp, L'\\'); if (s) wcscpy(s + 1, L"FNVVRMC-Setup.log"); g_log = _wfopen(lp, L"a"); }
    Out(C_HEAD, L"==============================================================\n");
    Out(C_HEAD, L"   Fallout: New Vegas VR (FNVVRMC) - one-click setup  v" SETUP_VERSION L"\n");
    Out(C_HEAD, L"==============================================================\n");
    Out(C_DIM, L"Installs the VR mod and everything it needs. Each file comes from its author's own page.\n\n");

    {   /* (a copy in a folder called FNVVRMC-Setup-Test installs into the test folder beside it, never the real game) */
        wchar_t d[MAX_PATH]; wcsncpy(d, g_self, MAX_PATH - 1); d[MAX_PATH - 1] = 0; wchar_t *sl = wcsrchr(d, L'\\'); if (sl) *sl = 0;
        const wchar_t *leaf = wcsrchr(d, L'\\'); leaf = leaf ? leaf + 1 : d;
        if (!(argc > 2 && !wcscmp(argv[1], L"--game")) && !_wcsicmp(leaf, L"FNVVRMC-Setup-Test")) {
            Join(g_game, d, L"Fallout New Vegas"); wchar_t dd[MAX_PATH]; Join(dd, g_game, L"Data"); MakeDirs(dd);
            wchar_t ex[MAX_PATH]; Join(ex, g_game, L"FalloutNV.exe"); if (!Exists(ex)) { FILE *f = _wfopen(ex, L"w"); if (f) { fputs("test", f); fclose(f); } }
            Out(C_WARN, L"TEST MODE - installing into the test folder, not your game.\n"); g_testMode = true;
        }
    }
    if (g_game[0]) { }
    else if (argc > 2 && !wcscmp(argv[1], L"--game")) { wcsncpy(g_game, argv[2], MAX_PATH - 1); g_game[MAX_PATH - 1] = 0; }
    else if (!FindGame() && !PickGame()) { Out(C_ERR, L"Fallout: New Vegas wasn't found - nothing was changed.\n"); Pause(); return 1; }
    Out(C_NORM, L"Game folder: "); Out(C_OK, L"%s\n", g_game);

    if (!CanWriteGame()) {   /* (Program Files needs administrator rights: ask once and start again) */
        bool elevated = argc > 3 && !wcscmp(argv[3], L"--elevated");
        if (elevated) { Out(C_ERR, L"Can't write to the game folder even as administrator - nothing was changed.\n"); Pause(); return 1; }
        Out(C_WARN, L"The game folder needs administrator rights to change - Windows will ask you once.\n");
        wchar_t args[MAX_PATH + 40]; _snwprintf(args, MAX_PATH + 39, L"--game \"%s\" --elevated", g_game);
        if ((INT_PTR)ShellExecuteW(NULL, L"runas", g_self, args, NULL, SW_SHOWNORMAL) > 32) return 0;
        Out(C_ERR, L"Administrator rights weren't given - nothing was changed.\n"); Pause(); return 1;
    }
    {   /* (all the DLC) */
        const wchar_t *dlc[] = { L"Data\\DeadMoney.esm", L"Data\\HonestHearts.esm", L"Data\\OldWorldBlues.esm", L"Data\\LonesomeRoad.esm", L"Data\\GunRunnersArsenal.esm" };
        int miss = 0; for (int i = 0; i < 5; i++) if (!InGame(dlc[i])) miss++;
        if (miss) Out(C_WARN, L"Note: %d of the 5 DLCs aren't installed. The VR mod is tested with all of them (Ultimate Edition).\n", miss);
    }
    { wchar_t t[MAX_PATH]; GetTempPathW(MAX_PATH, t); Join(g_tmp, t, L"FNVVRMC-Setup"); MakeDirs(g_tmp); }
    { PWSTR p = NULL; if (SHGetKnownFolderPath(&FOLDERID_Downloads, 0, NULL, &p) == S_OK) { wcsncpy(g_dl, p, MAX_PATH - 1); CoTaskMemFree(p); } }
    int failed = 0, skipped = 0;

    /* 1. xNVSE */
    Out(C_HEAD, L"\nxNVSE (script extender)\n"); Out(C_DIM, L"   FNVVRMC runs as an xNVSE plugin.\n");
    if (InGame(L"nvse_1_4.dll") && InGame(L"nvse_steam_loader.dll")) Out(C_OK, L"   already installed - skipping\n");
    else {
        wchar_t url[1024], name[MAX_PATH], file[MAX_PATH];
        if (GitHubAsset("xNVSE/NVSE", NULL, ".7z", url, name) || GitHubAsset("xNVSE/NVSE", NULL, ".zip", url, name) || GitHubAssetPage("xNVSE/NVSE", NULL, ".7z", url, name)) {
            Join(file, g_tmp, name); Out(C_NORM, L"   downloading %s from GitHub", name);
            if (HttpGet(url, file, NULL, NULL, false) && InstallArchive(file, true)) Out(C_OK, L"   installed (%d files)\n", g_copied);
            else { Out(C_ERR, L"   download or install failed\n"); failed++; }
        } else {
            Out(C_WARN, L"   GitHub couldn't be reached - getting it from Nexus instead\n");
            NexusMod x = { L"xNVSE", 67883, L"nvse_1_4.dll", L"", true, NULL }; int r = NexusStep(&x); if (r < 0) failed++; if (r > 0) skipped++;
        }
    }
    /* 2. the Nexus-only parts */
    static const NexusMod mods[] = {
        { L"FNV 4GB Patch", 62552, NULL, L"Lets the game use 4 GB of memory - stops the out-of-memory crashes.", true },
        { L"JIP LN NVSE Plugin", 58277, L"Data\\NVSE\\Plugins\\jip_nvse.dll", L"Scopes, climbing, holsters, quick-loot and reloading use it.", false },
        { L"JohnnyGuitar NVSE", 66927, L"Data\\NVSE\\Plugins\\johnnyguitar.dll", L"Script functions FNVVRMC uses.", false },
        { L"ShowOff xNVSE", 72541, L"Data\\NVSE\\Plugins\\ShowOffNVSE.dll", L"Script functions FNVVRMC uses.", false },
        { L"kNVSE", 71336, L"Data\\NVSE\\Plugins\\kNVSE.dll", L"Animation plugin the others expect.", false },
        { L"NVTF - New Vegas Tick Fix", 66537, L"Data\\NVSE\\Plugins\\NVTF.dll", L"Smooth frame pacing - needed for a steady picture in the headset.", false },
    };
    for (int i = 0; i < (int)(sizeof mods / sizeof mods[0]); i++) {
        const NexusMod *m = &mods[i];
        if (m->id == 62552 && InGame(L"FalloutNV_backup.exe")) { Out(C_HEAD, L"\n%s\n", m->name); Out(C_OK, L"   the game is already patched - skipping\n"); continue; }
        int r = NexusStep(m); if (r < 0) failed++; if (r > 0) skipped++;
        if (m->id == 62552 && r == 0) {   /* run the patcher once */
            const wchar_t *pn[] = { L"FNVpatch.exe", L"FalloutNVpatch.exe" }; bool ran = false;   /* (FNVpatch.exe is the current 1.5 patcher) */
            if (g_testMode) { Out(C_DIM, L"   (test mode: the patcher isn't run on the test folder)\n"); continue; }
            for (int j = 0; j < 2 && !ran; j++) if (InGame(pn[j])) { wchar_t exe[MAX_PATH], cmd[MAX_PATH + 4]; Join(exe, g_game, pn[j]); _snwprintf(cmd, MAX_PATH + 3, L"\"%s\"", exe);
                Out(C_NORM, L"   patching the game (a black window may flash)...\n"); DWORD ec = 0; Run(cmd, g_game, &ec, false); ran = true; }
            if (InGame(L"FalloutNV_backup.exe")) Out(C_OK, L"   game patched\n");
            else { Out(C_WARN, L"   the patcher didn't confirm - run FalloutNVpatch.exe in the game folder once yourself\n"); }
        }
    }
    /* 2b. the optional extras FNVVRMC has features for */
    {
        static const NexusMod opt[] = {
            { L"UIO - User Interface Organizer", 57174, L"Data\\NVSE\\Plugins\\ui_organizer.dll", L"Lets the quick-loot window and True Scopes add their menus.", false, L"" },
            { L"Just Assorted Mods", 66666, L"Data\\JustAssortedMods.esp", L"The quick-loot window (look at a container to see and take what's inside).", false, L"JustAssortedMods.esp" },
            { L"VATS Alternative - Bullet Time", 62278, L"Data\\VATS Alternative - Bullet Time.esp", L"Bullet time on the left trigger.", false, L"VATS Alternative - Bullet Time.esp" },
            { L"New Vegas True Scopes", 74824, L"Data\\NewVegasTrueScopes.esp", L"Real magnified scopes on scoped guns.", false, L"NewVegasTrueScopes.esp" },
        };
        int need = 0; for (int i = 0; i < 4; i++) if (!InGame(opt[i].check)) need++;
        Out(C_HEAD, L"\nOptional features\n");
        if (!need) { Out(C_OK, L"   quick-loot window, bullet time and True Scopes are already installed\n"); for (int i = 0; i < 4; i++) if (opt[i].esp && opt[i].esp[0]) EnablePlugin(opt[i].esp); }
        else {
            Out(C_NORM, L"   FNVVRMC also works with: the quick-loot window, bullet time, and True Scopes (%d more download%s).\n", need, need > 1 ? L"s" : L"");
            Out(C_WARN, L"   Install them too?  Y = yes (recommended),  N = no  "); int k = _getch(); Out(C_NORM, L"%c\n", k >= 32 && k < 127 ? k : L' ');
            if (k != 'n' && k != 'N') for (int i = 0; i < 4; i++) { int r = NexusStep(&opt[i]); if (r < 0) failed++; if (r > 0) skipped++; }
            else Out(C_DIM, L"   skipped - run setup again any time to add them\n");
        }
    }
    /* 3. FNVVRMC */
    Out(C_HEAD, L"\nFNVVRMC (the VR mod)\n");
    {
        wchar_t url[1024], name[MAX_PATH], file[MAX_PATH];
        bool have = GitHubAsset("LIME99902/Fallout-New-Vegas-VR", "-full", ".zip", url, name);
        if (!have) { Out(C_DIM, L"   (the release list didn't come through - reading the release page instead)\n"); have = GitHubAssetPage("LIME99902/Fallout-New-Vegas-VR", "-full", ".zip", url, name); }
        if (have) {
            Join(file, g_tmp, name); Out(C_NORM, L"   downloading %s from GitHub", name);
            wchar_t old[MAX_PATH], bak[MAX_PATH]; Join(old, g_game, L"Data\\NVSE\\Plugins\\FNVR.dll"); Join(bak, g_game, L"Data\\NVSE\\Plugins\\FNVR.dll.old");
            if (HttpGet(url, file, NULL, NULL, false)) {
                if (Exists(old)) CopyFileW(old, bak, FALSE);
                wchar_t ini[MAX_PATH]; Join(ini, g_game, L"Data\\NVSE\\Plugins\\FNVR\\FNVR.ini"); wchar_t iniKeep[MAX_PATH]; Join(iniKeep, g_tmp, L"FNVR.ini.keep");
                bool hadIni = Exists(ini) && CopyFileW(ini, iniKeep, FALSE);   /* (keep your own settings) */
                if (InstallArchive(file, false)) { if (hadIni) CopyFileW(iniKeep, ini, FALSE); Out(C_OK, L"   installed (%d files)%s\n", g_copied, hadIni ? L" - your settings were kept" : L""); }
                else { Out(C_ERR, L"   install failed\n"); failed++; }
            } else { Out(C_ERR, L"   download failed\n"); failed++; }
        } else { Out(C_ERR, L"   couldn't reach GitHub for the mod - check your internet and run setup again\n"); failed++; }
    }
    /* 4. SteamVR */
    Out(C_HEAD, L"\nSteamVR\n");
    { HKEY k; DWORD inst = 0, sz = sizeof inst;
      bool vr = RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam\\Apps\\250820", 0, KEY_READ, &k) == ERROR_SUCCESS && (RegQueryValueExW(k, L"Installed", NULL, NULL, (BYTE *)&inst, &sz) == ERROR_SUCCESS ? (RegCloseKey(k), inst != 0) : (RegCloseKey(k), false));
      if (vr) Out(C_OK, L"   installed\n"); else { Out(C_WARN, L"   not found - install SteamVR from Steam (it's free). Opening its store page.\n"); ShellExecuteW(NULL, L"open", L"steam://install/250820", NULL, NULL, SW_SHOWNORMAL); } }
    RemoveTree(g_tmp);

    Out(C_HEAD, L"\n==============================================================\n");
    if (failed) Out(C_ERR, L" %d part(s) didn't install - see above, then run this setup again (finished parts are skipped).\n", failed);
    else if (skipped) Out(C_WARN, L" Done, but %d part(s) were skipped - run setup again any time to add them.\n", skipped);
    else Out(C_OK, L" All done!\n");
    Out(C_NORM, L"\n How to play:\n");
    Out(C_NORM, L"   1. Start SteamVR with your headset on (Quest: Link / Air Link / Virtual Desktop first).\n");
    Out(C_NORM, L"   2. Launch Fallout: New Vegas from Steam as usual.\n");
    Out(C_NORM, L"   3. The first time, pick your headset, wired or wireless, and your PC's power - then restart the game once.\n");
    Out(C_NORM, L"   4. Stand up straight for the quick height/arm calibration, and play.\n");
    Out(C_DIM, L"\n Problems? Send FNVR.log from your Fallout New Vegas folder.\n");
    Out(C_HEAD, L"==============================================================\n");
    Pause();
    return failed ? 2 : 0;
}
