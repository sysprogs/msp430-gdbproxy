#include "stdafx.h"
#include "GlobalSessionMonitor.h"

using namespace BazisLib;
using namespace MSP430Proxy;

GlobalSessionMonitor MSP430Proxy::g_SessionMonitor;

bool MSP430Proxy::GlobalSessionMonitor::RegisterSession( ISyncGDBTarget *pTarget )
{
	MutexLocker lck(g_SessionMonitor.m_Mutex);
	if (pSession)
		return false;
	pSession = pTarget;
	return true;
}

void MSP430Proxy::GlobalSessionMonitor::UnregisterSession( ISyncGDBTarget *pTarget )
{
	MutexLocker lck(g_SessionMonitor.m_Mutex);
	ASSERT(pSession == pTarget);
	pSession = NULL;
}

MSP430Proxy::GlobalSessionMonitor::GlobalSessionMonitor()
	: pSession(NULL)
{
	SetConsoleCtrlHandler(CtrlHandler, TRUE);
}

BOOL CALLBACK MSP430Proxy::GlobalSessionMonitor::CtrlHandler( DWORD dwType )
{
	if (dwType == CTRL_C_EVENT)
	{
		MutexLocker lck(g_SessionMonitor.m_Mutex);
		if (g_SessionMonitor.pSession)
			g_SessionMonitor.pSession->SendBreakInRequestAsync();
		return TRUE;
	}
	return FALSE;
}
