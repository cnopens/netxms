/* 
** NetXMS - Network Management System
** Driver for Juniper Networks switches
** Copyright (C) 2003-2017 Victor Kirhenshtein
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
** File: juniper.cpp
**/

#include "juniper.h"

/**
 * Driver name
 */
static TCHAR s_driverName[] = _T("JUNIPER");

/**
 * Driver version
 */
static TCHAR s_driverVersion[] = NETXMS_VERSION_STRING;

/**
 * Get driver name
 */
const TCHAR *JuniperDriver::getName()
{
	return s_driverName;
}

/**
 * Get driver version
 */
const TCHAR *JuniperDriver::getVersion()
{
	return s_driverVersion;
}

/**
 * Check if given device can be potentially supported by driver
 *
 * @param oid Device OID
 */
int JuniperDriver::isPotentialDevice(const TCHAR *oid)
{
	return (_tcsncmp(oid, _T(".1.3.6.1.4.1.2636.1."), 20) == 0) ? 255 : 0;
}

/**
 * Check if given device is supported by driver
 *
 * @param snmp SNMP transport
 * @param oid Device OID
 */
bool JuniperDriver::isDeviceSupported(SNMP_Transport *snmp, const TCHAR *oid)
{
	return true;
}

/**
 * Do additional checks on the device required by driver.
 * Driver can set device's custom attributes from within
 * this function.
 *
 * @param snmp SNMP transport
 * @param attributes Node's custom attributes
 */
void JuniperDriver::analyzeDevice(SNMP_Transport *snmp, const TCHAR *oid, StringMap *attributes, DriverData **driverData)
{
}

/**
 * Handler for walking slot numbers
 */
static UINT32 SlotWalkHandler(SNMP_Variable *var, SNMP_Transport *snmp, void *arg)
{
   int slot = var->getValueAsInt();
   if (slot > 0)
   {
      InterfaceList *ifList = (InterfaceList *)arg;
      InterfaceInfo *iface = ifList->findByIfIndex(var->getName().getElement(var->getName().length() - 1));
      if (iface != NULL)
      {
         iface->isPhysicalPort = true;
         iface->slot = slot - 1;  // Juniper numbers slots from 0 but reports in SNMP as n + 1
      }
   }
   return SNMP_ERR_SUCCESS;
}

/**
 * Handler for walking port numbers
 */
static UINT32 PortWalkHandler(SNMP_Variable *var, SNMP_Transport *snmp, void *arg)
{
   int port = var->getValueAsInt();
   if (port > 0)
   {
      InterfaceList *ifList = (InterfaceList *)arg;
      InterfaceInfo *iface = ifList->findByIfIndex(var->getName().getElement(var->getName().length() - 1));
      if (iface != NULL)
      {
         iface->isPhysicalPort = true;
         iface->port = port - 1;  // Juniper numbers ports from 0 but reports in SNMP as n + 1
      }
   }
   return SNMP_ERR_SUCCESS;
}

/**
 * Get list of interfaces for given node
 *
 * @param snmp SNMP transport
 * @param attributes Node's custom attributes
 */
InterfaceList *JuniperDriver::getInterfaces(SNMP_Transport *snmp, StringMap *attributes, DriverData *driverData, int useAliases, bool useIfXTable)
{
	// Get interface list from standard MIB
	InterfaceList *ifList = NetworkDeviceDriver::getInterfaces(snmp, attributes, driverData, useAliases, useIfXTable);
	if (ifList == NULL)
		return NULL;

	// Slot numbers
   SnmpWalk(snmp, _T(".1.3.6.1.4.1.2636.3.3.2.1.1"), SlotWalkHandler, ifList);

   // Port numbers
   SnmpWalk(snmp, _T(".1.3.6.1.4.1.2636.3.3.2.1.3"), PortWalkHandler, ifList);

	return ifList;
}

/**
 * Get orientation of the modules in the device
 *
 * @param snmp SNMP transport
 * @param attributes Node's custom attributes
 * @param driverData driver-specific data previously created in analyzeDevice
 * @return module orientation
 */
int JuniperDriver::getModulesOrientation(SNMP_Transport *snmp, StringMap *attributes, DriverData *driverData)
{
   return NDD_ORIENTATION_HORIZONTAL;
}

/**
 * Get port layout of given module
 * @param snmp SNMP transport
 * @param attributes Node's custom attributes
 * @param driverData driver-specific data previously created in analyzeDevice
 * @param module Module number (starting from 1)
 * @param layout Layout structure to fill
 */
void JuniperDriver::getModuleLayout(SNMP_Transport *snmp, StringMap *attributes, DriverData *driverData, int module, NDD_MODULE_LAYOUT *layout)
{
   layout->numberingScheme = NDD_PN_UD_LR;
   layout->rows = 2;
}

/**
 * Driver entry point
 */
DECLARE_NDD_ENTRY_POINT(s_driverName, JuniperDriver);

#ifdef _WIN32

/**
 * DLL entry point
 */
BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
		DisableThreadLibraryCalls(hInstance);
	return TRUE;
}

#endif
