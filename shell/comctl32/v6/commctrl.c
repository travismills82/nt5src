/***************************************************************************
 *  msctls.c
 *
 *      Utils library initialization code
 *
 ***************************************************************************/

#include "ctlspriv.h"
#include <shfusion.h>
HINSTANCE g_hinst = 0;


BOOL g_fCriticalInitialized = FALSE;

CRITICAL_SECTION g_csDll = {{0},0, 0, NULL, NULL, 0 };

ATOM g_aCC32Subclass = 0;
ATOM g_atomThemeScrollBar = 0;

UINT g_uiACP = CP_ACP;

//
// Global DCs used during mirroring an Icon.
//
HDC g_hdc     = NULL;
HDC g_hdcMask = NULL;

// per process mem to store PlugUI information
LANGID g_PUILangId = MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL);

BOOL InitAnimateClass(HINSTANCE hInstance);
BOOL ListView_Init(HINSTANCE hinst);
BOOL TV_Init(HINSTANCE hinst);
BOOL InitComboExClass(HINSTANCE hinst);
BOOL Header_Init(HINSTANCE hinst);
BOOL Tab_Init(HINSTANCE hinst);
int  InitIPAddr(HANDLE hInstance);
BOOL InitPager(HINSTANCE hinst);
BOOL InitNativeFontCtl(HINSTANCE hinst);
void UnregisterClasses();
void Mem_Terminate();


// IMM initialize state 
BOOL g_fDBCSEnabled      = FALSE;
BOOL g_fMEEnabled        = FALSE;
BOOL g_fDBCSInputEnabled = FALSE;
BOOL g_fIMMEnabled       = FALSE;

void InitIme()
{
    g_fMEEnabled = GetSystemMetrics(SM_MIDEASTENABLED);
    
    g_fDBCSEnabled = g_fDBCSInputEnabled = GetSystemMetrics(SM_DBCSENABLED);
    g_fIMMEnabled = GetSystemMetrics(SM_IMMENABLED);

    if (!g_fDBCSInputEnabled)
    {
        g_fDBCSInputEnabled = g_fIMMEnabled;
    }
}


#ifdef DEBUG

// Verify that the localizers didn't accidentally change
// DLG_PROPSHEET from a DIALOG to a DIALOGEX.  _RealPropertySheet
// relies on this (as well as any apps which parse the dialog template
// in their PSCB_PRECREATE handler).

BOOL IsSimpleDialog(LPCTSTR ptszDialog)
{
    HRSRC hrsrc;
    LPDLGTEMPLATE pdlg;
    BOOL fSimple = FALSE;

    if ( (hrsrc = FindResource(HINST_THISDLL, ptszDialog, RT_DIALOG)) &&
         (pdlg = LoadResource(HINST_THISDLL, hrsrc)))
    {
        fSimple = HIWORD(pdlg->style) != 0xFFFF;
    }
    return fSimple;
}

//
//  For sublanguages to work, every language in our resources must contain
//  a SUBLANG_NEUTRAL variation so that (for example) Austria gets
//  German dialog boxes instead of English ones.
//
//  The DPA is really a DSA of WORDs, but DPA's are easier to deal with.
//  We just collect all the languages into the DPA, and study them afterwards.
//
BOOL CALLBACK CheckLangProc(HINSTANCE hinst, LPCTSTR lpszType, LPCTSTR lpszName, WORD wIdLang, LPARAM lparam)
{
    HDPA hdpa = (HDPA)lparam;
    DPA_AppendPtr(hdpa, (LPVOID)(UINT_PTR)wIdLang);
    return TRUE;
}

