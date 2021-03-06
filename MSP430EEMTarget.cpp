#include "stdafx.h"
#include "MSP430EEMTarget.h"

#include "TI/Inc/MSP430_EEM.h"
#include "SoftwareBreakpointManager.h"

#define REPORT_AND_RETURN(msg, result) { ReportLastMSP430Error(msg); return result; }

using namespace GDBServerFoundation;
using namespace MSP430Proxy;

//Breakpoint cookie format:
//32 bits: [type : 4] [reserved : 12] [user_data : 16]
//For RAM breakpoints user_data contains the original insn
//For hardware breakpoints user_data contains the handle returned by EEM API

enum {kBpCookieTypeMask = 0xF0000000,
	kBpCookieTypeHardware = 0x10000000,
	kBpCookieTypeSoftwareFLASH = 0x20000000,
	kBpCookieTypeSoftwareRAM = 0x30000000,
	kBpCookieDataMask = 0x0000FFFF};

#define MAKE_BP_COOKIE(type, data) (((type) & kBpCookieTypeMask) | ((data) & kBpCookieDataMask))

enum MSP430_MSG
{
	WMX_SINGLESTEP = WM_USER + 1,
	WMX_BREKAPOINT,
	WMX_STORAGE,
	WMX_STATE,
	WMX_WARNING,
	WMX_STOPPED,
};

bool MSP430Proxy::MSP430EEMTarget::Initialize(const GlobalSettings &settings)
{
	if (!__super::Initialize(settings))
		return false;

	if (m_pBreakpointManager)
		return false;

	m_BreakpointPolicy = settings.SoftBreakPolicy;
	m_BreakpointInstruction = settings.BreakpointInstruction;

	MESSAGE_ID msgs = {0,};
	msgs.uiMsgIdSingleStep = WMX_SINGLESTEP;
	msgs.uiMsgIdBreakpoint = WMX_BREKAPOINT;
	msgs.uiMsgIdStorage = WMX_STORAGE;
	msgs.uiMsgIdState = WMX_STATE;
	msgs.uiMsgIdWarning = WMX_WARNING;
	msgs.uiMsgIdCPUStopped = WMX_STOPPED;

	C_ASSERT(sizeof(LONG) == sizeof(this));
	if (MSP430_EEM_Init(sEEMHandler, (LONG)this, &msgs) != STATUS_OK)
		REPORT_AND_RETURN("Cannot enter EEM mode", false);

	m_bEEMInitialized = true;

	if (m_BreakpointPolicy != HardwareOnly)
	{
		BpParameter_t bkpt;
		memset(&bkpt, 0, sizeof(bkpt));

		bkpt.bpMode = BP_COMPLEX;
		bkpt.lAddrVal = settings.BreakpointInstruction;
		bkpt.bpType = BP_MDB;
		bkpt.bpAccess = BP_FETCH;
		bkpt.bpAction = BP_BRK;
		bkpt.bpOperat = BP_EQUAL;
		bkpt.bpCondition = BP_NO_COND;
		bkpt.lMask = 0xffff;
		if (MSP430_EEM_SetBreakpoint(&m_SoftwareBreakpointWrapperHandle, &bkpt) != STATUS_OK)
			REPORT_AND_RETURN("Cannot create a MDB breakpoint for handling software breakpoints", false);
		if (m_bVerbose)
			printf("Registered software breakpoint support. Breakpoint instruction: 0x%x; meta-breakpoint handle: %d\n", m_BreakpointInstruction, m_SoftwareBreakpointWrapperHandle);
		m_HardwareBreakpointsUsed++;
	}
	else
	{
		m_SoftwareBreakpointWrapperHandle = -1;
		if (m_bVerbose)
			printf("Warning: Software breakpoints disabled by configuration\n");
	}

	m_pBreakpointManager = new SoftwareBreakpointManager(m_DeviceInfo.mainStart, m_DeviceInfo.mainEnd, settings.BreakpointInstruction, settings.InstantBreakpointCleanup, settings.Verbose);

	return true;
}

