/* 
** NetXMS multiplatform core agent
** Copyright (C) 2003-2020 Victor Kirhenshtein
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be usefu,,
** but ITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
** File: watchdog.cpp
**
**/

#include "nxagentd.h"
#ifdef _WIN32
#include <WtsApi32.h>
#else
#include <signal.h>
#include <syslog.h>
#endif

#define DEBUG_TAG _T("watchdog")

/**
 * Static data
 */
#ifdef _WIN32
static HANDLE m_hWatchdogProcess = INVALID_HANDLE_VALUE;
#else
static pid_t m_pidWatchdogProcess = -1;
#endif

/**
 * Start watchdog process
 */
void StartWatchdog()
{
   TCHAR szCmdLine[4096], szPlatformSuffixOption[MAX_PSUFFIX_LENGTH + 16];
#ifdef _WIN32
   TCHAR szExecName[MAX_PATH];
   STARTUPINFO si;
   PROCESS_INFORMATION pi;

   GetModuleFileName(GetModuleHandle(NULL), szExecName, MAX_PATH);
#else
   TCHAR szExecName[MAX_PATH] = PREFIX _T("/bin/nxagentd");
#endif

	if (g_szPlatformSuffix[0] != 0)
	{
		_sntprintf(szPlatformSuffixOption, MAX_PSUFFIX_LENGTH + 16, _T("-P \"%s\" "), g_szPlatformSuffix);
	}
	else
	{
		szPlatformSuffixOption[0] = 0;
	}

	shared_ptr<Config> generalConfig = GetConfig();
   const TCHAR *configSection = generalConfig->getAlias(_T("agent"));
   TCHAR configSectionOption[256];
   if ((configSection != NULL) && (*configSection != 0))
   {
      _sntprintf(configSectionOption, 256, _T("-G %s "), configSection);
   }
   else
   {
      configSectionOption[0] = 0;
   }

#ifdef _WIN32
   _sntprintf(szCmdLine, 4096, _T("\"%s\" -c \"%s\" %s%s%s%s%s%s-D %d %s-W %u"), szExecName,
              g_szConfigFile, configSectionOption,
              (g_dwFlags & AF_DAEMON) ? _T("-d ") : _T(""),
              (g_dwFlags & AF_HIDE_WINDOW) ? _T("-H ") : _T(""),
				  (g_dwFlags & AF_CENTRAL_CONFIG) ? _T("-M ") : _T(""),
				  (g_dwFlags & AF_CENTRAL_CONFIG) ? g_szConfigServer : _T(""),
				  (g_dwFlags & AF_CENTRAL_CONFIG) ? _T(" ") : _T(""),
				  nxlog_get_debug_level(), szPlatformSuffixOption,
              (g_dwFlags & AF_DAEMON) ? 0 : GetCurrentProcessId());
	nxlog_debug_tag(DEBUG_TAG, 1, _T("Starting agent watchdog with command line '%s'"), szCmdLine);

   // Fill in process startup info structure
   memset(&si, 0, sizeof(STARTUPINFO));
   si.cb = sizeof(STARTUPINFO);

   // Create new process
   if (!CreateProcess(NULL, szCmdLine, NULL, NULL, FALSE, 
                      (g_dwFlags & AF_DAEMON) ? (CREATE_NO_WINDOW | DETACHED_PROCESS) : (CREATE_NEW_CONSOLE),
                      NULL, NULL, &si, &pi))
   {
      TCHAR buffer[1024];
      nxlog_write_tag(NXLOG_ERROR, DEBUG_TAG, _T("Unable to create process \"%s\" (%s)"),
            szCmdLine, GetSystemErrorText(GetLastError(), buffer, 1024));
   }
   else
   {
      // Close main thread handle
      CloseHandle(pi.hThread);
      m_hWatchdogProcess = pi.hProcess;
      nxlog_write_tag(NXLOG_INFO, DEBUG_TAG, _T("Watchdog process started"));
   }
#else
   _sntprintf(szCmdLine, 4096, _T("\"%s\" -c \"%s\" %s%s%s%s%s-D %d %s-W %lu"), szExecName,
              g_szConfigFile, configSectionOption,
              (g_dwFlags & AF_DAEMON) ? _T("-d ") : _T(""),
				  (g_dwFlags & AF_CENTRAL_CONFIG) ? _T("-M ") : _T(""),
				  (g_dwFlags & AF_CENTRAL_CONFIG) ? g_szConfigServer : _T(""),
				  (g_dwFlags & AF_CENTRAL_CONFIG) ? _T(" ") : _T(""),
				  nxlog_get_debug_level(), szPlatformSuffixOption,
              (unsigned long)getpid());
   if (ExecuteCommand(szCmdLine, NULL, &m_pidWatchdogProcess) == ERR_SUCCESS)
	{
	   nxlog_write_tag(NXLOG_INFO, DEBUG_TAG, _T("Watchdog process started"));
	}
	else
	{
      nxlog_write_tag(NXLOG_ERROR, DEBUG_TAG, _T("Unable to create process \"%s\""), szCmdLine);
	}
#endif
}

/**
 * Stop watchdog process
 */
void StopWatchdog()
{
#ifdef _WIN32
	TerminateProcess(m_hWatchdogProcess, 0);
   WaitForSingleObject(m_hWatchdogProcess, 10000);
#else
	kill(m_pidWatchdogProcess, SIGKILL);
#endif
   nxlog_write_tag(NXLOG_INFO, DEBUG_TAG, _T("Watchdog process stopped"));
}

