/* 
** NetXMS - Network Management System
** Server Library
** Copyright (C) 2003, 2004 Victor Kirhenshtein
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
** $module: agent.cpp
**
**/

#include "libnxsrv.h"
#include <stdarg.h>


//
// Constants
//

#define RECEIVER_BUFFER_SIZE        262144


//
// Receiver thread starter
//

void AgentConnection::ReceiverThreadStarter(void *pArg)
{
   ((AgentConnection *)pArg)->ReceiverThread();
}


//
// Default constructor for AgentConnection - normally shouldn't be used
//

AgentConnection::AgentConnection()
{
   m_dwAddr = inet_addr("127.0.0.1");
   m_wPort = AGENT_LISTEN_PORT;
   m_iAuthMethod = AUTH_NONE;
   m_szSecret[0] = 0;
   m_hSocket = -1;
   m_tLastCommandTime = 0;
   m_dwNumDataLines = 0;
   m_ppDataLines = NULL;
   m_pMsgWaitQueue = new MsgWaitQueue;
   m_dwRequestId = 1;
   m_dwCommandTimeout = 10000;   // Default timeout 10 seconds
   m_bIsConnected = FALSE;
   m_mutexDataLock = MutexCreate();
   m_mutexReceiverThreadRunning = MutexCreate();
}


//
// Normal constructor for AgentConnection
//

AgentConnection::AgentConnection(DWORD dwAddr, WORD wPort, int iAuthMethod, char *szSecret)
{
   m_dwAddr = dwAddr;
   m_wPort = wPort;
   m_iAuthMethod = iAuthMethod;
   if (szSecret != NULL)
      strncpy(m_szSecret, szSecret, MAX_SECRET_LENGTH);
   else
      m_szSecret[0] = 0;
   m_hSocket = -1;
   m_tLastCommandTime = 0;
   m_dwNumDataLines = 0;
   m_ppDataLines = NULL;
   m_pMsgWaitQueue = new MsgWaitQueue;
   m_dwRequestId = 1;
   m_dwCommandTimeout = 10000;   // Default timeout 10 seconds
   m_bIsConnected = FALSE;
   m_mutexDataLock = MutexCreate();
   m_mutexReceiverThreadRunning = MutexCreate();
}


//
// Destructor
//

AgentConnection::~AgentConnection()
{
   // Destroy socket
   if (m_hSocket != -1)
      closesocket(m_hSocket);

   // Wait for receiver thread termination
   MutexLock(m_mutexReceiverThreadRunning, INFINITE);
   MutexUnlock(m_mutexReceiverThreadRunning);

   Lock();
   DestroyResultData();
   Unlock();

   delete m_pMsgWaitQueue;

   MutexDestroy(m_mutexDataLock);
   MutexDestroy(m_mutexReceiverThreadRunning);
}


//
// Print message. This function is virtual and can be overrided in
// derived classes. Default implementation will print message to stdout.
//

void AgentConnection::PrintMsg(char *pszFormat, ...)
{
   va_list args;

   va_start(args, pszFormat);
   vprintf(pszFormat, args);
   va_end(args);
   printf("\n");
}


//
// Receiver thread
//

void AgentConnection::ReceiverThread(void)
{
   CSCPMessage *pMsg;
   CSCP_MESSAGE *pRawMsg;
   CSCP_BUFFER *pMsgBuffer;
   int iErr;
   char szBuffer[128];

   MutexLock(m_mutexReceiverThreadRunning, INFINITE);

   // Initialize raw message receiving function
   pMsgBuffer = (CSCP_BUFFER *)malloc(sizeof(CSCP_BUFFER));
   RecvCSCPMessage(0, NULL, pMsgBuffer, 0);

   // Allocate space for raw message
   pRawMsg = (CSCP_MESSAGE *)malloc(RECEIVER_BUFFER_SIZE);

   // Message receiving loop
   while(1)
   {
      // Receive raw message
      if ((iErr = RecvCSCPMessage(m_hSocket, pRawMsg, pMsgBuffer, RECEIVER_BUFFER_SIZE)) <= 0)
         break;

      // Check if we get too large message
      if (iErr == 1)
      {
         PrintMsg("Received too large message %s (%ld bytes)", 
                  CSCPMessageCodeName(ntohs(pRawMsg->wCode), szBuffer),
                  ntohl(pRawMsg->dwSize));
         continue;
      }

      // Check that actual received packet size is equal to encoded in packet
      if ((int)ntohl(pRawMsg->dwSize) != iErr)
      {
         PrintMsg("RecvMsg: Bad packet length [dwSize=%d ActualSize=%d]", ntohl(pRawMsg->dwSize), iErr);
         continue;   // Bad packet, wait for next
      }

      // Create message object from raw message
      pMsg = new CSCPMessage(pRawMsg);
      if (pMsg->GetCode() == CMD_TRAP)
      {
         OnTrap(pMsg);
         delete pMsg;
      }
      else
      {
         m_pMsgWaitQueue->Put(pMsg);
      }
   }

   // Close socket and mark connection as disconnected
   Disconnect();

   free(pRawMsg);
   free(pMsgBuffer);

   MutexUnlock(m_mutexReceiverThreadRunning);
}