MSP430Proxy::MSP430EEMTarget::~MSP430EEMTarget()
{
	delete m_pBreakpointManager;

	if (m_SoftwareBreakpointWrapperHandle != -1)
	{
		BpParameter_t bkpt;
		memset(&bkpt, 0, sizeof(bkpt));
		bkpt.bpMode = BP_CLEAR;

		STATUS_T status = MSP430_EEM_SetBreakpoint(&m_SoftwareBreakpointWrapperHandle, &bkpt);
		if (m_bVerbose)
			printf("Unregistering software breakpoint support => %d, bpHandle = %d\n", status, m_SoftwareBreakpointWrapperHandle);
	}
}

void MSP430Proxy::MSP430EEMTarget::EEMNotificationHandler( MSP430_MSG wMsg, WPARAM wParam, LPARAM lParam )
{
	if (m_bVerbose)
		printf("EEM notification: 0x%x\n", wMsg);

	switch(wMsg)
	{
	case WMX_BREKAPOINT:
	case WMX_SINGLESTEP:
	case WMX_STOPPED:
		if (m_bVerbose)
			printf("Target stop detected\n");
		m_LastStopEvent = wMsg;
		m_TargetStopped.Set();
		break;
	}
}

void MSP430Proxy::MSP430EEMTarget::sEEMHandler( UINT MsgId, UINT wParam, LONG lParam, LONG clientHandle )
{
	C_ASSERT(sizeof(clientHandle) == sizeof(MSP430EEMTarget *));
	((MSP430EEMTarget *)clientHandle)->EEMNotificationHandler((MSP430_MSG)MsgId, wParam, lParam);
}

void MSP430Proxy::MSP430EEMTarget::DoSendBreakInRequest()
{
	LONG state = 0;
	LONG cpuCycles = 0;
	STATUS_T status = MSP430_State(&state, TRUE, &cpuCycles);
	if (m_bVerbose)
		printf("Break-in request: MSP430_State() => %d, state = %d, CPU cycles = %d\n", status, state, cpuCycles);
	if (status != STATUS_OK)
		ReportLastMSP430Error("Cannot stop device");
}

bool MSP430Proxy::MSP430EEMTarget::WaitForJTAGEvent()
{
	for (;;)
	{
		if (m_bVerbose)
			printf("Waiting for the target to stop (EEM event will be generated)...\n");
		
		for (;;)
		{
			HANDLE events[] = {m_TargetStopped.GetHandle(), m_BreakInSemaphore.GetHandle()};
			DWORD waitResult = WaitForMultipleObjects(2, events, FALSE, INFINITE);
			if (waitResult == WAIT_OBJECT_0)
				break;	//Target stopped
			else if (waitResult == (WAIT_OBJECT_0 + 1))
			{
				if (m_bVerbose)
					printf("Handling break-in request from main worker thread...\n");
				DoSendBreakInRequest();

				//We have requested a break-in. Let's give the target some time to react.

				for (;;)
				{
					if (WaitForSingleObject(m_TargetStopped.GetHandle(), 100) == WAIT_OBJECT_0)
					{
						if (m_bVerbose)
							printf("Break-in handled normally. Target stop reported.\n");
						break;	//Done
					}

					LONG state = 0, cpuCycles = 0;
					STATUS_T status = MSP430_State(&state, FALSE, &cpuCycles);
					if (m_bVerbose)
						printf("Post-break-in check: MSP430_State() => %d, state = %d, CPU cycles = %d\n", status, state, cpuCycles);
					
					if (state == STOPPED)
					{
						if (m_bVerbose)
							printf("Stop event not reported, but the CPU state is STOPPED. Exiting wait loop...\n");
						break;
					}

					if (m_bVerbose)
						printf("CPU state is not STOPPED. Continuing...\n");
				}

				break;
			}
			else
			{
				if (m_bVerbose)
					printf("Unexpected wait result: 0x%x\n", waitResult);
				break;
			}
		}

		LONG regPC = 0;

		if (MSP430_Read_Register(&regPC, PC) != STATUS_OK)
			REPORT_AND_RETURN("Cannot read PC register", false);

		if (m_bVerbose)
			printf("Target stopped, PC = 0x%x\n", regPC);

		SoftwareBreakpointManager::BreakpointState bpState = m_pBreakpointManager->GetBreakpointState(regPC - 2);
		if (m_RAMBreakpoints.IsBreakpointPresent((USHORT)regPC - 2))
			bpState = SoftwareBreakpointManager::BreakpointActive;

		switch(bpState)
		{
		case SoftwareBreakpointManager::BreakpointActive:
		case SoftwareBreakpointManager::BreakpointInactive:
			if (m_bVerbose)
				printf("Found a software breakpoint at PC = 0x%X\n", regPC - 2);

			regPC -= 2;

			if (regPC == m_BreakpointAddrOfLastResumeOp)
			{
				//We have just continued from the breakpoint event by patching the original instruction into the instruction fetcher.
				//We don't want to stop indefinitely here.
				if (m_LastResumeMode != SINGLE_STEP)
				{
					if (m_bVerbose)
						printf("Resuming execution after a software breakpoint\n");
					if (!DoResumeTarget(m_LastResumeMode))
						return false;
					continue;
				}
				return true;
			}

			if (MSP430_Write_Register(&regPC, PC) != STATUS_OK)
				REPORT_AND_RETURN("Cannot read PC register", false);

			if (bpState == SoftwareBreakpointManager::BreakpointInactive && m_LastResumeMode != SINGLE_STEP && !m_BreakInSemaphore.TryWait())
			{
				if (m_bVerbose)
					printf("Breakpoint at PC = 0x%X is inactive. Skipping...\n", regPC);

				//Skip the breakpoint
				if (!DoResumeTarget(m_LastResumeMode))
					return false;
				continue;
			}

			return true;
		case SoftwareBreakpointManager::NoBreakpoint:
		default:
			return true;	//The stop is not related to a software breakpoint
		}
	}
}

