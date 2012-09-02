#pragma once

#include "MSP430Target.h"
#include <bzscore/sync.h>
#include <set>

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
		RUN_MODES_t m_LastResumeMode;

		//! If the last resume operation was resuming from a breakpoint, this field contains its address. If not, it contains -1
		LONG m_BreakpointAddrOfLastResumeOp;

	private:
		class RAMBreakpointDatabase
		{
		private:
			unsigned char Flags[65536 / 8];

		public:
			RAMBreakpointDatabase()
			{
				memset(Flags, 0, sizeof(Flags));
			}

			void InsertBreakpoint(USHORT addr)
			{
				Flags[addr >> 3] |= (1 << (addr & 7));
			}

			void RemoveBreakpoint(USHORT addr)
			{
				Flags[addr >> 3] &= ~(1 << (addr & 7));
			}

			bool IsBreakpointPresent(USHORT addr)
			{
				return (Flags[addr >> 3] & (1 << (addr & 7))) != 0;
			}
		};

		RAMBreakpointDatabase m_RAMBreakpoints;
		unsigned short m_BreakpointInstruction;

	protected:
		virtual bool DoResumeTarget(RUN_MODES_t mode) override;

		bool IsFLASHAddress(ULONGLONG addr)
		{
			return addr >= m_DeviceInfo.mainStart && addr <= m_DeviceInfo.mainEnd;
		}

	public:
		MSP430EEMTarget()
			: m_bEEMInitialized(false)
			, m_SoftwareBreakpointWrapperHandle(0)
			, m_pBreakpointManager(NULL)
			, m_LastResumeMode(RUN_TO_BREAKPOINT)
			, m_BreakpointAddrOfLastResumeOp(-1)
			, m_BreakpointInstruction(0)	//Will be updated in Initialize()
		{
		}


	public:
		virtual bool Initialize(const GlobalSettings &settings) override;
		~MSP430EEMTarget();

		virtual bool WaitForJTAGEvent() override;

	private:
		static void sEEMHandler(UINT MsgId, UINT wParam, LONG lParam, LONG clientHandle);
		void EEMNotificationHandler(MSP430_MSG wMsg, WPARAM wParam, LPARAM lParam);

		GDBStatus DoCreateCodeBreakpoint(bool hardware, ULONGLONG Address, INT_PTR *pCookie);
		GDBStatus DoRemoveCodeBreakpoint(bool hardware, ULONGLONG Address, INT_PTR Cookie);

	public:
		virtual GDBStatus CreateBreakpoint(BreakpointType type, ULONGLONG Address, unsigned kind, OUT INT_PTR *pCookie) override;
		virtual GDBStatus RemoveBreakpoint(BreakpointType type, ULONGLONG Address, INT_PTR Cookie) override;

		virtual GDBStatus SendBreakInRequestAsync();

	public:
		virtual GDBStatus ReadTargetMemory(ULONGLONG Address, void *pBuffer, size_t *pSizeInBytes) override;
		virtual GDBStatus WriteTargetMemory(ULONGLONG Address, const void *pBuffer, size_t sizeInBytes) override;
	};
}