/*
** OllyDbg v2.0 Address Shortcuts plugin - address_shortcuts.c
** Authors: Andrew D'Addesio <andrew@fatbag.net>
** License: Public domain
** Compile: gcc -std=c99 -Wall -Wextra -Wno-unused-parameter -m32 -Os -g0 -fomit-frame-pointer
**	-funsigned-char -shared -nostartfiles -s -static-libgcc -o address_shortcuts.dll
**	address_shortcuts.c ollydbg.lib
**
** This plugin adds the following 6 shortcuts to OllyDbg:
** (*) Follow DWORD in Dump (Alt+Enter)
** (*) Follow DWORD in Disassembler (Ctrl+Enter)
** (*) Copy address base relative (Alt+"+")
** (*) Copy address file relative (Ctrl+"+")
** (*) Copy DWORD base relative (Alt+"-")
** (*) Copy DWORD file relative (Ctrl+"-").
** where the "+" and "-" keys are at the top row of the keyboard, *not* the number pad.
**
** Base or file relative addresses will be copied to the clipboard in the form:
**     mymodule_base+0x26c0 (base-relative)
** or
**     mymodule_file+0x26c0 (file-relative).
**
** Personally, I find the first three shortcuts to be essential.
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

enum rel_mode {
	BASE_RELATIVE,
	FILE_RELATIVE
};

static void copy_to_clipboard(uint32_t address, enum rel_mode mode)
{
	t_module *m;
	HANDLE hData;
	wchar_t *str;
	uint32_t offset;

	m = Findmodule(address);
	if (!m) {
		Flash(L"Address does not belong to any module");
		return;
	}

	switch (mode) {
	case BASE_RELATIVE:
		offset = address - m->base;
		break;
	case FILE_RELATIVE: default:
		if (mode == FILE_RELATIVE) {
			if (address == m->base)
				offset = 0;
			else {
				offset = Findfileoffset(m, address);
				if (offset == 0) {
					Flash(L"Address has no physical mapping in file");
					return;
				}
			}
		}
	}

	/* Copy to the clipboard. */

	hData = GlobalAlloc(GMEM_MOVEABLE, 1024*sizeof(wchar_t));
	if (!hData)
		return;

	str = GlobalLock(hData);
	if (!str) {
		GlobalFree(hData);
		return;
	}
	wsprintf(str, L"%s%s+0x%x", m->modname, mode == BASE_RELATIVE ? L"_base" : L"_file", offset);
	GlobalUnlock(hData);

	if (!OpenClipboard(NULL)) {
		GlobalFree(hData);
		return;
	}

	EmptyClipboard();
	SetClipboardData(CF_UNICODETEXT, hData);
	CloseClipboard();
}

static int cb_copy_address_base_relative(t_table *pt, wchar_t *name, ulong index, int mode)
{
	uint32_t address;

	if ((mode != MENU_EXECUTE && mode != MENU_VERIFY) || (address = get_selection(pt)) == 0)
		return MENU_ABSENT;
	if (mode == MENU_VERIFY)
		return MENU_NORMAL;

	copy_to_clipboard(address, BASE_RELATIVE);
	return MENU_NOREDRAW;
};

static int cb_copy_address_file_relative(t_table *pt, wchar_t *name, ulong index, int mode)
{
	uint32_t address;

	if ((mode != MENU_EXECUTE && mode != MENU_VERIFY) || (address = get_selection(pt)) == 0)
		return MENU_ABSENT;
	if (mode == MENU_VERIFY)
		return MENU_NORMAL;

	copy_to_clipboard(address, FILE_RELATIVE);
	return MENU_NOREDRAW;
};

static int cb_copy_dword_base_relative(t_table *pt, wchar_t *name, ulong index, int mode)
{
	uint32_t address;

	if ((mode != MENU_EXECUTE && mode != MENU_VERIFY) || (address = get_selection(pt)) == 0)
		return MENU_ABSENT;
	if (mode == MENU_VERIFY)
		return MENU_NORMAL;

	Readmemory(&address, address, 4, MM_REPORT);
	if (!Findmemory(address)) {
		Flash(L"No memory at the specified address");
		return MENU_REDRAW;
	}

	copy_to_clipboard(address, BASE_RELATIVE);
	return MENU_NOREDRAW;
};