//
// Connect to agent
//

BOOL AgentConnection::Connect(BOOL bVerbose)
{
   struct sockaddr_in sa;
   char szBuffer[256];
   BOOL bSuccess = FALSE;
   DWORD dwError;

   // Check if already connected
   if ((m_bIsConnected) || (m_hSocket != -1))
      return FALSE;

   // Create socket
   m_hSocket = socket(AF_INET, SOCK_STREAM, 0);
   if (m_hSocket == -1)
   {
      PrintMsg("Call to socket() failed");
      goto connect_cleanup;
   }

   // Fill in address structure
   memset(&sa, 0, sizeof(sa));
   sa.sin_addr.s_addr = m_dwAddr;
   sa.sin_family = AF_INET;
   sa.sin_port = htons(m_wPort);

   // Connect to server
   if (connect(m_hSocket, (struct sockaddr *)&sa, sizeof(sa)) == -1)
   {
      if (bVerbose)
         PrintMsg("Cannot establish connection with agent %s", IpToStr(m_dwAddr, szBuffer));
      goto connect_cleanup;
   }

   // Start receiver thread
   ThreadCreate(ReceiverThreadStarter, 0, this);

   // Authenticate itself to agent
   if ((dwError = Authenticate()) != ERR_SUCCESS)
   {
      PrintMsg("Authentication to agent %s failed (%s)", IpToStr(m_dwAddr, szBuffer),
               AgentErrorCodeToText(dwError));
      goto connect_cleanup;
   }

   // Test connectivity
   if ((dwError = Nop()) != ERR_SUCCESS)
   {
      PrintMsg("Communication with agent %s failed (%s)", IpToStr(m_dwAddr, szBuffer),
               AgentErrorCodeToText(dwError));
      goto connect_cleanup;
   }

   bSuccess = TRUE;

connect_cleanup:
   if (!bSuccess)
   {
      Lock();
      if (m_hSocket != -1)
      {
         shutdown(m_hSocket, 2);
         closesocket(m_hSocket);
         m_hSocket = -1;
      }
      Unlock();
   }
   m_bIsConnected = bSuccess;
   return bSuccess;
}


//
// Disconnect from agent
//

void AgentConnection::Disconnect(void)
{
   Lock();
   if (m_hSocket != -1)
   {
      shutdown(m_hSocket, 2);
      closesocket(m_hSocket);
      m_hSocket = -1;
   }
   DestroyResultData();
   m_bIsConnected = FALSE;
   Unlock();
}


//
// Destroy command execuion results data
//

void AgentConnection::DestroyResultData(void)
{
   DWORD i;

   if (m_ppDataLines != NULL)
   {
      for(i = 0; i < m_dwNumDataLines; i++)
         if (m_ppDataLines[i] != NULL)
            free(m_ppDataLines[i]);
      free(m_ppDataLines);
      m_ppDataLines = NULL;
   }
   m_dwNumDataLines = 0;
}


//
// Get interface list from agent
//

INTERFACE_LIST *AgentConnection::GetInterfaceList(void)
{
   INTERFACE_LIST *pIfList = NULL;
   DWORD i;
   char *pChar, *pBuf;

   if (GetList("InterfaceList") == ERR_SUCCESS)
   {
      pIfList = (INTERFACE_LIST *)malloc(sizeof(INTERFACE_LIST));
      pIfList->iNumEntries = m_dwNumDataLines;
      pIfList->pInterfaces = (INTERFACE_INFO *)malloc(sizeof(INTERFACE_INFO) * m_dwNumDataLines);
      memset(pIfList->pInterfaces, 0, sizeof(INTERFACE_INFO) * m_dwNumDataLines);
      for(i = 0; i < m_dwNumDataLines; i++)
      {
         pBuf = m_ppDataLines[i];

         // Index
         pChar = strchr(pBuf, ' ');
         if (pChar != NULL)
         {
            *pChar = 0;
            pIfList->pInterfaces[i].dwIndex = strtoul(pBuf, NULL, 10);
            pBuf = pChar + 1;
         }

         // Address and mask
         pChar = strchr(pBuf, ' ');
         if (pChar != NULL)
         {
            char *pSlash;

            *pChar = 0;
            pSlash = strchr(pBuf, '/');
            if (pSlash != NULL)
            {
               *pSlash = 0;
               pSlash++;
            }
            else     // Just a paranoia protection, should'n happen if agent working correctly
            {
               pSlash = "24";
            }
            pIfList->pInterfaces[i].dwIpAddr = inet_addr(pBuf);
            pIfList->pInterfaces[i].dwIpNetMask = htonl(~(0xFFFFFFFF >> strtoul(pSlash, NULL, 10)));
            pBuf = pChar + 1;
         }

         // Interface type
         pChar = strchr(pBuf, ' ');
         if (pChar != NULL)
         {
            *pChar = 0;
            pIfList->pInterfaces[i].dwIndex = strtoul(pBuf, NULL, 10);
            pBuf = pChar + 1;
         }

         // Name
         strncpy(pIfList->pInterfaces[i].szName, pBuf, MAX_OBJECT_NAME - 1);
      }

      Lock();
      DestroyResultData();
      Unlock();
   }

   return pIfList;
}


