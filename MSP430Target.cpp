#include "stdafx.h"
#include "MSP430Target.h"
#include "MSP430Util.h"

using namespace GDBServerFoundation;
using namespace MSP430Proxy;

#define REPORT_AND_RETURN(msg, result) { ReportLastMSP430Error(msg); return result; }
#define MAIN_SEGMENT_SIZE 512

bool MSP430Proxy::MSP430GDBTarget::Initialize(const GlobalSettings &settings)
{
	if (m_bClosePending)
		return m_bValid;

	m_bVerbose = settings.Verbose;

	LONG version = 0;
	if (MSP430_Initialize((char *)settings.PortName, &version) != STATUS_OK)
		REPORT_AND_RETURN("Cannot initialize MSP430.DLL", false);
	m_bClosePending = true;

	switch(settings.Interface)
	{
	case Jtag:
		if (m_bVerbose)
			printf("Selecting JTAG interface...\n");
		if (MSP430_Configure(INTERFACE_MODE, JTAG_IF) != STATUS_OK)
			REPORT_AND_RETURN("Cannot select JTAG interface", false);
		break;
	case SpyBiWare:
		if (m_bVerbose)
			printf("Selecting Spy-bi-Wire interface...\n");
		if (MSP430_Configure(INTERFACE_MODE, SPYBIWIRE_IF) != STATUS_OK)
			REPORT_AND_RETURN("Cannot select Spy-bi-Wire interface", false);
		break;
	case JtagOverSpyBiWare:
		if (m_bVerbose)
			printf("Selecting JTAG-over-Spy-bi-Wire interface...\n");
		if (MSP430_Configure(INTERFACE_MODE, SPYBIWIREJTAG_IF) != STATUS_OK)
			REPORT_AND_RETURN("Cannot select JTAG-over-Spy-bi-Wire interface", false);
		break;
	case AutomaticInterface:
		if (m_bVerbose)
			printf("Selecting auto interface...\n");
		if (MSP430_Configure(INTERFACE_MODE, AUTOMATIC_IF) != STATUS_OK)
			REPORT_AND_RETURN("Cannot select auto interface", false);
		break;
	}

	switch(settings.InterfaceSpeed)
	{
	case Slow:
		if (m_bVerbose)
			printf("Setting interface speed to slow...\n");
		if (MSP430_Configure(SET_INTERFACE_SPEED, SLOW) != STATUS_OK)
			REPORT_AND_RETURN("Cannot set interface speed", false);
		break;
	case Medium:
		if (m_bVerbose)
			printf("Setting interface speed to medium...\n");
		if (MSP430_Configure(SET_INTERFACE_SPEED, MEDIUM) != STATUS_OK)
			REPORT_AND_RETURN("Cannot set interface speed", false);
		break;
	case Fast:
		if (m_bVerbose)
			printf("Setting interface speed to fast...\n");
		if (MSP430_Configure(SET_INTERFACE_SPEED, FAST) != STATUS_OK)
			REPORT_AND_RETURN("Cannot set interface speed", false);
		break;
	}

	if (MSP430_VCC(settings.Voltage) != STATUS_OK)
		REPORT_AND_RETURN("Cannot enable Vcc", false);

	if (MSP430_OpenDevice("DEVICE_UNKNOWN", "", 0, 0, DEVICE_UNKNOWN) != STATUS_OK)
		REPORT_AND_RETURN("Cannot identify the MSP430 device", false);

	if (MSP430_GetFoundDevice((char *)&m_DeviceInfo, sizeof(m_DeviceInfo)) != STATUS_OK)
		REPORT_AND_RETURN("Cannot identify the MSP430 device", false);

	if (MSP430_Reset(ALL_RESETS, FALSE, FALSE) != STATUS_OK)
		REPORT_AND_RETURN("Cannot reset the MSP430 device", false);

	if (settings.AutoErase)
	{
		printf("Erasing FLASH...\n");
		if (MSP430_Erase(ERASE_MAIN, m_DeviceInfo.mainStart, m_DeviceInfo.mainEnd - m_DeviceInfo.mainStart) != STATUS_OK)
			printf("Warning: cannot erase FLASH: %s\n", GetLastMSP430Error());
		else
			m_bFLASHErased = true;
	}

	m_DeviceInfo.string[__countof(m_DeviceInfo.string) - 1] = 0;
	printf("Found a device: %s\n", m_DeviceInfo.string);
	printf("Number of hardware breakpoints: %d\n", m_DeviceInfo.nBreakpoints);
	printf("%d bytes of FLASH memory (0x%04x-0x%04x)\n", m_DeviceInfo.mainEnd - m_DeviceInfo.mainStart + 1, m_DeviceInfo.mainStart, m_DeviceInfo.mainEnd);
	printf("%d bytes of RAM (0x%04x-0x%04x)\n", m_DeviceInfo.ramEnd - m_DeviceInfo.ramStart + 1, m_DeviceInfo.ramStart, m_DeviceInfo.ramEnd);
	if (m_DeviceInfo.ram2End || m_DeviceInfo.ram2Start)
		printf("%d bytes of RAM2 (0x%04x-0x%04x)\n", m_DeviceInfo.ram2End - m_DeviceInfo.ram2Start + 1, m_DeviceInfo.ram2Start, m_DeviceInfo.ram2End);
	printf("%d bytes of INFO memory (0x%04x-0x%04x)\n", m_DeviceInfo.infoEnd - m_DeviceInfo.infoStart + 1, m_DeviceInfo.infoStart, m_DeviceInfo.infoEnd);

	m_UsedBreakpoints.resize(m_DeviceInfo.nBreakpoints);
	m_bValid = true;
	return true;
}