void CheckResourceLanguages(void)
{
    HDPA hdpa = DPA_Create(8);
    if (hdpa) {
        int i, j;
        EnumResourceLanguages(HINST_THISDLL, RT_DIALOG,
                              MAKEINTRESOURCE(DLG_PROPSHEET), CheckLangProc,
                              (LPARAM)hdpa);

        // Walk the language list.  For each language we find, make sure
        // there is a SUBLANG_NEUTRAL version of it somewhere else
        // in the list.  We use an O(n^2) algorithm because this is debug
        // only code and happens only at DLL load.

        for (i = 0; i < DPA_GetPtrCount(hdpa); i++) {
            UINT_PTR uLangI = (UINT_PTR)DPA_FastGetPtr(hdpa, i);
            BOOL fFound = FALSE;

            //
            //  It is okay to have English (American) with no
            //  English (Neutral) because Kernel32 uses English (American)
            //  as its fallback, so we fall back to the correct language
            //  after all.
            //
            if (uLangI == MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US))
                continue;

            //
            //  If this language is already the Neutral one, then there's
            //  no point looking for it - here it is!
            //
            if (SUBLANGID(uLangI) == SUBLANG_NEUTRAL)
                continue;

            //
            //  Otherwise, this language is a dialect.  See if there is
            //  a Neutral version elsewhere in the table.
            //
            for (j = 0; j < DPA_GetPtrCount(hdpa); j++) {
                UINT_PTR uLangJ = (UINT_PTR)DPA_FastGetPtr(hdpa, j);
                if (PRIMARYLANGID(uLangI) == PRIMARYLANGID(uLangJ) &&
                    SUBLANGID(uLangJ) == SUBLANG_NEUTRAL) {
                    fFound = TRUE; break;
                }
            }

            //
            //  If this assertion fires, it means that the localization team
            //  added support for a new language but chose to specify the
            //  language as a dialect instead of the Neutral version.  E.g.,
            //  specifying Romanian (Romanian) instead of Romanian (Neutral).
            //  This means that people who live in Moldavia will see English
            //  strings, even though Romanian (Romanian) would almost
            //  certainly have been acceptable.
            //
            //  If you want to support multiple dialects of a language
            //  (e.g., Chinese), you should nominate one of the dialects
            //  as the Neutral one.  For example, we currently support
            //  both Chinese (PRC) and Chinese (Taiwan), but the Taiwanese
            //  version is marked as Chinese (Neutral), so people who live in
            //  Singapore get Chinese instead of English.  Sure, it's
            //  Taiwanese Chinese, but at least it's Chinese.
            //
            AssertMsg(fFound, TEXT("Localization bug: No SUBLANG_NEUTRAL for language %04x"), uLangI);
        }

        DPA_Destroy(hdpa);
    }
}

#endif

BOOL IsRunningIn16BitProcess()
{
    NTSTATUS status;
    ULONG    ulVDMFlags = 0;
    status = NtQueryInformationProcess(GetCurrentProcess(), ProcessWx86Information, &ulVDMFlags, sizeof(ulVDMFlags), NULL);
    return (NT_SUCCESS(status) && (ulVDMFlags != 0));
}


int _ProcessAttach(HANDLE hInstance)
{
    g_hinst = hInstance;

    g_uiACP = GetACP();
    g_atomThemeScrollBar = GlobalAddAtom(TEXT("ThemePropScrollBarCtl"));

    SHFusionInitialize(NULL);

#ifdef DEBUG
    CcshellGetDebugFlags();
#endif

    g_fCriticalInitialized = InitializeCriticalSectionAndSpinCount(&g_csDll, 0);

    InitGlobalMetrics(0);
    InitGlobalColors();
    
    InitIme();

    //
    // Initialize special language pack callouts for
    // the edit controls. Once per process.
    //
    InitEditLpk();

    if (IsRunningIn16BitProcess())
    {
        // This is a 16bit process. We need to artificially init the common controls
        INITCOMMONCONTROLSEX icce;
        icce.dwSize = sizeof(icce);
        icce.dwICC = ICC_ALL_CLASSES & ~ICC_STANDARD_CLASSES;
        InitCommonControlsEx(&icce);
    }

    return TRUE;
}



void _ProcessDetach(HANDLE hInstance)
{
    //
    // Cleanup cached DCs. No need to synchronize the following section of
    // code since it is only called in DLL_PROCESS_DETACH which is 
    // synchronized by the OS Loader.
    //
    if (g_hdc)
        DeleteDC(g_hdc);

    if (g_hdcMask)
        DeleteDC(g_hdcMask);

    g_hdc = g_hdcMask = NULL;

    UnregisterClasses();
    g_fCriticalInitialized = FALSE;
    DeleteCriticalSection(&g_csDll);
    SHFusionUninitialize();
}


