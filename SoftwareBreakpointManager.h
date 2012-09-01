#pragma once
#include <vector>
#include <list>

namespace MSP430Proxy
{
	class SoftwareBreakpointManager
	{
	public:
		enum BreakpointState
		{
			NoBreakpoint = 0,	//Breakpoint is not set at this address
			BreakpointPending,	//Breakpoint will be created in the next call to CommitBreakpoints()
			BreakpointInactive,	//Breakpoint is present in memory, but should be ignored and removed next time the segment is overwritten
			BreakpointActive,	//Breakpoint is present in memory and should be handled
		};

	private:
		unsigned m_FlashStart, m_FlashEnd, m_FlashSize;
		enum{MAIN_SEGMENT_SIZE = 512};

		struct SegmentRecord
		{
			unsigned BpState[MAIN_SEGMENT_SIZE / 2];
			unsigned short OriginalInstructions[MAIN_SEGMENT_SIZE /2];
			
			int PendingBreakpointCount;

			SegmentRecord()
			{
				memset(BpState, 0, sizeof(BpState));
				memset(OriginalInstructions, 0, sizeof(OriginalInstructions));
				PendingBreakpointCount = 0;
			}

			bool SetBreakpoint(unsigned offset);
			bool RemoveBreakpoint(unsigned offset);
		};

		std::vector<SegmentRecord> m_Segments;
		unsigned short m_BreakInstruction;

		struct TranslatedAddr
		{
			bool Valid;
			unsigned Segment, Offset;
		};

	private:
		TranslatedAddr TranslateAddress(unsigned addr);
		
	public:
		bool SetBreakpoint(unsigned addr);
		bool RemoveBreakpoint(unsigned addr);
		bool CommitBreakpoints();

		BreakpointState GetBreakpointState(unsigned addr);
		bool GetOriginalInstruction(unsigned addr, unsigned short *pInsn);

	public:
		void HideOrRestoreBreakpointsInMemorySnapshot(unsigned addr, void *pBlock, size_t length, bool hideBreakpoints);

	public:
		SoftwareBreakpointManager(unsigned flashStart, unsigned flashEnd, unsigned short breakInstruction);
	};
}

