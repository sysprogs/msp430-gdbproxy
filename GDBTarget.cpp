#include "stdafx.h"
#include "GDBTarget.h"
#include "MSP430Util.h"

#include <MSP430_EEM.h>

using namespace GDBServerFoundation;
using namespace MSP430Proxy;

#define REPORT_AND_RETURN(msg, result) { ReportLastMSP430Error(msg); return result; }

enum {kMSP430_NOP = 0x4303,
	  kMSP430_BREAKPOINT = 0x0000};

bool MSP430Proxy::MSP430GDBTarget::Initialize( const char *pPortName )
{
	if (m_bClosePending)
		return m_bValid;

	LONG version = 0;
	if (MSP430_Initialize((char *)pPortName, &version) != STATUS_OK)
		REPORT_AND_RETURN("Cannot initialize MSP430.DLL", false);
	m_bClosePending = true;

	if (MSP430_VCC(3333) != STATUS_OK)
		REPORT_AND_RETURN("Cannot enable Vcc", false);

	if (MSP430_Identify((char *)&m_DeviceInfo, sizeof(m_DeviceInfo), DEVICE_UNKNOWN) != STATUS_OK)
		REPORT_AND_RETURN("Cannot identify the MSP430 device", false);

	if (MSP430_Reset(ALL_RESETS, FALSE, FALSE) != STATUS_OK)
		REPORT_AND_RETURN("Cannot reset the MSP430 device", false);

	m_UsedBreakpoints.resize(m_DeviceInfo.nBreakpoints);

	m_bValid = true;
	return true;
}

MSP430Proxy::MSP430GDBTarget::~MSP430GDBTarget()
{
	if (m_bClosePending)
	{
		printf("GDB Disconnected. Releasing MSP430 interface.\n");
		MSP430_Close(FALSE);
	}
}

bool MSP430Proxy::MSP430GDBTarget::WaitForJTAGEvent()
{
	for (;;)
	{
		LONG state = 0;
		if (MSP430_State(&state, m_BreakInPending, NULL) != STATUS_OK)
			REPORT_AND_RETURN("Cannot query device state", false);
		if (state != RUNNING)
		{
			m_BreakInPending = false;
			return true;
		}
	}
}

GDBServerFoundation::GDBStatus MSP430Proxy::MSP430GDBTarget::ExecuteRemoteCommand( const std::string &command, std::string &output )
{
	if (command == "help")
	{
		output = "Supported stub commands:\n\
\tmon help      - Display this message\n\
\tmon erase     - Erase the FLASH memory\n";
		return kGDBSuccess;
	}
	else if (command == "erase")
	{
		if (MSP430_Erase(ERASE_MAIN, m_DeviceInfo.mainStart, m_DeviceInfo.mainEnd - m_DeviceInfo.mainStart) != STATUS_OK)
		{
			output = "Cannot erase FLASH: ";
			output += GetLastMSP430Error();
			output += "\n";
		}
		else
		{
			output = "Flash memory erased. Run \"load\" to program your binary.\n";
			m_bFLASHErased = true;
		}

		return kGDBSuccess;
	}
	else
		return kGDBNotSupported;
}

GDBServerFoundation::GDBStatus MSP430Proxy::MSP430GDBTarget::GetEmbeddedMemoryRegions( std::vector<EmbeddedMemoryRegion> &regions )
{
	if (m_DeviceInfo.mainStart || m_DeviceInfo.mainEnd)
		regions.push_back(EmbeddedMemoryRegion(mtFLASH, m_DeviceInfo.mainStart, m_DeviceInfo.mainEnd - m_DeviceInfo.mainStart + 1));
	if (m_DeviceInfo.ramStart || m_DeviceInfo.ramEnd)
		regions.push_back(EmbeddedMemoryRegion(mtRAM, m_DeviceInfo.ramStart, m_DeviceInfo.ramEnd- m_DeviceInfo.ramStart + 1));
	if (m_DeviceInfo.ram2Start || m_DeviceInfo.ram2End)
		regions.push_back(EmbeddedMemoryRegion(mtRAM, m_DeviceInfo.ram2Start, m_DeviceInfo.ram2End- m_DeviceInfo.ram2Start + 1));
	return kGDBSuccess;
}

GDBServerFoundation::GDBStatus MSP430GDBTarget::GetLastStopRecord( TargetStopRecord *pRec )
{
	pRec->Reason = kSignalReceived;
	pRec->Extension.SignalNumber = SIGTRAP;
	return kGDBSuccess;
}