GDBServerFoundation::GDBStatus MSP430Proxy::MSP430EEMTarget::CreateBreakpoint( BreakpointType type, ULONGLONG Address, unsigned kind, OUT INT_PTR *pCookie )
{
	switch(type)
	{
	case bptSoftwareBreakpoint:
		switch(m_BreakpointPolicy)
		{
		case HardwareOnly:
			return DoCreateCodeBreakpoint(true, Address, pCookie);
		case HardwareThenSoftware:
			return DoCreateCodeBreakpoint((m_HardwareBreakpointsUsed < m_DeviceInfo.nBreakpoints), Address, pCookie);
		case SoftwareOnly:
		default:
			return DoCreateCodeBreakpoint(false, Address, pCookie);
		}
	case bptHardwareBreakpoint:
		if (m_BreakpointPolicy == HardwareThenSoftware && m_bFLASHCommandsUsed)
			return DoCreateCodeBreakpoint((m_HardwareBreakpointsUsed < m_DeviceInfo.nBreakpoints), Address, pCookie);
		else
			return DoCreateCodeBreakpoint(true, Address, pCookie);
	case bptAccessWatchpoint:
	case bptWriteWatchpoint:
	case bptReadWatchpoint:
		break;
	default:
		return kGDBNotSupported;
	}

	BpParameter_t bkpt;
	memset(&bkpt, 0, sizeof(bkpt));
	bkpt.bpMode = BP_COMPLEX;
	bkpt.lAddrVal = (ULONG)Address;
	bkpt.bpType = BP_MAB;
	switch(type)
	{
	case bptAccessWatchpoint:
		bkpt.bpAccess = BP_NO_FETCH;
		break;
	case bptReadWatchpoint:
		bkpt.bpAccess = BP_READ;
		break;
	case bptWriteWatchpoint:
		bkpt.bpAccess = BP_WRITE;
		break;
	}
	bkpt.bpAction = BP_BRK;
	bkpt.bpOperat = BP_EQUAL;
	bkpt.bpCondition = BP_NO_COND;
	bkpt.lMask = 0xffff;

	WORD bpHandle = 0;
	if (MSP430_EEM_SetBreakpoint(&bpHandle, &bkpt) != STATUS_OK)
		REPORT_AND_RETURN("Cannot set an EEM breakpoint", kGDBUnknownError);

	*pCookie = MAKE_BP_COOKIE(kBpCookieTypeHardware, bpHandle);

	return kGDBSuccess;
}

