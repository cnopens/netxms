/*
** NetXMS - Network Management System
** Copyright (C) 2003-2018 Victor Kirhenshtein
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
** File: vobject.cpp
**
**/

#include "nxcore.h"

/**
 * Versionable object constructor
 */
VersionableObject::VersionableObject(NetObj *parent)
{
   m_parent = parent;
   m_version = 0;
   m_mutexProperties = MutexCreate();
}

/**
 * Versionable object constructor
 */
VersionableObject::VersionableObject(NetObj *parent, ConfigEntry *config)
{
   m_parent = parent;
   m_version = config->getSubEntryValueAsUInt(_T("version"), 0, 0x00010000);
   m_mutexProperties = MutexCreate();
}

/**
 * AutoApply object destructor
 */
VersionableObject::~VersionableObject()
{
   MutexDestroy(m_mutexProperties);
}

/**
 * Create NXCP message with object's data
 */
void VersionableObject::fillMessageInternal(NXCPMessage *pMsg, UINT32 userId)
{
   pMsg->setField(VID_VERSION, m_version);
}


bool VersionableObject::saveToDatabase(DB_HANDLE hdb)
{
   bool success = false;

   DB_STATEMENT hStmt;
   if (IsDatabaseRecordExist(hdb, _T("versionable_object"), _T("object_id"), m_parent->getId()))
   {
      hStmt = DBPrepare(hdb, _T("UPDATE versionable_object SET version=? WHERE object_id=?"));
   }
   else
   {
      hStmt = DBPrepare(hdb, _T("INSERT INTO versionable_object (version,object_id) VALUES (?,?)"));
   }
   if (hStmt != NULL)
   {
      internalLock();
      DBBind(hStmt, 1, DB_SQLTYPE_INTEGER, m_version);
      DBBind(hStmt, 2, DB_SQLTYPE_INTEGER, m_parent->getId());
      internalUnlock();
      success = DBExecute(hStmt);
      DBFreeStatement(hStmt);
   }

   return success;
}

bool VersionableObject::deleteFromDatabase(DB_HANDLE hdb)
{
   return ExecuteQueryOnObject(hdb, m_parent->getId(), _T("DELETE FROM versionable_object WHERE object_id=?"));
}

bool VersionableObject::loadFromDatabase(DB_HANDLE hdb, UINT32 id)
{
   TCHAR szQuery[256];

   _sntprintf(szQuery, sizeof(szQuery) / sizeof(TCHAR), _T("SELECT version FROM versionable_object WHERE object_id=%d"), m_parent->getId());
   DB_RESULT hResult = DBSelect(hdb, szQuery);
   if (hResult == NULL)
      return false;

   m_version = DBGetFieldLong(hResult, 0, 0);
   DBFreeResult(hResult);
   return true;
}

void VersionableObject::updateFromImport(ConfigEntry *config)
{
   internalLock();
   m_version = config->getSubEntryValueAsUInt(_T("version"), 0, m_version);
   internalUnlock();
}

/**
 * Serialize object to JSON
 */
void VersionableObject::toJson(json_t *root)
{
   json_object_set_new(root, "version", json_integer(m_version));
}
