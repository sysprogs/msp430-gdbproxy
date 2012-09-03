// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>

/*! \mainpage The MSP430 GDB server for JTAG debugging
	
	\section Introduction
	This project allows debugging MSP430 firmware with GDB using the original drivers and DLL from TI.
	The project is based on the <a href="http://sysprogs.com/GDBServerFoundation">GDBServerFoundation</a> framework.

	The main functionality of this project is implemented in the following items:
	1. The GDBServerFoundation::MSP430::RegisterList list defines the MSP430 registers in the order expected by GDB.
	2. The \ref MSP430Proxy::MSP430GDBTarget "MSP430GDBTarget" class implements the MSP430 debugging functionality without EEM support (no advanced breakpoints).
	3. The \ref MSP430Proxy::MSP430EEMTarget "MSP430EEMTarget" class implements the EEM-related functionality (software breakpoints, data breakpoints, etc.).
	4. The \ref MSP430Proxy::SoftwareBreakpointManager "SoftwareBreakpointManager" class manages setting and removing of software breakpoints in FLASH.
*/