//
// Get parameter value
//

DWORD AgentConnection::GetParameter(char *pszParam, DWORD dwBufSize, char *pszBuffer)
{
   CSCPMessage msg, *pResponce;
   DWORD dwRqId, dwRetCode;

   if (m_bIsConnected)
   {
      dwRqId = m_dwRequestId++;
      msg.SetCode(CMD_GET_PARAMETER);
      msg.SetId(dwRqId);
      msg.SetVariable(VID_PARAMETER, pszParam);
      if (SendMessage(&msg))
      {
         pResponce = WaitForMessage(CMD_REQUEST_COMPLETED, dwRqId, m_dwCommandTimeout);
         if (pResponce != NULL)
         {
            dwRetCode = pResponce->GetVariableLong(VID_RCC);
            if (dwRetCode == ERR_SUCCESS)
               pResponce->GetVariableStr(VID_VALUE, pszBuffer, dwBufSize);
            delete pResponce;
         }
         else
         {
            dwRetCode = ERR_REQUEST_TIMEOUT;
         }
      }
      else
      {
         dwRetCode = ERR_CONNECTION_BROKEN;
      }
   }
   else
   {
      dwRetCode = ERR_NOT_CONNECTED;
   }

   return dwRetCode;
}


//
// Get ARP cache
//

ARP_CACHE *AgentConnection::GetArpCache(void)
{
   ARP_CACHE *pArpCache = NULL;
   char szByte[4], *pBuf, *pChar;
   DWORD i, j;

   if (GetList("ArpCache") == ERR_SUCCESS)
   {
      // Create empty structure
      pArpCache = (ARP_CACHE *)malloc(sizeof(ARP_CACHE));
      pArpCache->dwNumEntries = m_dwNumDataLines;
      pArpCache->pEntries = (ARP_ENTRY *)malloc(sizeof(ARP_ENTRY) * m_dwNumDataLines);
      memset(pArpCache->pEntries, 0, sizeof(ARP_ENTRY) * m_dwNumDataLines);

      szByte[2] = 0;

      // Parse data lines
      // Each line has form of XXXXXXXXXXXX a.b.c.d
      // where XXXXXXXXXXXX is a MAC address (12 hexadecimal digits)
      // and a.b.c.d is an IP address in decimal dotted notation
      for(i = 0; i < m_dwNumDataLines; i++)
      {
         pBuf = m_ppDataLines[i];
         if (strlen(pBuf) < 20)     // Invalid line
            continue;

         // MAC address
         for(j = 0; j < 6; j++)
         {
            memcpy(szByte, pBuf, 2);
            pArpCache->pEntries[i].bMacAddr[j] = (BYTE)strtol(szByte, NULL, 16);
            pBuf+=2;
         }

         // IP address
         while(*pBuf == ' ')
            pBuf++;
         pChar = strchr(pBuf, ' ');
         if (pChar != NULL)
            *pChar = 0;
         pArpCache->pEntries[i].dwIpAddr = inet_addr(pBuf);

         // Interface index
         if (pChar != NULL)
            pArpCache->pEntries[i].dwIndex = strtoul(pChar + 1, NULL, 10);
      }

      DestroyResultData();
   }
   return pArpCache;
}


//
// Send dummy command to agent (can be used for keepalive)
//

DWORD AgentConnection::Nop(void)
{
   CSCPMessage msg;
   DWORD dwRqId;

   dwRqId = m_dwRequestId++;
   msg.SetCode(CMD_KEEPALIVE);
   msg.SetId(dwRqId);
   if (SendMessage(&msg))
      return WaitForRCC(dwRqId, m_dwCommandTimeout);
   else
      return ERR_CONNECTION_BROKEN;
}


//
// Wait for request completion code
//

