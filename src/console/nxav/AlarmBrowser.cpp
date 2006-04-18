// AlarmBrowser.cpp : implementation file
//

#include "stdafx.h"
#include "nxav.h"
#include "AlarmBrowser.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

//
// Static data
//

static HHOOK m_hHook = NULL;


//
// Hook procedure for mouse events
//

static LRESULT CALLBACK MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
   if (nCode < 0)
      return CallNextHookEx(m_hHook, nCode, wParam, lParam);

   if (wParam == WM_RBUTTONDOWN)
   {
      return 1;
   }
   else
   {
      return CallNextHookEx(m_hHook, nCode, wParam, lParam);
   }
}


/////////////////////////////////////////////////////////////////////////////
// CAlarmBrowser

IMPLEMENT_DYNCREATE(CAlarmBrowser, CHtmlView)

CAlarmBrowser::CAlarmBrowser()
{
	//{{AFX_DATA_INIT(CAlarmBrowser)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT

   if (m_hHook == NULL)
      m_hHook = SetWindowsHookEx(WH_MOUSE, MouseHookProc, appAlarmViewer.m_hInstance, GetCurrentThreadId());
}

CAlarmBrowser::~CAlarmBrowser()
{
   if (m_hHook != NULL)
   {
      UnhookWindowsHookEx(m_hHook);
      m_hHook = NULL;
   }
}

void CAlarmBrowser::DoDataExchange(CDataExchange* pDX)
{
	CHtmlView::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAlarmBrowser)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CAlarmBrowser, CHtmlView)
	//{{AFX_MSG_MAP(CAlarmBrowser)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CAlarmBrowser diagnostics

#ifdef _DEBUG
void CAlarmBrowser::AssertValid() const
{
	CHtmlView::AssertValid();
}

void CAlarmBrowser::Dump(CDumpContext& dc) const
{
	CHtmlView::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CAlarmBrowser message handlers


//
// Handle navigation
//

void CAlarmBrowser::OnBeforeNavigate2(LPCTSTR lpszURL, DWORD nFlags, LPCTSTR lpszTargetFrameName, CByteArray& baPostedData, LPCTSTR lpszHeaders, BOOL* pbCancel) 
{
   DWORD dwId;

	CHtmlView::OnBeforeNavigate2(lpszURL, nFlags,	lpszTargetFrameName, baPostedData, lpszHeaders, pbCancel);
   if (!_tcsncmp(lpszURL, _T("nxav:"), 5))
   {
      *pbCancel = TRUE;
      switch(lpszURL[5])
      {
         case 'A':   // Acknowlege
            dwId = _tcstoul(&lpszURL[7], NULL, 10);
            AcknowledgeAlarm(dwId);
            break;
         case 'S':   // Disable repeated sound
            dwId = _tcstoul(&lpszURL[7], NULL, 10);
            AfxGetMainWnd()->PostMessage(WM_DISABLE_ALARM_SOUND, dwId, 0);
            break;
         default:
            break;
      }
   }
}


//
// Acknowlege alarm by ID
//

BOOL CAlarmBrowser::AcknowledgeAlarm(DWORD dwAlarmId)
{
   DWORD dwResult;

   dwResult = DoRequestArg2(NXCAcknowlegeAlarm, g_hSession,
                            (void *)dwAlarmId, _T("Acknowleging alarm..."));
   return (dwResult == RCC_SUCCESS);
}


//
// Set content from string
//

BOOL CAlarmBrowser::SetHTML(CString &strHTML)
{
   CComPtr<IDispatch> pDisp2 = GetHtmlDocument();
   if (pDisp2 == NULL)
      return FALSE;

   CComPtr<IHTMLDocument2> pDoc;
   pDisp2->QueryInterface(IID_IHTMLDocument2, (void* *)&pDoc);

   BSTR bstr = strHTML.AllocSysString();
   HRESULT hresult = S_OK;
   SAFEARRAY *sfArray = SafeArrayCreateVector(VT_VARIANT, 0, 1);
   if (sfArray != NULL)
   {
      VARIANT *param;

      hresult = SafeArrayAccessData(sfArray, (LPVOID *)&param);
      param->vt = VT_BSTR;
      param->bstrVal = bstr;
      hresult = SafeArrayUnaccessData(sfArray);
      hresult = pDoc->write(sfArray);
      hresult = pDoc->close();
   }

   SysFreeString(bstr);
   if (sfArray != NULL)
      SafeArrayDestroy(sfArray);

   return TRUE;
}
