/* 
** NetXMS - Network Management System
** Client Library
** Copyright (C) 2004 Victor Kirhenshtein
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
** $module: epp.cpp
**
**/

#include "libnxcl.h"


//
// Delete event policy structure
//

void LIBNXCL_EXPORTABLE NXCDestroyEventPolicy(NXC_EPP *pEventPolicy)
{
   DWORD i;

   for(i = 0; i < pEventPolicy->dwNumRules; i++)
   {
      safe_free(pEventPolicy->pRuleList[i].pszComment);
      safe_free(pEventPolicy->pRuleList[i].pdwActionList);
      safe_free(pEventPolicy->pRuleList[i].pdwSourceList);
      safe_free(pEventPolicy->pRuleList[i].pdwEventList);
   }
   safe_free(pEventPolicy->pRuleList);
   free(pEventPolicy);
}


//
// Open and load to client event processing policy
//

DWORD LIBNXCL_EXPORTABLE NXCOpenEventPolicy(NXC_EPP **ppEventPolicy)
{
   CSCPMessage msg, *pResponce;
   DWORD i, dwRqId, dwRetCode;

   dwRqId = g_dwMsgId++;

   // Prepare message
   msg.SetCode(CMD_OPEN_EPP);
   msg.SetId(dwRqId);
   SendMsg(&msg);

   // Wait for reply
   pResponce = WaitForMessage(CMD_REQUEST_COMPLETED, dwRqId, 2000);
   if (pResponce != NULL)
   {
      dwRetCode = pResponce->GetVariableLong(VID_RCC);
      if (dwRetCode == RCC_SUCCESS)
      {
         // Prepare event policy structure
         *ppEventPolicy = (NXC_EPP *)malloc(sizeof(NXC_EPP));
         (*ppEventPolicy)->dwNumRules = pResponce->GetVariableLong(VID_NUM_RULES);
         (*ppEventPolicy)->pRuleList = 
            (NXC_EPP_RULE *)malloc(sizeof(NXC_EPP_RULE) * (*ppEventPolicy)->dwNumRules);
         memset((*ppEventPolicy)->pRuleList, 0, sizeof(NXC_EPP_RULE) * (*ppEventPolicy)->dwNumRules);
         delete pResponce;

         // Receive policy rules, each in separate message
         for(i = 0; i < (*ppEventPolicy)->dwNumRules; i++)
         {
            pResponce = WaitForMessage(CMD_EPP_RECORD, dwRqId, 2000);
            if (pResponce != NULL)
            {
               (*ppEventPolicy)->pRuleList[i].dwFlags = pResponce->GetVariableLong(VID_FLAGS);
               (*ppEventPolicy)->pRuleList[i].dwId = pResponce->GetVariableLong(VID_RULE_ID);
               (*ppEventPolicy)->pRuleList[i].pszComment = pResponce->GetVariableStr(VID_COMMENT);

               (*ppEventPolicy)->pRuleList[i].dwNumActions = 
                  pResponce->GetVariableLong(VID_NUM_ACTIONS);
               (*ppEventPolicy)->pRuleList[i].pdwActionList = 
                  (DWORD *)malloc(sizeof(DWORD) * (*ppEventPolicy)->pRuleList[i].dwNumActions);
               pResponce->GetVariableInt32Array(VID_RULE_ACTIONS, 
                                                (*ppEventPolicy)->pRuleList[i].dwNumActions,
                                                (*ppEventPolicy)->pRuleList[i].pdwActionList);

               (*ppEventPolicy)->pRuleList[i].dwNumEvents = 
                  pResponce->GetVariableLong(VID_NUM_EVENTS);
               (*ppEventPolicy)->pRuleList[i].pdwEventList = 
                  (DWORD *)malloc(sizeof(DWORD) * (*ppEventPolicy)->pRuleList[i].dwNumEvents);
               pResponce->GetVariableInt32Array(VID_RULE_EVENTS, 
                                                (*ppEventPolicy)->pRuleList[i].dwNumEvents,
                                                (*ppEventPolicy)->pRuleList[i].pdwEventList);

               (*ppEventPolicy)->pRuleList[i].dwNumSources = 
                  pResponce->GetVariableLong(VID_NUM_SOURCES);
               (*ppEventPolicy)->pRuleList[i].pdwSourceList = 
                  (DWORD *)malloc(sizeof(DWORD) * (*ppEventPolicy)->pRuleList[i].dwNumSources);
               pResponce->GetVariableInt32Array(VID_RULE_SOURCES, 
                                                (*ppEventPolicy)->pRuleList[i].dwNumSources,
                                                (*ppEventPolicy)->pRuleList[i].pdwSourceList);

               pResponce->GetVariableStr(VID_ALARM_KEY, 
                                         (*ppEventPolicy)->pRuleList[i].szAlarmKey,
                                         MAX_DB_STRING);
               pResponce->GetVariableStr(VID_ALARM_ACK_KEY, 
                                         (*ppEventPolicy)->pRuleList[i].szAlarmAckKey,
                                         MAX_DB_STRING);
               pResponce->GetVariableStr(VID_ALARM_MESSAGE, 
                                         (*ppEventPolicy)->pRuleList[i].szAlarmMessage,
                                         MAX_DB_STRING);
               (*ppEventPolicy)->pRuleList[i].wAlarmSeverity = pResponce->GetVariableShort(VID_ALARM_SEVERITY);

               delete pResponce;
            }
            else
            {
               dwRetCode = RCC_TIMEOUT;
               break;
            }
         }

         // Delete allocated policy structure if we have failed somewhere in the middle
         if (dwRetCode != RCC_SUCCESS)
            NXCDestroyEventPolicy(*ppEventPolicy);
      }
      else
      {
         delete pResponce;
      }
   }
   else
   {
      dwRetCode = RCC_TIMEOUT;
   }
   return dwRetCode;
}