GDBServerFoundation::GDBStatus MSP430Proxy::MSP430EEMTarget::RemoveBreakpoint( BreakpointType type, ULONGLONG Address, INT_PTR Cookie )
{
	switch(type)
	{
	case bptSoftwareBreakpoint:
		return DoRemoveCodeBreakpoint(false, Address, Cookie);
	case bptHardwareBreakpoint:
	case bptReadWatchpoint:
	case bptWriteWatchpoint:
	case bptAccessWatchpoint:
		return DoRemoveCodeBreakpoint(true, Address, Cookie);
	default:
		return kGDBNotSupported;
	}

	return kGDBSuccess;
}

bool MSP430Proxy::MSP430EEMTarget::DoResumeTarget( RUN_MODES_t mode )
{
	unsigned short originalInsn;
	LONG regPC = 0;
	if (MSP430_Read_Register(&regPC, PC) != STATUS_OK)
		REPORT_AND_RETURN("Cannot read PC register", false);
	if (m_pBreakpointManager->GetOriginalInstruction(regPC, &originalInsn))
	{
		if (MSP430_Configure(SET_MDB_BEFORE_RUN, originalInsn) != STATUS_OK)
			REPORT_AND_RETURN("Cannot resume from a software breakpoint", false);
		m_BreakpointAddrOfLastResumeOp = regPC;
	}
	else
		m_BreakpointAddrOfLastResumeOp = -1;

	m_LastResumeMode = mode;
	m_TargetStopped.Reset();
	if (!m_pBreakpointManager->CommitBreakpoints())
	{
		printf("ERROR: Cannot commit software breakpoints\n");
		return false;
	}
	
	if (!__super::DoResumeTarget(mode))
		return false;

	return true;
}

GDBServerFoundation::GDBStatus MSP430Proxy::MSP430EEMTarget::SendBreakInRequestAsync()
{
	if (m_bVerbose)
		printf("Received an asynchronous break-in request.\n");
	m_BreakInSemaphore.Signal();

	return kGDBSuccess;
}

GDBServerFoundation::GDBStatus MSP430Proxy::MSP430EEMTarget::ReadTargetMemory( ULONGLONG Address, void *pBuffer, size_t *pSizeInBytes )
{
	GDBStatus status = __super::ReadTargetMemory(Address, pBuffer, pSizeInBytes);
	if (status != kGDBSuccess)
		return status;

	m_pBreakpointManager->HideOrRestoreBreakpointsInMemorySnapshot((unsigned)Address, pBuffer, *pSizeInBytes, true);

	return kGDBSuccess;
}

GDBServerFoundation::GDBStatus MSP430Proxy::MSP430EEMTarget::WriteTargetMemory( ULONGLONG Address, const void *pBuffer, size_t sizeInBytes )
{
	GDBStatus status = __super::WriteTargetMemory(Address, pBuffer, sizeInBytes);
	if (status != kGDBSuccess)
		return status;

//	m_pBreakpointManager->HideOrRestoreBreakpointsInMemorySnapshot((unsigned)Address, pBuffer, *pSizeInBytes, false);

	return kGDBSuccess;
}

