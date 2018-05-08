/*
** OllyDbg Address Shortcuts plugin (for OllyDbg v2.01) - address_shortcuts.c
** Authors: Andrew D'Addesio
** License: Public domain
** Compile: gcc -std=c99 -Wall -m32 -Os -g0 -funsigned-char -shared -nostartfiles -s
**          -static-libgcc -o address_shortcuts.dll address_shortcuts.c ollydbg.lib
*/

#define _CRT_SECURE_NO_DEPRECATE
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "plugin.h"

BOOL WINAPI DllMainCRTStartup(HANDLE hInst, ULONG ul_reason_for_call, LPVOID lpReserved)
{
	return TRUE;
}

static uint32_t get_selection(t_table *pt)
{
	t_dump *pd;
	uint32_t address;

	if (pt != NULL
		&& (pd = pt->customdata) != NULL
		&& (address = pd->sel0) < pd->sel1)
		return address;

	/* No selection */
	return 0;
}

static const wchar_t *get_filename(const wchar_t *path)
{
	const wchar_t *last_slash = path-1;

	while (*path != 0) {
		if (*path == '/' || *path == '\\')
			last_slash = path;
		path++;
	}

	return last_slash+1;
}

enum copy_mode {
	rva,
	rva_pretty,
	file_offset,
	file_offset_pretty
};

enum follow_mode {
	disasm,
	dump,
	stack
};

static void copy_to_clipboard(uint32_t address, enum copy_mode mode)
{
	t_module *m;
	HANDLE hData;
	wchar_t *str;
	uint32_t offset;
	const wchar_t *name;

	m = Findmodule(address);
	if (!m) {
		Flash(L"Address does not belong to any module");
		return;
	}

	if (mode == rva || mode == rva_pretty) {
		offset = address - m->base;
		name = m->modname;
	} else {
		offset = Findfileoffset(m, address);
		if (offset == 0) {
			Flash(L"Address has no physical mapping in file");
			return;
		}

		name = get_filename(m->path);
	}

	/* Copy to the clipboard. */

	hData = GlobalAlloc(GMEM_MOVEABLE, (wcslen(name)+11+1)*sizeof(wchar_t));
	if (!hData)
		return;

	str = GlobalLock(hData);
	if (!str)
		goto close_hdata;

	if (mode == rva_pretty || mode == file_offset_pretty)
		wsprintf(str, L"%s+0x%x", name, offset);
	else
		wsprintf(str, L"%08X", offset);

	GlobalUnlock(hData);

	if (!OpenClipboard(NULL))
		goto close_hdata;

	if (!EmptyClipboard())
		goto close_clipboard;

	if (!SetClipboardData(CF_UNICODETEXT, hData))
		goto close_clipboard;

	CloseClipboard();
	return;

close_clipboard:
	CloseClipboard();
close_hdata:
	GlobalFree(hData);
}

static int cb_copy(t_table *pt, wchar_t *name, ulong index, int mode)
{
	uint32_t address;
	enum copy_mode copy_mode = (enum copy_mode)index;

	if ((mode != MENU_EXECUTE && mode != MENU_VERIFY) || (address = get_selection(pt)) == 0)
		return MENU_ABSENT;
	if (mode == MENU_VERIFY)
		return MENU_NORMAL;

	copy_to_clipboard(address, copy_mode);
	return MENU_NOREDRAW;
};

static int cb_follow(t_table *pt, wchar_t *name, ulong index, int mode)
{
	uint32_t address;
	enum follow_mode follow_mode = (enum follow_mode)index;

	if ((mode != MENU_EXECUTE && mode != MENU_VERIFY) || (address = get_selection(pt)) == 0)
		return MENU_ABSENT;
	if (mode == MENU_VERIFY)
		return MENU_NORMAL;

	Readmemory(&address, address, 4, MM_REPORT);
	if (!Findmemory(address)) {
		Flash(L"No memory at the specified address");
		return MENU_REDRAW;
	}

	switch (follow_mode) {
	case disasm:
		Setcpu(0, address, 0, 0, 0, CPU_ASMHIST|CPU_ASMCENTER|CPU_ASMFOCUS);
		break;
	case dump:
		Setcpu(0, 0, address, 0, 0, CPU_DUMPHIST|CPU_DUMPFOCUS);
		break;
	case stack: default:
		Setcpu(0, 0, 0, 0, address, CPU_STACKCTR|CPU_STACKFOCUS);
		break;
	}

	return MENU_NOREDRAW;
};