#include "GlobalSessionMonitor.h"

MSP430Proxy::MSP430GDBTarget::~MSP430GDBTarget()
{
	if (m_bClosePending)
	{
		printf("GDB Disconnected. Releasing MSP430 interface.\n");
		MSP430_Close(FALSE);
	}

	g_SessionMonitor.UnregisterSession(this);
}

bool MSP430Proxy::MSP430GDBTarget::WaitForJTAGEvent()
{
	LONGLONG lastReportTime = {0,};
	for (;;)
	{
		LONG state = 0;
		if (MSP430_State(&state, m_BreakInPending, NULL) != STATUS_OK)
			REPORT_AND_RETURN("Cannot query device state", false);

		if (m_bVerbose)
		{
			LONGLONG currentTime = {0,};
			GetSystemTimeAsFileTime(reinterpret_cast<FILETIME *>(&currentTime));
			if ((currentTime - lastReportTime) > (3000000))	//300ms
			{
				printf("MSP430_State() => %d, break-in %s\n", state, m_BreakInPending ? "requested" : "not requested");
			}
		}

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
		regions.push_back(EmbeddedMemoryRegion(mtFLASH, m_DeviceInfo.mainStart, m_DeviceInfo.mainEnd - m_DeviceInfo.mainStart + 1, MAIN_SEGMENT_SIZE));
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
	if (!DoResumeTarget(RUN_TO_BREAKPOINT))
		return kGDBUnknownError;

	if (!WaitForJTAGEvent())
		return kGDBUnknownError;
	return kGDBSuccess;
}

GDBServerFoundation::GDBStatus MSP430GDBTarget::Step( int threadID )
{
	if (!DoResumeTarget(SINGLE_STEP))
		return kGDBUnknownError;

	if (!WaitForJTAGEvent())
		return kGDBUnknownError;

	return kGDBSuccess;
}

GDBServerFoundation::GDBStatus MSP430GDBTarget::SendBreakInRequestAsync()
{
	m_BreakInPending = true;
	return kGDBSuccess;
}

GDBServerFoundation::GDBStatus MSP430GDBTarget::ReadFrameRelatedRegisters( int threadID, RegisterSetContainer &registers )
{
	LONG rawRegs[16] = {0,};
	if (MSP430_Read_Registers(rawRegs, MASKREG(PC) | MASKREG(SP)) != STATUS_OK)
		REPORT_AND_RETURN("Cannot read frame-related registers", kGDBUnknownError);

	registers[PC] = RegisterValue(rawRegs[PC], 2);
	registers[SP] = RegisterValue(rawRegs[SP], 2);

	return kGDBSuccess;
}

GDBServerFoundation::GDBStatus MSP430GDBTarget::ReadTargetRegisters( int threadID, RegisterSetContainer &registers )
{
	LONG rawRegs[16] = {0,};
	if (MSP430_Read_Registers(rawRegs, ALL_REGS) != STATUS_OK)
		REPORT_AND_RETURN("Cannot read device registers", kGDBUnknownError);

	for (size_t i = 0; i < __countof(rawRegs); i++)
		registers[i] = RegisterValue(rawRegs[i], 16);
	
	return kGDBSuccess;
}

GDBServerFoundation::GDBStatus MSP430GDBTarget::WriteTargetRegisters( int threadID, const RegisterSetContainer &registers )
{
	LONG rawRegs[16] = {0,};
	int mask = 0;

	for (size_t i = 0; i < 16; i++)
		if (registers[i].Valid)
		{
			mask |= MASKREG(i);
			rawRegs[i] = registers[i].ToUInt16();
		}

	if (MSP430_Write_Registers(rawRegs, mask) != STATUS_OK)
		REPORT_AND_RETURN("Cannot write device registers", kGDBUnknownError);

	return kGDBSuccess;
}

GDBServerFoundation::GDBStatus MSP430GDBTarget::ReadTargetMemory( ULONGLONG Address, void *pBuffer, size_t *pSizeInBytes )
{
	size_t readSize = *pSizeInBytes;
	if (MSP430_Read_Memory((LONG)Address, (char *)pBuffer, *pSizeInBytes) != STATUS_OK)
	{
		char szMsg[256];
		_snprintf(szMsg, _TRUNCATE, "Cannot read %d memory bytes at 0x%I64X", readSize, Address);
		REPORT_AND_RETURN(szMsg, kGDBUnknownError);
	}

	if (m_bVerbose)
		printf("MSP430_Read_Memory(0x%x, %d) => success\n", (LONG)Address, *pSizeInBytes);

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
	printf("Warning! Breakpoints are no longer supported in non-EEM mode.\n");
	return kGDBUnknownError;
}

GDBServerFoundation::GDBStatus MSP430GDBTarget::RemoveBreakpoint( BreakpointType type, ULONGLONG Address, INT_PTR Cookie )
{
	printf("Warning! Breakpoints are no longer supported in non-EEM mode.\n");
	return kGDBUnknownError;
}

GDBServerFoundation::GDBStatus MSP430Proxy::MSP430GDBTarget::EraseFLASH( ULONGLONG addr, size_t length )
{
	m_bFLASHCommandsUsed = true;
	if (MSP430_Erase(ERASE_MAIN, (LONG)addr, length) != STATUS_OK)
		REPORT_AND_RETURN("Cannot erase FLASH memory", kGDBUnknownError);
	m_bFLASHErased = true;
	return kGDBSuccess;
}

GDBServerFoundation::GDBStatus MSP430Proxy::MSP430GDBTarget::WriteFLASH( ULONGLONG addr, const void *pBuffer, size_t length )
{
	m_bFLASHCommandsUsed = true;
	if (MSP430_Write_Memory((LONG)addr, (char *)pBuffer, length) != STATUS_OK)
		REPORT_AND_RETURN("Cannot program FLASH memory", kGDBUnknownError);
	return kGDBSuccess;
}

GDBServerFoundation::GDBStatus MSP430Proxy::MSP430GDBTarget::CommitFLASHWrite()
{
	m_bFLASHCommandsUsed = true;
	return kGDBSuccess;
}

void MSP430Proxy::MSP430GDBTarget::ReportLastMSP430Error( const char *pHint )
{
	if (pHint)
		printf("%s: %s\n", pHint, GetLastMSP430Error());
	else
		printf("%s\n", GetLastMSP430Error());
}

bool MSP430Proxy::MSP430GDBTarget::DoResumeTarget( RUN_MODES_t mode )
{
	STATUS_T status = MSP430_Run(mode, FALSE);
	if (m_bVerbose)
		printf("MSP430_Run(%d) => %d\n", mode, status);

	if (status != STATUS_OK)
		REPORT_AND_RETURN("Cannot resume device", false);

	return true;
}

