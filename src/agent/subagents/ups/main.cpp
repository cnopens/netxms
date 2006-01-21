/*
** NetXMS UPS management subagent
** Copyright (C) 2006 Victor Kirhenshtein
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
** File: main.cpp
**
**/

#include "ups.h"


//
// Static data
//

static UPSInterface *m_deviceInfo[MAX_UPS_DEVICES];


//
// Universal handler for string results
//

static LONG H_StringParam(TCHAR *pszParam, TCHAR *pArg, TCHAR *pValue)
{
   LONG nRet;

   switch(*((char *)pArg))
   {
      case 'M':   // Model
         break;
      default:
         nRet = SYSINFO_RC_UNSUPPORTED;
         break;
   }
   return nRet;
}


//
// List configured devices
//

static LONG H_DeviceList(TCHAR *pszParam, TCHAR *pArg, NETXMS_VALUES_LIST *pValue)
{
   return SYSINFO_RC_UNSUPPORTED;
}


//
// Called by master agent at unload
//

static void UnloadHandler(void)
{
}


//
// Add device from configuration file parameter
// Parameter value should be <device_id>:<port>:<protocol>
//

static BOOL AddDeviceFromConfig(TCHAR *pszCfg)
{
   m_deviceInfo[0] = new APCInterface("/dev/ttyS0");
   return TRUE;
}


//
// Subagent information
//

static NETXMS_SUBAGENT_PARAM m_parameters[] =
{
   { _T("UPS.Model(*)"), H_StringParam, "M", DCI_DT_STRING, _T("UPS model") }
};
static NETXMS_SUBAGENT_ENUM m_enums[] =
{
   { _T("UPS.DeviceList"), H_DeviceList, NULL }
};

static NETXMS_SUBAGENT_INFO m_info =
{
   NETXMS_SUBAGENT_INFO_MAGIC,
	_T("UPS"), _T(NETXMS_VERSION_STRING),
   UnloadHandler, NULL,
	sizeof(m_parameters) / sizeof(NETXMS_SUBAGENT_PARAM),
	m_parameters,
	sizeof(m_enums) / sizeof(NETXMS_SUBAGENT_ENUM),
	m_enums,
   0, NULL
};


//
// Configuration file template
//

static TCHAR *m_pszDeviceList = NULL;
static NX_CFG_TEMPLATE cfgTemplate[] =
{
   { _T("Device"), CT_STRING_LIST, _T('\n'), 0, 0, 0, &m_pszDeviceList },
   { _T(""), CT_END_OF_LIST, 0, 0, 0, 0, NULL }
};


//
// Entry point for NetXMS agent
//

DECLARE_SUBAGENT_INIT(APC)
{
   DWORD dwResult;

   memset(m_deviceInfo, 0, sizeof(UPSInterface *) * MAX_UPS_DEVICES);

   // Load configuration
   dwResult = NxLoadConfig(pszConfigFile, _T("UPS"), cfgTemplate, FALSE);
   if (dwResult == NXCFG_ERR_OK)
   {
      TCHAR *pItem, *pEnd;

      // Parse device list
      if (m_pszDeviceList != NULL)
      {
         for(pItem = m_pszDeviceList; *pItem != 0; pItem = pEnd + 1)
         {
            pEnd = _tcschr(pItem, _T('\n'));
            if (pEnd != NULL)
               *pEnd = 0;
            StrStrip(pItem);
            if (!AddDeviceFromConfig(pItem))
               NxWriteAgentLog(EVENTLOG_WARNING_TYPE,
                               _T("Unable to add device from configuration file. ")
                               _T("Original configuration record: %s"), pItem);
         }
         free(m_pszDeviceList);
      }
   }
   else
   {
      safe_free(m_pszDeviceList);
   }

   *ppInfo = &m_info;
   return dwResult == NXCFG_ERR_OK;
}


//
// DLL entry point
//

#ifdef _WIN32

BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
   if (dwReason == DLL_PROCESS_ATTACH)
      DisableThreadLibraryCalls(hInstance);
   return TRUE;
}

#endif
