#pragma once
#include <bzscore/sync.h>
#include "../../GDBServerFoundation/IGDBTarget.h"

namespace MSP430Proxy
{
	using namespace GDBServerFoundation;

	//! Ensures that only one session can be active simultaneously and sends the global Ctrl+C events to the active session.
	class GlobalSessionMonitor
	{
	private:
		BazisLib::Mutex m_Mutex;
		ISyncGDBTarget *pSession;

	public:
		//! Registers the given session as the active session.
		/*!
			\return If another session is already active, this method aborts and returns false. Otherwise it returns true.
		*/
		bool RegisterSession(ISyncGDBTarget *pTarget);

		//! Unregisters a session that has been previously set as active
		void UnregisterSession(ISyncGDBTarget *pTarget);

	public:
		GlobalSessionMonitor();

	private:
		static BOOL CALLBACK CtrlHandler(DWORD dwType);
	};

	extern GlobalSessionMonitor g_SessionMonitor;
}