static int cb_copy_dword_file_relative(t_table *pt, wchar_t *name, ulong index, int mode)
{
	uint32_t address;

	if ((mode != MENU_EXECUTE && mode != MENU_VERIFY) || (address = get_selection(pt)) == 0)
		return MENU_ABSENT;
	if (mode == MENU_VERIFY)
		return MENU_NORMAL;

	Readmemory(&address, address, 4, MM_REPORT);
	if (!Findmemory(address)) {
		Flash(L"No memory at the specified address");
		return MENU_REDRAW;
	}

	copy_to_clipboard(address, FILE_RELATIVE);
	return MENU_NOREDRAW;
};

static int cb_follow_in_disassembler(t_table *pt, wchar_t *name, ulong index, int mode)
{
	uint32_t address;

	if ((mode != MENU_EXECUTE && mode != MENU_VERIFY) || (address = get_selection(pt)) == 0)
		return MENU_ABSENT;
	if (mode == MENU_VERIFY)
		return MENU_NORMAL;

	Readmemory(&address, address, 4, MM_REPORT);
	if (!Findmemory(address)) {
		Flash(L"No memory at the specified address");
		return MENU_REDRAW;
	}

	Setcpu(0, address, 0, 0, 0, CPU_ASMHIST|CPU_ASMCENTER|CPU_ASMFOCUS);
	return MENU_NOREDRAW;
};

static int cb_follow_in_dump(t_table *pt, wchar_t *name, ulong index, int mode)
{
	uint32_t address;

	if ((mode != MENU_EXECUTE && mode != MENU_VERIFY) || (address = get_selection(pt)) == 0)
		return MENU_ABSENT;
	if (mode == MENU_VERIFY)
		return MENU_NORMAL;

	Readmemory(&address, address, 4, MM_REPORT);
	if (!Findmemory(address)) {
		Flash(L"No memory at the specified address");
		return MENU_REDRAW;
	}

	Setcpu(0, 0, address, 0, 0, CPU_DUMPHIST|CPU_DUMPFOCUS);
	return MENU_REDRAW;
};

static int cb_about(t_table *pt, wchar_t *name, ulong index, int mode)
{
	if (mode == MENU_VERIFY)
		return MENU_NORMAL;
	if (mode != MENU_EXECUTE && mode != MENU_VERIFY)
		return MENU_ABSENT;

	Resumeallthreads();
	MessageBox(hwollymain,
		L"Address shortcuts\r\n"
		L"Authors: Andrew D'Addesio <andrew.daddesio@utexas.edu>\r\n"
		L"URL: https://github.com/daddesio/ollydbg-address-shortcuts\r\n\r\n"
		L"This plugin is public domain software.",
		L"Address shortcuts", MB_OK | MB_ICONINFORMATION);
	Suspendallthreads();

	return MENU_NOREDRAW;
};

static t_menu main_menu[] = {
	{L"|About address shortcuts...",
		L"About Address shortcuts",
		K_NONE, cb_about, NULL, {0} },
	{NULL, NULL, K_NONE, NULL, NULL, {0} }
};

static t_menu right_click_menu[] = {
	{ L"Copy address base relative",
		L"Copy address of selection to clipboard, relative to module base",
		KK_DIRECT|KK_ALT|VK_OEM_PLUS, cb_copy_address_base_relative, NULL, {0} },
	{ L"Copy address file relative",
		L"Copy address of selection to clipboard, relative to module file start",
		KK_DIRECT|KK_CTRL|VK_OEM_PLUS, cb_copy_address_file_relative, NULL, {0} },
	{ L"Copy DWORD base relative",
		L"Copy doubleword to clipboard, relative to module base",
		KK_DIRECT|KK_ALT|VK_OEM_MINUS, cb_copy_dword_base_relative, NULL, {0} },
	{ L"Copy DWORD file relative",
		L"Copy doubleword to clipboard, relative to module file start",
		KK_DIRECT|KK_CTRL|VK_OEM_MINUS, cb_copy_dword_file_relative, NULL, {0} },
	{ L"Follow DWORD in Disassembler",
		L"Follow doubleword in CPU Disassembler",
		KK_DIRECT|KK_CTRL|VK_RETURN, cb_follow_in_disassembler, NULL, {0} },
	{ L"Follow DWORD in Dump",
		L"Follow doubleword in CPU Dump",
		KK_DIRECT|KK_ALT|VK_RETURN, cb_follow_in_dump, NULL, {0} },
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

	wcscpy(pluginname, L"Address shortcuts");
	wcscpy(pluginversion, L"0");
	return PLUGIN_VERSION;
};