GDBServerFoundation::GDBStatus MSP430GDBTarget::ResumeAndWait( int threadID )
{
	if (MSP430_Run(RUN_TO_BREAKPOINT, FALSE) != STATUS_OK)
		REPORT_AND_RETURN("Cannot resume device", kGDBUnknownError);

	if (!WaitForJTAGEvent())
		return kGDBUnknownError;
	return kGDBSuccess;
}

GDBServerFoundation::GDBStatus MSP430GDBTarget::Step( int threadID )
{
	if (MSP430_Run(SINGLE_STEP, FALSE) != STATUS_OK)
		REPORT_AND_RETURN("Cannot perform single stepping", kGDBUnknownError);

	if (!WaitForJTAGEvent())
		return kGDBUnknownError;

	return kGDBSuccess;
}

GDBServerFoundation::GDBStatus MSP430GDBTarget::SendBreakInRequestAsync()
{
	m_BreakInPending = true;
	return kGDBSuccess;
}

GDBServerFoundation::GDBStatus MSP430GDBTarget::ReadFrameRelatedRegisters( int threadID, TargetRegisterValues &registers )
{
	LONG rawRegs[16];
	if (MSP430_Read_Registers(rawRegs, MASKREG(PC) | MASKREG(SP)) != STATUS_OK)
		REPORT_AND_RETURN("Cannot read frame-related registers", kGDBUnknownError);

	registers[PC] = RegisterValue(rawRegs[PC], 2);
	registers[SP] = RegisterValue(rawRegs[SP], 2);

	return kGDBSuccess;
}

GDBServerFoundation::GDBStatus MSP430GDBTarget::ReadTargetRegisters( int threadID, TargetRegisterValues &registers )
{
	LONG rawRegs[16];
	if (MSP430_Read_Registers(rawRegs, ALL_REGS) != STATUS_OK)
		REPORT_AND_RETURN("Cannot read device registers", kGDBUnknownError);

	for (size_t i = 0; i < __countof(rawRegs); i++)
		registers[i] = RegisterValue(rawRegs[i], 16);
	
	return kGDBSuccess;
}

GDBServerFoundation::GDBStatus MSP430GDBTarget::WriteTargetRegisters( int threadID, const TargetRegisterValues &registers )
{
	LONG rawRegs[16];
	int mask = 0;

	for (size_t i = 0; i < 16; i++)
		if (registers[i].Valid)
		{
			mask |= MASKREG(i);
			rawRegs[i] = registers[i].ToUInt32();
		}

	if (MSP430_Write_Registers(rawRegs, mask) != STATUS_OK)
		REPORT_AND_RETURN("Cannot write device registers", kGDBUnknownError);

	return kGDBSuccess;
}

GDBServerFoundation::GDBStatus MSP430GDBTarget::ReadTargetMemory( ULONGLONG Address, void *pBuffer, size_t *pSizeInBytes )
{
	if (MSP430_Read_Memory((LONG)Address, (char *)pBuffer, *pSizeInBytes) != STATUS_OK)
		REPORT_AND_RETURN("Cannot read device memory", kGDBUnknownError);
	return kGDBSuccess;
}

GDBServerFoundation::GDBStatus MSP430GDBTarget::WriteTargetMemory( ULONGLONG Address, const void *pBuffer, size_t sizeInBytes )
{
	if (Address >= m_DeviceInfo.mainStart && Address <= m_DeviceInfo.mainEnd)
	{
		if (!m_bFLASHErased)
		{
			printf("Error: FLASH needs to be erased before programming.\nPlease either use gdb with XML support, or execute \"mon erase\" command in GDB.");
			return kGDBUnknownError;
		}
	}

	if (MSP430_Write_Memory((LONG)Address, (char *)pBuffer, sizeInBytes) != STATUS_OK)
		REPORT_AND_RETURN("Cannot write device memory", kGDBUnknownError);
	return kGDBSuccess;
}

GDBServerFoundation::GDBStatus MSP430GDBTarget::GetDynamicLibraryList( std::vector<DynamicLibraryRecord> &libraries )
{
	return kGDBNotSupported;
}

GDBServerFoundation::GDBStatus MSP430GDBTarget::GetThreadList( std::vector<ThreadRecord> &threads )
{
	return kGDBNotSupported;
}

