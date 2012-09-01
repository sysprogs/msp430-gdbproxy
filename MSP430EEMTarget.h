#pragma once

#include "MSP430Target.h"
#include <bzscore/sync.h>

namespace MSP430Proxy
{
	class SoftwareBreakpointManager;

	class MSP430EEMTarget : public MSP430GDBTarget
	{
	private:
		bool m_bEEMInitialized;
		
		BazisLib::Event m_TargetStopped;
		DWORD m_LastStopEvent;
		
		WORD m_SoftwareBreakpointWrapperHandle;
		SoftwareBreakpointManager *m_pBreakpointManager;
		unsigned short m_BreakpointInstruction;
		RUN_MODES_t m_LastResumeMode;
		
		//! If the last resume operation was resuming from a breakpoint, this field contains its address. If not, it contains -1
		LONG m_BreakpointAddrOfLastResumeOp;

	protected:
		virtual bool DoResumeTarget(RUN_MODES_t mode) override;

	public:
		MSP430EEMTarget(unsigned short breakpointInstruction = 0x4343)
			: m_bEEMInitialized(false)
			, m_SoftwareBreakpointWrapperHandle(0)
			, m_pBreakpointManager(NULL)
			, m_BreakpointInstruction(breakpointInstruction)
			, m_LastResumeMode(RUN_TO_BREAKPOINT)
			, m_BreakpointAddrOfLastResumeOp(-1)
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

	public:
		virtual GDBStatus ReadTargetMemory(ULONGLONG Address, void *pBuffer, size_t *pSizeInBytes) override;
		virtual GDBStatus WriteTargetMemory(ULONGLONG Address, const void *pBuffer, size_t sizeInBytes) override;
	};
}