static int cb_about(t_table *pt, wchar_t *name, ulong index, int mode)
{
	if (mode == MENU_VERIFY)
		return MENU_NORMAL;
	if (mode != MENU_EXECUTE && mode != MENU_VERIFY)
		return MENU_ABSENT;

	Resumeallthreads();
	MessageBox(hwollymain,
		L"Address Shortcuts\r\n"
		L"Authors: Andrew D'Addesio\r\n"
		L"URL: https://github.com/daddesio/ollydbg-address-shortcuts\r\n\r\n"
		L"This plugin is public domain software released under the UNLICENSE.",
		L"Address Shortcuts", MB_OK | MB_ICONINFORMATION);
	Suspendallthreads();

	return MENU_NOREDRAW;
};

static t_menu main_menu[] = {
	{L"|About Address Shortcuts...",
		L"About Address Shortcuts",
		K_NONE, cb_about, NULL, {0} },
	{NULL, NULL, K_NONE, NULL, NULL, {0} }
};

static t_menu right_click_menu[] = {
	{ L"Follow DWORD in Disassembler",
		L"Follow doubleword in CPU Disassembler",
		KK_DIRECT|KK_CTRL|VK_RETURN, cb_follow, NULL, {disasm} },
	{ L"Follow DWORD in Dump",
		L"Follow doubleword in CPU Dump",
		/* TODO: Can we make the follow-dword-in-dump shortcut Enter
		** instead of Alt+Enter? On Enter, Olly pops open an Edit Bytes
		** dialog that we'd have to silence. */
		KK_DIRECT|KK_ALT|VK_RETURN, cb_follow, NULL, {dump} },
	{ L"Follow DWORD in Stack",
		L"Follow doubleword in CPU Stack",
		0, cb_follow, NULL, {stack} },
	{ L"Copy RVA",
		L"Copy RVA of selection to clipboard",
		0, cb_copy, NULL, {rva} },
	{ L"Copy RVA (pretty)",
		L"Copy RVA of selection to clipboard in the format module+offset",
		KK_DIRECT|KK_ALT|VK_OEM_PLUS, cb_copy, NULL, {rva_pretty} },
	{ L"Copy file offset",
		L"Copy file offset of selection to clipboard",
		0, cb_copy, NULL, {file_offset} },
	{ L"Copy file offset (pretty)",
		L"Copy file offset of selection to clipboard in the format file+offset",
		KK_DIRECT|KK_CTRL|VK_OEM_PLUS, cb_copy, NULL, {file_offset_pretty} },
	{ NULL, NULL, K_NONE, NULL, NULL, {0} }
};

extc t_menu * __cdecl ODBG2_Pluginmenu(wchar_t *type)
{
	if (!wcscmp(type, PWM_MAIN))
		return main_menu;
	if (!wcscmp(type, PWM_DISASM) || !wcscmp(type, PWM_DUMP) || !wcscmp(type, PWM_STACK))
		return right_click_menu;

	return NULL;
};

extc int __cdecl ODBG2_Plugininit(void)
{
	return 0;
}

extc void __cdecl ODBG2_Pluginreset(void)
{
}

extc void __cdecl ODBG2_Plugindestroy(void)
{
}

extc int ODBG2_Pluginquery(int ollydbgversion, ulong *features,
	wchar_t pluginname[SHORTNAME], wchar_t pluginversion[SHORTNAME])
{
	if (ollydbgversion < 201)
		return 0;

	wcscpy(pluginname, L"Address Shortcuts");
	wcscpy(pluginversion, L"0");
	return PLUGIN_VERSION;
};
