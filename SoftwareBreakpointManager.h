#pragma once
#include <vector>
#include <list>

namespace MSP430Proxy
{
	//! Allows setting and removing software breakpoints in FLASH.
	/*! This class sets and removes software breakpoints in the FLASH memory of an MSP430 device.
		It minimizes the amount of write/erase cycles by queuing the breakpoint requests and only modifying the FLASH when CommitBreakpoints() method is called.
		\section soft_breakpoint_mechanism Software breakpoint mechanism
		The MSP430 devices implement software breakpoints by programming one of the hardware breakpoints to be triggered when a certain instruction is executed.
		Typically, the "MOV.B r3,r3" instruction with code 0x4343 is used for software breakpoints. The instruction itself does nothing, and should not normally
		appear in the normal MSP430 code. When the hardware breakpoint is programmed accordingly, replacing any actual instruction with the breakpoint instruction
		will cause the breakpoint to trigger at that address without consuming any additional hardware breakpoint resources.
		Note that setting and removing the breakpoints in FLASH requires erasing and writing the FLASH and thus consumes the write cycles.
	*/
	class SoftwareBreakpointManager
	{
	public:
		enum BreakpointState
		{
			//!Breakpoint is not set at this address
			NoBreakpoint = 0,	
			//!Breakpoint will be created in the next call to CommitBreakpoints()
			BreakpointPending,	
			//!Breakpoint is present in memory, but should be ignored and removed next time the segment is overwritten
			BreakpointInactive,	
			//!Breakpoint is present in memory and should be handled
			BreakpointActive,	
		};

	private:
		unsigned m_FlashStart, m_FlashEnd, m_FlashSize;
		enum{MAIN_SEGMENT_SIZE = 512};

		//! Contains the information about breakpoints in a single FLASH segment that can be erased in one operation
		struct SegmentRecord
		{
			unsigned BpState[MAIN_SEGMENT_SIZE / 2];
			unsigned short OriginalInstructions[MAIN_SEGMENT_SIZE /2];
			
			int PendingBreakpointCount, InactiveBreakpointCount;

			SegmentRecord()
			{
				memset(BpState, 0, sizeof(BpState));
				memset(OriginalInstructions, 0, sizeof(OriginalInstructions));
				PendingBreakpointCount = InactiveBreakpointCount = 0;
			}

			bool SetBreakpoint(unsigned offset);
			bool RemoveBreakpoint(unsigned offset);
		};

		std::vector<SegmentRecord> m_Segments;
		unsigned short m_BreakInstruction;

		bool m_bInstantCleanup;
		bool m_bVerbose;

		struct TranslatedAddr
		{
			bool Valid;
			unsigned Segment, Offset;
		};

	private:
		TranslatedAddr TranslateAddress(unsigned addr);
		
	public:
		//! Queues a breakpoint set request until the next call to CommitBreakpoints()
		bool SetBreakpoint(unsigned addr);
		//! Queues a breakpoint removal request until the next call to RemoveBreakpoints()
		bool RemoveBreakpoint(unsigned addr);
		//! Modifies the FLASH memory to reflect the changed breakpoints
		bool CommitBreakpoints();

		//! Returns the state of a breakpoint at a given address
		BreakpointState GetBreakpointState(unsigned addr);

		//! Returns the original instruction that was present at a given address before the breakpoint was set
		bool GetOriginalInstruction(unsigned addr, unsigned short *pInsn);

	public:
		//! Modifies the given memory snapshot to hide or show the software breakpoints
		/*! This method is used to hide the software breakpoints from the memory dumps sent to gdb so that
			the gdb disassembler still shows the original code.
		*/
		void HideOrRestoreBreakpointsInMemorySnapshot(unsigned addr, void *pBlock, size_t length, bool hideBreakpoints);

	public:
		//! Creates an instance of the SoftwareBreakpointManager class
		/*!
			\param flashStart Specifies the address where the FLASH memory starts
			\param flashEnd Specifies the address of the last byte of the FLASH memory
			\param breakInstruction Specifies the instruction that is used as a software breakpoint. One of the hardware breakpoints
				   should be programmed to trigger when this instruction gets executed.
			\param instantCleanup If this argument is set to false, removing a breakpoint won't cause a FLASH rewrite cycle.
				   Instead, the breakpoint will be marked as inactive (when it hits, the software should ignore it and resume execution).
				   In this mode the inactive breakpoints will be physically removed only when the same FLASH block is erased and rewritten
				   to set another breakpoint.
			\remarks The size of the FLASH erase block is assumed to be a constant of 512 bytes.
		*/
		SoftwareBreakpointManager(unsigned flashStart, unsigned flashEnd, unsigned short breakInstruction, bool instantCleanup, bool verbose);
	};
}