STDAPI_(BOOL) LibMain(HANDLE hDll, DWORD dwReason, LPVOID pv)
{
    switch(dwReason) 
    {
    case DLL_PROCESS_ATTACH:
#ifdef DEBUG
        GdiSetBatchLimit(1);
#else
        DisableThreadLibraryCalls(hDll);
#endif
        return _ProcessAttach(hDll);

    case DLL_PROCESS_DETACH:
        _ProcessDetach(hDll);
        break;

    case DLL_THREAD_ATTACH:
#ifdef DEBUG
        GdiSetBatchLimit(1);
#endif
    case DLL_THREAD_DETACH:
    default:
        break;

    } // end switch()

    return TRUE;

} // end DllEntryPoint()


/* Stub function to call if all you want to do is make sure this DLL is loaded
 */
void WINAPI InitCommonControls(void)
{
}

STDAPI_(void) FixupSubclassRecordsAfterLogoff();

BOOL InitForWinlogon(HINSTANCE hInstance)
{
    //  Some people like to use comctl32 from inside winlogon, and
    //  for C2 security reasons, all global atoms are nuked from the
    //  window station when you log off.
    //
    //  So the rule is that all winlogon clients of comctl32 must
    //  call InitCommonControlsEx(ICC_WINLOGON_REINIT) immediately
    //  before doing any common control things (creating windows
    //  or property sheets/wizards) from winlogon.

    FixupSubclassRecordsAfterLogoff();

    InitGlobalMetrics(0);
    InitGlobalColors();

    return TRUE;
}

/* InitCommonControlsEx creates the classes. Only those classes requested are created!
** The process attach figures out if it's an old app and supplies ICC_WIN95_CLASSES.
*/
typedef BOOL (PASCAL *PFNINIT)(HINSTANCE);
typedef struct 
{
    PFNINIT pfnInit;
    LPCTSTR pszName;
    DWORD   dwClassSet;
    BOOL    fRegistered;
} INITCOMMONCONTROLSINFO;

#define MAKEICC(pfnInit, pszClass, dwFlags) { pfnInit, pszClass, dwFlags, FALSE }

INITCOMMONCONTROLSINFO icc[] =
{
     // Init function      Class name         Requested class sets which use this class
MAKEICC(InitToolbarClass,  TOOLBARCLASSNAME,  ICC_BAR_CLASSES),
MAKEICC(InitReBarClass,    REBARCLASSNAME,    ICC_COOL_CLASSES),
MAKEICC(InitToolTipsClass, TOOLTIPS_CLASS,    ICC_TREEVIEW_CLASSES|ICC_BAR_CLASSES|ICC_TAB_CLASSES),
MAKEICC(InitStatusClass,   STATUSCLASSNAME,   ICC_BAR_CLASSES),
MAKEICC(ListView_Init,     WC_LISTVIEW,       ICC_LISTVIEW_CLASSES),
MAKEICC(Header_Init,       WC_HEADER,         ICC_LISTVIEW_CLASSES),
MAKEICC(Tab_Init,          WC_TABCONTROL,     ICC_TAB_CLASSES),
MAKEICC(TV_Init,           WC_TREEVIEW,       ICC_TREEVIEW_CLASSES),
MAKEICC(InitTrackBar,      TRACKBAR_CLASS,    ICC_BAR_CLASSES),
MAKEICC(InitUpDownClass,   UPDOWN_CLASS,      ICC_UPDOWN_CLASS),
MAKEICC(InitProgressClass, PROGRESS_CLASS,    ICC_PROGRESS_CLASS),
MAKEICC(InitHotKeyClass,   HOTKEY_CLASS,      ICC_HOTKEY_CLASS),
MAKEICC(InitAnimateClass,  ANIMATE_CLASS,     ICC_ANIMATE_CLASS),
MAKEICC(InitDateClasses,   DATETIMEPICK_CLASS,ICC_DATE_CLASSES),
MAKEICC(InitDateClasses,   MONTHCAL_CLASS,    0),
MAKEICC(InitComboExClass,  WC_COMBOBOXEX,     ICC_USEREX_CLASSES),
MAKEICC(InitIPAddr,        WC_IPADDRESS,      ICC_INTERNET_CLASSES),
MAKEICC(InitPager,         WC_PAGESCROLLER,   ICC_PAGESCROLLER_CLASS),
MAKEICC(InitNativeFontCtl, WC_NATIVEFONTCTL,  ICC_NATIVEFNTCTL_CLASS),
MAKEICC(InitButtonClass,   WC_BUTTON,         ICC_STANDARD_CLASSES),
MAKEICC(InitStaticClass,   WC_STATIC,         ICC_STANDARD_CLASSES),
MAKEICC(InitEditClass,     WC_EDIT,           ICC_STANDARD_CLASSES),
MAKEICC(InitListBoxClass,  WC_LISTBOX,        ICC_STANDARD_CLASSES),
MAKEICC(InitComboboxClass, WC_COMBOBOX,       ICC_STANDARD_CLASSES),
MAKEICC(InitComboLBoxClass,WC_COMBOLBOX,      ICC_STANDARD_CLASSES),
MAKEICC(InitScrollBarClass,WC_SCROLLBAR,      ICC_STANDARD_CLASSES),
MAKEICC(InitLinkClass,     WC_LINK,           ICC_LINK_CLASS),
MAKEICC(InitReaderModeClass,WC_READERMODE,    ICC_STANDARD_CLASSES),


//
//  These aren't really classes.  They're just goofy flags.
//
MAKEICC(InitForWinlogon,   NULL,              ICC_WINLOGON_REINIT),
};