DWORD AgentConnection::WaitForRCC(DWORD dwRqId, DWORD dwTimeOut)
{
   CSCPMessage *pMsg;
   DWORD dwRetCode;

   pMsg = m_pMsgWaitQueue->WaitForMessage(CMD_REQUEST_COMPLETED, dwRqId, dwTimeOut);
   if (pMsg != NULL)
   {
      dwRetCode = pMsg->GetVariableLong(VID_RCC);
      delete pMsg;
   }
   else
   {
      dwRetCode = ERR_REQUEST_TIMEOUT;
   }
   return dwRetCode;
}


//
// Send message to agent
//

BOOL AgentConnection::SendMessage(CSCPMessage *pMsg)
{
   CSCP_MESSAGE *pRawMsg;
   BOOL bResult;

   pRawMsg = pMsg->CreateMessage();
   bResult = (send(m_hSocket, (char *)pRawMsg, ntohl(pRawMsg->dwSize), 0) == (int)ntohl(pRawMsg->dwSize));
   free(pRawMsg);
   return bResult;
}


//
// Trap handler. Should be overriden in derived classes to implement
// actual trap processing. Default implementation do nothing.
//

void AgentConnection::OnTrap(CSCPMessage *pMsg)
{
}


//
// Get list of values
//

DWORD AgentConnection::GetList(char *pszParam)
{
   CSCPMessage msg, *pResponce;
   DWORD i, dwRqId, dwRetCode;

   if (m_bIsConnected)
   {
      DestroyResultData();
      dwRqId = m_dwRequestId++;
      msg.SetCode(CMD_GET_LIST);
      msg.SetId(dwRqId);
      msg.SetVariable(VID_PARAMETER, pszParam);
      if (SendMessage(&msg))
      {
         pResponce = WaitForMessage(CMD_REQUEST_COMPLETED, dwRqId, m_dwCommandTimeout);
         if (pResponce != NULL)
         {
            dwRetCode = pResponce->GetVariableLong(VID_RCC);
            if (dwRetCode == ERR_SUCCESS)
            {
               m_dwNumDataLines = pResponce->GetVariableLong(VID_NUM_STRINGS);
               m_ppDataLines = (char **)malloc(sizeof(char *) * m_dwNumDataLines);
               for(i = 0; i < m_dwNumDataLines; i++)
                  m_ppDataLines[i] = pResponce->GetVariableStr(VID_ENUM_VALUE_BASE + i);
            }
            delete pResponce;
         }
         else
         {
            dwRetCode = ERR_REQUEST_TIMEOUT;
         }
      }
      else
      {
         dwRetCode = ERR_CONNECTION_BROKEN;
      }
   }
   else
   {
      dwRetCode = ERR_NOT_CONNECTED;
   }

   return dwRetCode;
}


//
// Authenticate to agent
//

DWORD AgentConnection::Authenticate(void)
{
   CSCPMessage msg;
   DWORD dwRqId;
   BYTE hash[32];

   if (m_iAuthMethod == AUTH_NONE)
      return ERR_SUCCESS;  // No authentication required

   dwRqId = m_dwRequestId++;
   msg.SetCode(CMD_AUTHENTICATE);
   msg.SetId(dwRqId);
   msg.SetVariable(VID_AUTH_METHOD, (WORD)m_iAuthMethod);
   switch(m_iAuthMethod)
   {
      case AUTH_PLAINTEXT:
         msg.SetVariable(VID_SHARED_SECRET, m_szSecret);
         break;
      case AUTH_MD5_HASH:
         CalculateMD5Hash((BYTE *)m_szSecret, strlen(m_szSecret), hash);
         msg.SetVariable(VID_SHARED_SECRET, hash, MD5_DIGEST_SIZE);
         break;
      case AUTH_SHA1_HASH:
         CalculateSHA1Hash((BYTE *)m_szSecret, strlen(m_szSecret), hash);
         msg.SetVariable(VID_SHARED_SECRET, hash, SHA1_DIGEST_SIZE);
         break;
      default:
         break;
   }
   if (SendMessage(&msg))
      return WaitForRCC(dwRqId, m_dwCommandTimeout);
   else
      return ERR_CONNECTION_BROKEN;
}


//
// Execute action on agent
//

DWORD AgentConnection::ExecAction(char *pszAction, int argc, char **argv)
{
   CSCPMessage msg;
   DWORD dwRqId;
   int i;

   if (!m_bIsConnected)
      return ERR_NOT_CONNECTED;

   dwRqId = m_dwRequestId++;
   msg.SetCode(CMD_ACTION);
   msg.SetId(dwRqId);
   msg.SetVariable(VID_ACTION_NAME, pszAction);
   msg.SetVariable(VID_NUM_ARGS, (DWORD)argc);
   for(i = 0; i < argc; i++)
      msg.SetVariable(VID_ACTION_ARG_BASE + i, argv[i]);

   if (SendMessage(&msg))
      return WaitForRCC(dwRqId, m_dwCommandTimeout);
   else
      return ERR_CONNECTION_BROKEN;
}