GDBServerFoundation::GDBStatus MSP430Proxy::MSP430EEMTarget::DoCreateCodeBreakpoint( bool hardware, ULONGLONG Address, INT_PTR *pCookie )
{
	if (hardware)
	{
		BpParameter_t bkpt;
		memset(&bkpt, 0, sizeof(bkpt));
		bkpt.bpMode = BP_CODE;
		bkpt.lAddrVal = (LONG)Address;
		bkpt.bpCondition = BP_NO_COND;
		bkpt.bpAction = BP_BRK;

		WORD bpHandle = 0;
		if (MSP430_EEM_SetBreakpoint(&bpHandle, &bkpt) != STATUS_OK)
			REPORT_AND_RETURN("Cannot set an EEM breakpoint", kGDBUnknownError);

		if (m_bVerbose)
			printf("Created a hardware breakpoint #%d at 0x%x\n", bpHandle, (ULONG)Address);

		m_HardwareBreakpointsUsed++;

		C_ASSERT(sizeof(bpHandle) == 2);
		*pCookie = MAKE_BP_COOKIE(kBpCookieTypeHardware, bpHandle);

		return kGDBSuccess;
	}
	else
	{
		if (!IsFLASHAddress(Address))
		{
			ULONG addr = (ULONG)(Address & ~1);

			if (m_RAMBreakpoints.IsBreakpointPresent((USHORT)addr))
			{
				printf("Cannot set a breakpoint at 0x%04x. Breakpoint already exists.\n", (unsigned)Address);
				return kGDBUnknownError;
			}

			unsigned short insn;
			if (MSP430_Read_Memory(addr, (char *)&insn, 2) != STATUS_OK)
				REPORT_AND_RETURN("Cannot set a software breakpoint in RAM", kGDBUnknownError);

			if (m_bVerbose)
				printf("Setting a RAM breakpoint at 0x%x. Previous instruction is 0x%x\n", addr, insn);
			
			*pCookie = MAKE_BP_COOKIE(kBpCookieTypeSoftwareRAM, insn);

			insn = m_BreakpointInstruction;
			if (MSP430_Write_Memory(addr, (char *)&insn, 2) != STATUS_OK)
				REPORT_AND_RETURN("Cannot set a software breakpoint in RAM", kGDBUnknownError);

			m_RAMBreakpoints.InsertBreakpoint((USHORT)addr);
		}
		else
		{
			if (!m_pBreakpointManager->SetBreakpoint((unsigned)Address))
				return kGDBUnknownError;
			*pCookie = MAKE_BP_COOKIE(kBpCookieTypeSoftwareFLASH, 0);
		}
		return kGDBSuccess;
	}
}

GDBServerFoundation::GDBStatus MSP430Proxy::MSP430EEMTarget::DoRemoveCodeBreakpoint( bool hardware, ULONGLONG Address, INT_PTR Cookie )
{
	if ((Cookie & kBpCookieTypeMask) == kBpCookieTypeHardware)
	{
		BpParameter_t bkpt;
		memset(&bkpt, 0, sizeof(bkpt));
		bkpt.bpMode = BP_CLEAR;

		WORD bpHandle = (WORD)(Cookie & kBpCookieDataMask);

		if (MSP430_EEM_SetBreakpoint(&bpHandle, &bkpt) != STATUS_OK)
			REPORT_AND_RETURN("Cannot remove an EEM breakpoint", kGDBUnknownError);

		if (m_bVerbose)
			printf("Removed a hardware breakpoint #%d at 0x%x\n", (WORD)(Cookie & kBpCookieDataMask), (ULONG)Address);

		m_HardwareBreakpointsUsed--;
		return kGDBSuccess;
	}
	else
	{
		if (!IsFLASHAddress(Address))
		{
			ASSERT((Cookie & kBpCookieTypeMask) == kBpCookieTypeSoftwareRAM);
			unsigned short originalINSN = (unsigned short)(Cookie & kBpCookieDataMask);

			if (m_bVerbose)
				printf("Deleting SRAM breakpoint at 0x%x. Restoring original instruction of 0x%x.\n", (ULONG)Address, originalINSN);

			if (MSP430_Write_Memory(Address & ~1, (char *)&originalINSN, 2) != STATUS_OK)
				REPORT_AND_RETURN("Cannot remove a software breakpoint from RAM", kGDBUnknownError);

			m_RAMBreakpoints.RemoveBreakpoint((USHORT)(Address & ~1));
		}
		else
		{
			ASSERT((Cookie & kBpCookieTypeMask) == kBpCookieTypeSoftwareFLASH);
			if (!m_pBreakpointManager->RemoveBreakpoint((unsigned)Address))
				return kGDBUnknownError;
		}

		return kGDBSuccess;
	}
}