BOOL WINAPI InitCommonControlsEx(LPINITCOMMONCONTROLSEX picce)
{
    ULONG_PTR ul;
    int  i;
    BOOL fRet = TRUE;

    if (!picce ||
        (picce->dwSize != sizeof(INITCOMMONCONTROLSEX)) ||
        (picce->dwICC & ~ICC_ALL_VALID))
    {
        DebugMsg(DM_WARNING, TEXT("comctl32 - picce is bad"));
        return FALSE;
    }

    SHActivateContext(&ul);

    for (i=0 ; i < ARRAYSIZE(icc); i++)
    {
        if (picce->dwICC & icc[i].dwClassSet)
        {
            if (!icc[i].pfnInit(HINST_THISDLL))
            {
                fRet = FALSE;
                break;
            }
            else
            {
                icc[i].fRegistered = TRUE;
            }
        }
    }

    SHDeactivateContext(ul);

    return fRet;
}
//
// InitMUILanguage / GetMUILanguage implementation
//
// we have a per process PUI language setting. For NT it's just a global
// initialized with LANG_NEUTRAL and SUBLANG_NEUTRAL
// For Win95 it's DPA slot for the current process.
// InitMUILanguage sets callers preferred language id for common control
// GetMUILangauge returns what the caller has set to us 
// 
void WINAPI
InitMUILanguage(LANGID wLang)
{
    ENTERCRITICAL;
    g_PUILangId = wLang;
    LEAVECRITICAL;
}

LANGID WINAPI
GetMUILanguage(void)
{
    return g_PUILangId;
}
// end MUI functions

//
//  Unlike Win9x, WinNT does not automatically unregister classes
//  when a DLL unloads.  We have to do it manually.  Leaving the
//  class lying around means that if an app loads our DLL, then
//  unloads it, then reloads it at a different address, all our
//  leftover RegisterClass()es will point the WndProc at the wrong
//  place and we fault at the next CreateWindow().
//
//  This is not purely theoretical - NT4/FE hit this bug.
//
void UnregisterClasses()
{
    int i;

    for (i=0 ; i < ARRAYSIZE(icc) ; i++)
    {
        if (icc[i].pszName && icc[i].fRegistered)
        {
            UnregisterClass(icc[i].pszName, HINST_THISDLL);
        }
    }
}

#if defined(DEBUG)
LRESULT WINAPI SendMessageD(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    ASSERTNONCRITICAL;
    return SendMessageW(hWnd, Msg, wParam, lParam);
}
#endif // defined(DEBUG)


BOOL WINAPI RegisterClassNameW(LPCWSTR pszClass)
{
    ULONG_PTR ul;
    int  i;
    BOOL fRet = FALSE;

    SHActivateContext(&ul);

    for (i = 0; i < ARRAYSIZE(icc); i++)
    {
        if (lstrcmpi(icc[i].pszName, pszClass) == 0)
        {
            if (icc[i].pfnInit(HINST_THISDLL))
            {
                icc[i].fRegistered = TRUE;
                fRet = TRUE;
            }

            break;
        }
    }

    SHDeactivateContext(ul);

    return fRet;
}


