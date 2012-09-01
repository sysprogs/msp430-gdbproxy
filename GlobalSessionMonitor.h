#pragma once
#include <bzscore/sync.h>
#include "../../GDBServerFoundation/IGDBTarget.h"

namespace MSP430Proxy
{
	using namespace GDBServerFoundation;

	class GlobalSessionMonitor
	{
	private:
		BazisLib::Mutex m_Mutex;
		ISyncGDBTarget *pSession;

	public:
		bool RegisterSession(ISyncGDBTarget *pTarget);
		void UnregisterSession(ISyncGDBTarget *pTarget);

	public:
		GlobalSessionMonitor();

	private:
		static BOOL CALLBACK CtrlHandler(DWORD dwType);
	};

	extern GlobalSessionMonitor g_SessionMonitor;
}