/**
 * Watchdog process main
 */
int WatchdogMain(DWORD pid, const TCHAR *configSection)
{
	TCHAR szPlatformSuffixOption[MAX_PSUFFIX_LENGTH + 16];
	if (g_szPlatformSuffix[0] != 0)
	{
		_sntprintf(szPlatformSuffixOption, MAX_PSUFFIX_LENGTH + 16, _T("-P \"%s\" "), g_szPlatformSuffix);
	}
	else
	{
		szPlatformSuffixOption[0] = 0;
	}

	TCHAR configSectionOption[256];
   if ((configSection != NULL) && (*configSection != 0))
   {
      _sntprintf(configSectionOption, 256, _T("-G %s "), configSection);
   }
   else
   {
      configSectionOption[0] = 0;
   }

#if defined(_WIN32)
   if (pid == 0)
   {
      // Service
		WaitForService(SERVICE_STOPPED);
		StartAgentService();
   }
   else
   {
      HANDLE hProcess;

      hProcess = OpenProcess(SYNCHRONIZE, FALSE, pid);
      if (hProcess != NULL)
      {
         if (WaitForSingleObject(hProcess, INFINITE) != WAIT_TIMEOUT)
         {
				/* TODO: start agent's process */
         }
         CloseHandle(hProcess);
      }
   }
#else
	openlog("nxagentd-watchdog", LOG_PID, LOG_DAEMON);
	while(1)
   {
      sleep(2);
      if (kill(pid, SIGCONT) == -1)
         break;
   }
	syslog(LOG_WARNING, "restarting agent");
	
   TCHAR cmdLine[4096];
   _sntprintf(cmdLine, 4096, _T("\"") PREFIX _T("/bin/nxagentd\" -c \"%s\" %s%s%s%s%s-D %d %s"),
              g_szConfigFile, configSectionOption,
              (g_dwFlags & AF_DAEMON) ? _T("-d ") : _T(""),
	           (g_dwFlags & AF_CENTRAL_CONFIG) ? _T("-M ") : _T(""),
				  (g_dwFlags & AF_CENTRAL_CONFIG) ? g_szConfigServer : _T(""),
				  (g_dwFlags & AF_CENTRAL_CONFIG) ? _T(" ") : _T(""),
				  nxlog_get_debug_level(), szPlatformSuffixOption);
#ifdef UNICODE
	syslog(LOG_INFO, "command line: %ls", cmdLine);
#else
	syslog(LOG_INFO, "command line: %s", cmdLine);
#endif
   if (ExecuteCommand(cmdLine, NULL, NULL) != ERR_SUCCESS)
	{
#ifdef UNICODE
		syslog(LOG_ERR, "failed to restart agent (command line: %ls)", cmdLine);
#else
		syslog(LOG_ERR, "failed to restart agent (command line: %s)", cmdLine);
#endif
	}
#endif
	return 1;
}

#ifdef _WIN32

bool ExecuteInSession(WTS_SESSION_INFO *session, TCHAR *command, bool allSessions);

/**
 * Watchdog for user agents
 */
THREAD_RESULT THREAD_CALL UserAgentWatchdog(void *arg)
{
   TCHAR *executableName = static_cast<TCHAR*>(arg);
   nxlog_debug_tag(DEBUG_TAG, 1, _T("User agent watchdog started (executable name %s)"), executableName);

   while (!AgentSleepAndCheckForShutdown(60000))
   {
      WTS_SESSION_INFO *sessions;
      DWORD sessionCount;
      if (!WTSEnumerateSessions(WTS_CURRENT_SERVER_HANDLE, 0, 1, &sessions, &sessionCount))
         continue;

      WTS_PROCESS_INFO *processes;
      DWORD processCount;
      if (!WTSEnumerateProcesses(WTS_CURRENT_SERVER_HANDLE, 0, 1, &processes, &processCount))
      {
         WTSFreeMemory(sessions);
         continue;
      }

      for (DWORD i = 0; i < sessionCount; i++)
      {
         if ((sessions[i].State != WTSActive) && (sessions[i].State != WTSConnected))
            continue;

         DWORD sessionId = sessions[i].SessionId;
         bool found = false;
         for (DWORD j = 0; j < processCount; j++)
         {
            if ((processes[j].SessionId == sessionId) && !_tcscmp(processes[j].pProcessName, executableName))
            {
               found = true;
               break;
            }
         }

         if (!found)
         {
            nxlog_debug_tag(DEBUG_TAG, 3, _T("User agent process not found in session #%u (%s)"),
                  sessionId, sessions[i].pWinStationName);
            TCHAR binDir[MAX_PATH];
            GetNetXMSDirectory(nxDirBin, binDir);

            StringBuffer command = _T("\"");
            command.append(binDir);
            command.append(_T("\\"));
            command.append(executableName);
            if (VerifyFileSignature(command.cstr() + 1)) // skip leading "
            {
               command.append(_T("\""));
               ExecuteInSession(&sessions[i], command.getBuffer(), false);
            }
            else
            {
               nxlog_write_tag(NXLOG_WARNING, DEBUG_TAG, _T("Signature validation failed for user agent executable \"%s\""), command.cstr() + 1);
            }
         }
      }

      WTSFreeMemory(sessions);
      WTSFreeMemory(processes);
   }

   MemFree(executableName);
   nxlog_debug_tag(DEBUG_TAG, 1, _T("User agent watchdog stopped"));
   return THREAD_OK;
}

#endif
