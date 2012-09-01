#include "stdafx.h"
#include "MSP430EEMTarget.h"

#include <MSP430_EEM.h>

#define REPORT_AND_RETURN(msg, result) { ReportLastMSP430Error(msg); return result; }

using namespace GDBServerFoundation;
using namespace MSP430Proxy;

enum MSP430_MSG
{
	WMX_SINGLESTEP = WM_USER + 1,
	WMX_BREKAPOINT,
	WMX_STORAGE,
	WMX_STATE,
	WMX_WARNING,
	WMX_STOPPED,
};

bool MSP430Proxy::MSP430EEMTarget::Initialize( const char *pPortName )
{
	if (!__super::Initialize(pPortName))
		return false;

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
	return true;
}

MSP430Proxy::MSP430EEMTarget::~MSP430EEMTarget()
{
	if (m_bEEMInitialized)
		MSP430_EEM_Close();
}

void MSP430Proxy::MSP430EEMTarget::EEMNotificationHandler( MSP430_MSG wMsg, WPARAM wParam, LPARAM lParam )
{
	switch(wMsg)
	{
	case WMX_BREKAPOINT:
	case WMX_SINGLESTEP:
	case WMX_STOPPED:
		m_TargetStopped.Set();
		break;
	}
}

void MSP430Proxy::MSP430EEMTarget::sEEMHandler( UINT MsgId, UINT wParam, LONG lParam, LONG clientHandle )
{
	C_ASSERT(sizeof(clientHandle) == sizeof(MSP430EEMTarget *));
	((MSP430EEMTarget *)clientHandle)->EEMNotificationHandler((MSP430_MSG)MsgId, wParam, lParam);
}

bool MSP430Proxy::MSP430EEMTarget::WaitForJTAGEvent()
{
	m_TargetStopped.Wait();
	return true;
}

GDBServerFoundation::GDBStatus MSP430Proxy::MSP430EEMTarget::CreateBreakpoint( BreakpointType type, ULONGLONG Address, unsigned kind, OUT INT_PTR *pCookie )
{
	BpParameter_t bkpt;
	memset(&bkpt, 0, sizeof(bkpt));
	switch(type)
	{
	case bptHardwareBreakpoint:
		bkpt.bpMode = BP_CODE;
		bkpt.lAddrVal = (LONG)Address;
		bkpt.bpCondition = BP_NO_COND;
		bkpt.bpAction = BP_BRK;
		break;
	default:
		return kGDBNotSupported;
	}

	WORD bpHandle = 0;
	if (MSP430_EEM_SetBreakpoint(&bpHandle, &bkpt) != STATUS_OK)
		REPORT_AND_RETURN("Cannot set an EEM breakpoint", kGDBUnknownError);

	*pCookie = bpHandle;

	return kGDBSuccess;
}

GDBServerFoundation::GDBStatus MSP430Proxy::MSP430EEMTarget::RemoveBreakpoint( BreakpointType type, ULONGLONG Address, INT_PTR Cookie )
{
	BpParameter_t bkpt;
	memset(&bkpt, 0, sizeof(bkpt));
	bkpt.bpMode = BP_CLEAR;

	WORD bpHandle = (WORD)Cookie;

	if (MSP430_EEM_SetBreakpoint(&bpHandle, &bkpt) != STATUS_OK)
		REPORT_AND_RETURN("Cannot remove an EEM breakpoint", kGDBUnknownError);

	return kGDBSuccess;
}

bool MSP430Proxy::MSP430EEMTarget::DoResumeTarget( RUN_MODES_t mode )
{
	m_TargetStopped.Reset();
	return __super::DoResumeTarget(mode);
}

GDBServerFoundation::GDBStatus MSP430Proxy::MSP430EEMTarget::SendBreakInRequestAsync()
{
	LONG state;
	if (MSP430_State(&state, TRUE, NULL) != STATUS_OK)
		REPORT_AND_RETURN("Cannot stop device", kGDBNotSupported);
	return kGDBSuccess;
}
