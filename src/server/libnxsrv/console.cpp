/*
** NetXMS - Network Management System
** Copyright (C) 2003-2019 Victor Kirhenshtein
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU Lesser General Public License as published by
** the Free Software Foundation; either version 3 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU Lesser General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
** File: console.cpp
**
**/

#include "libnxsrv.h"
#include <server_console.h>

/**
 * Base console constructor
 */
ServerConsole::ServerConsole()
{
   m_remote = true;
}

/**
 * Base console destructor
 */
ServerConsole::~ServerConsole()
{
}

/**
 * Print console message with formatting
 */
void ServerConsole::printf(const TCHAR *format, ...)
{
   va_list args;
   va_start(args, format);
   vprintf(format, args);
   va_end(args);
}

/**
 * Print console message with formatting
 */
void ServerConsole::vprintf(const TCHAR *format, va_list args)
{
   TCHAR buffer[16384];
   _vsntprintf(buffer, 16384, format, args);
   buffer[16383] = 0;
   write(buffer);
}

/**
 * Local terminal console constructor
 */
LocalTerminalConsole::LocalTerminalConsole()
{
   m_remote = false;
}

/**
 * Local terminal console destructor
 */
LocalTerminalConsole::~LocalTerminalConsole()
{
}

/**
 * Write to local terminal console
 */
void LocalTerminalConsole::write(const TCHAR *text)
{
   WriteToTerminal(text);
}

/**
 * String buffer console constructor
 */
StringBufferConsole::StringBufferConsole()
{
   m_mutex = MutexCreate();
}

/**
 * String buffer console destructor
 */
StringBufferConsole::~StringBufferConsole()
{
   MutexDestroy(m_mutex);
}

/**
 * Write to local string buffer console
 */
void StringBufferConsole::write(const TCHAR *text)
{
   size_t start = 0, i;
   MutexLock(m_mutex);
   for(i = 0; text[i] != 0; i++)
   {
      if (text[i] == 27)
      {
         m_buffer.append(&text[start], i - start);
         i++;
         if (text[i] == '[')
         {
            for(i++; (text[i] != 0) && (text[i] != 'm'); i++);
            if (text[i] == 'm')
               i++;
         }
         start = i;
         i--;
      }
   }
   m_buffer.append(&text[start], i - start);
   MutexUnlock(m_mutex);
}

/**
 * Socket console constructor
 */
SocketConsole::SocketConsole(SOCKET s, UINT16 msgCode)
{
   m_socket = s;
   m_mutex = MutexCreate();
   m_messageCode = msgCode;
}

/**
 * Socket console destructor
 */
SocketConsole::~SocketConsole()
{
   MutexDestroy(m_mutex);
}

/**
 * Write to socket console
 */
void SocketConsole::write(const TCHAR *text)
{
   NXCPMessage msg(m_messageCode, 0);
   msg.setField(VID_MESSAGE, text);
   NXCP_MESSAGE *rawMsg = msg.serialize();
   SendEx(m_socket, rawMsg, ntohl(rawMsg->size), 0, m_mutex);
   MemFree(rawMsg);
}