GDBServerFoundation::GDBStatus MSP430GDBTarget::SetThreadModeForNextCont( int threadID, DebugThreadMode mode, OUT bool *pNeedRestoreCall, IN OUT INT_PTR *pRestoreCookie )
{
	return kGDBNotSupported;
}

GDBServerFoundation::GDBStatus MSP430GDBTarget::Terminate()
{
	return kGDBNotSupported;
}

GDBServerFoundation::GDBStatus MSP430GDBTarget::CreateBreakpoint( BreakpointType type, ULONGLONG Address, unsigned kind, OUT INT_PTR *pCookie )
{
	if (type == bptHardwareBreakpoint)
	{
		for (size_t i = 0; i < m_UsedBreakpoints.size(); i++)
			if (!m_UsedBreakpoints[i])
			{
				if (MSP430_Breakpoint(i, (LONG)Address) != STATUS_OK)
					REPORT_AND_RETURN("Cannot set a hardware breakpoint", kGDBUnknownError);
				m_UsedBreakpoints[i] = true;
				*pCookie = i;
				return kGDBSuccess;
			}
			return kGDBUnknownError;
	}
	else if (type == bptSoftwareBreakpoint)
	{
		return kGDBNotSupported;

		short breakpointInsn = 0;

		if (MSP430_Read_Memory((LONG)Address, (char *)&breakpointInsn, 2) != STATUS_OK)
			REPORT_AND_RETURN("Cannot read instruction before setting breakpoint", kGDBUnknownError);

		if (Address >= m_DeviceInfo.mainStart && Address <= m_DeviceInfo.mainEnd)
		{
			if ((breakpointInsn != kMSP430_NOP) && (breakpointInsn != kMSP430_BREAKPOINT))
			{
				printf("Cannot set a breakpoint in FLASH memory.\n");
				return kGDBUnknownError;
			}
		}

		m_BreakpointInstructionMap[Address] = breakpointInsn;
		breakpointInsn = kMSP430_BREAKPOINT;

		if (MSP430_Write_Memory((LONG)Address, (char *)&breakpointInsn, 2) != STATUS_OK)
			REPORT_AND_RETURN("Cannot set a software breakpoint", kGDBUnknownError);

		return kGDBSuccess;
	}
	return kGDBNotSupported;
}

GDBServerFoundation::GDBStatus MSP430GDBTarget::RemoveBreakpoint( BreakpointType type, ULONGLONG Address, INT_PTR Cookie )
{
	if (type == bptHardwareBreakpoint)
	{
		size_t i = (size_t)Cookie;
		MSP430_Clear_Breakpoint(i);
		m_UsedBreakpoints[i] = false;

		return kGDBSuccess;
	}
	else if (type == bptSoftwareBreakpoint)
	{
		std::map<ULONGLONG, short>::iterator it = m_BreakpointInstructionMap.find(Address);
		if (it == m_BreakpointInstructionMap.end())
			return kGDBUnknownError;

		short breakpointInsn = it->second;

		if (MSP430_Write_Memory((LONG)Address, (char *)&breakpointInsn, 2) != STATUS_OK)
			REPORT_AND_RETURN("Cannot remoev a software breakpoint", kGDBUnknownError);
		return kGDBSuccess;
	}
	return kGDBNotSupported;
}

GDBServerFoundation::GDBStatus MSP430Proxy::MSP430GDBTarget::EraseFLASH( ULONGLONG addr, size_t length )
{
	if (MSP430_Erase(ERASE_MAIN, (LONG)addr, length) != STATUS_OK)
		REPORT_AND_RETURN("Cannot erase FLASH memory", kGDBUnknownError);
	m_bFLASHErased = true;
	return kGDBSuccess;
}

GDBServerFoundation::GDBStatus MSP430Proxy::MSP430GDBTarget::WriteFLASH( ULONGLONG addr, const void *pBuffer, size_t length )
{
	if (MSP430_Write_Memory((LONG)addr, (char *)pBuffer, length) != STATUS_OK)
		REPORT_AND_RETURN("Cannot program FLASH memory", kGDBUnknownError);
	return kGDBSuccess;
}

GDBServerFoundation::GDBStatus MSP430Proxy::MSP430GDBTarget::CommitFLASHWrite()
{
	return kGDBSuccess;
}

void MSP430Proxy::MSP430GDBTarget::ReportLastMSP430Error( const char *pHint )
{
	if (pHint)
		printf("%s: %s\n", pHint, GetLastMSP430Error());
	else
		printf("%s\n", GetLastMSP430Error());
}