//
// Close event policy (without saving)
//

DWORD LIBNXCL_EXPORTABLE NXCCloseEventPolicy(void)
{
   CSCPMessage msg;
   DWORD dwRqId;

   dwRqId = g_dwMsgId++;

   // Prepare message
   msg.SetCode(CMD_CLOSE_EPP);
   msg.SetId(dwRqId);
   SendMsg(&msg);
   
   // Wait for reply
   return WaitForRCC(dwRqId);
}


//
// Save (and install) new event policy
//

DWORD LIBNXCL_EXPORTABLE NXCSaveEventPolicy(NXC_EPP *pEventPolicy)
{
   CSCPMessage msg;
   DWORD i, dwRqId, dwRetCode;

   dwRqId = g_dwMsgId++;

   // Prepare message
   msg.SetCode(CMD_SAVE_EPP);
   msg.SetId(dwRqId);
   msg.SetVariable(VID_NUM_RULES, pEventPolicy->dwNumRules);

   // Send message and wait for responce
   SendMsg(&msg);
   dwRetCode = WaitForRCC(dwRqId);
   if (dwRetCode == RCC_SUCCESS)
   {
      // Send all event policy records, one per message
      msg.SetCode(CMD_EPP_RECORD);
      for(i = 0; i < pEventPolicy->dwNumRules; i++)
      {
         msg.DeleteAllVariables();

         msg.SetVariable(VID_FLAGS, pEventPolicy->pRuleList[i].dwFlags);
         msg.SetVariable(VID_RULE_ID, pEventPolicy->pRuleList[i].dwId);
         msg.SetVariable(VID_COMMENT, VALIDATE_STRING(pEventPolicy->pRuleList[i].pszComment));
         msg.SetVariable(VID_NUM_ACTIONS, pEventPolicy->pRuleList[i].dwNumActions);
         msg.SetVariableToInt32Array(VID_RULE_ACTIONS,
                                     pEventPolicy->pRuleList[i].dwNumActions,
                                     pEventPolicy->pRuleList[i].pdwActionList);
         msg.SetVariable(VID_NUM_EVENTS, pEventPolicy->pRuleList[i].dwNumEvents);
         msg.SetVariableToInt32Array(VID_RULE_EVENTS,
                                     pEventPolicy->pRuleList[i].dwNumEvents,
                                     pEventPolicy->pRuleList[i].pdwEventList);
         msg.SetVariable(VID_NUM_SOURCES, pEventPolicy->pRuleList[i].dwNumSources);
         msg.SetVariableToInt32Array(VID_RULE_SOURCES,
                                     pEventPolicy->pRuleList[i].dwNumSources,
                                     pEventPolicy->pRuleList[i].pdwSourceList);
         msg.SetVariable(VID_ALARM_KEY, pEventPolicy->pRuleList[i].szAlarmKey);
         msg.SetVariable(VID_ALARM_ACK_KEY, pEventPolicy->pRuleList[i].szAlarmAckKey);
         msg.SetVariable(VID_ALARM_MESSAGE, pEventPolicy->pRuleList[i].szAlarmMessage);
         msg.SetVariable(VID_ALARM_SEVERITY, pEventPolicy->pRuleList[i].wAlarmSeverity);

         SendMsg(&msg);
      }

      // Wait for final confirmation if we have sent some rules
      if (pEventPolicy->dwNumRules > 0)
         dwRetCode = WaitForRCC(dwRqId);
   }
   return dwRetCode;
}


//
// Delete rule from policy
//

void LIBNXCL_EXPORTABLE NXCDeletePolicyRule(NXC_EPP *pEventPolicy, DWORD dwRule)
{
   if (dwRule < pEventPolicy->dwNumRules)
   {
      safe_free(pEventPolicy->pRuleList[dwRule].pdwActionList);
      safe_free(pEventPolicy->pRuleList[dwRule].pdwEventList);
      safe_free(pEventPolicy->pRuleList[dwRule].pdwSourceList);
      safe_free(pEventPolicy->pRuleList[dwRule].pszComment);
      pEventPolicy->dwNumRules--;
      memmove(&pEventPolicy->pRuleList[dwRule], &pEventPolicy->pRuleList[dwRule + 1],
              sizeof(NXC_EPP_RULE) * (pEventPolicy->dwNumRules - dwRule));
   }
}
