#pragma once

#include "MSP430Target.h"
#include <bzscore/sync.h>

namespace MSP430Proxy
{
	class MSP430EEMTarget : public MSP430GDBTarget
	{
	private:
		bool m_bEEMInitialized;
		
		BazisLib::Event m_TargetStopped;

	protected:
		virtual bool DoResumeTarget(RUN_MODES_t mode) override;

	public:
		MSP430EEMTarget()
			: m_bEEMInitialized(false)
		{
		}


	public:
		virtual bool Initialize(const char *pPortName) override;
		~MSP430EEMTarget();

		virtual bool WaitForJTAGEvent() override;

	private:
		static void sEEMHandler(UINT MsgId, UINT wParam, LONG lParam, LONG clientHandle);
		void EEMNotificationHandler(MSP430_MSG wMsg, WPARAM wParam, LPARAM lParam);

	public:
		virtual GDBStatus CreateBreakpoint(BreakpointType type, ULONGLONG Address, unsigned kind, OUT INT_PTR *pCookie) override;
		virtual GDBStatus RemoveBreakpoint(BreakpointType type, ULONGLONG Address, INT_PTR Cookie) override;

		virtual GDBStatus SendBreakInRequestAsync();
	};
}