#include "StdAfx.h"
#include "SoftwareBreakpointManager.h"
#include <bzscore/assert.h>
#include <MSP430_Debug.h>

using namespace MSP430Proxy;

MSP430Proxy::SoftwareBreakpointManager::SoftwareBreakpointManager( unsigned flashStart, unsigned flashEnd, unsigned short breakInstruction, bool instantCleanup, bool verbose )
	: m_FlashStart(flashStart)
	, m_FlashEnd(flashEnd)
	, m_FlashSize(flashEnd - flashStart + 1)
	, m_BreakInstruction(breakInstruction)
	, m_bInstantCleanup(instantCleanup)
	, m_bVerbose(verbose)
{
	ASSERT(!(m_FlashSize & 1));
	size_t segmentCount = (m_FlashSize + MAIN_SEGMENT_SIZE - 1) / MAIN_SEGMENT_SIZE;
	m_Segments.resize(segmentCount);
}

bool MSP430Proxy::SoftwareBreakpointManager::SetBreakpoint( unsigned rawAddr )
{
	TranslatedAddr addr = TranslateAddress(rawAddr);
	if (!addr.Valid)
		return false;

	return m_Segments[addr.Segment].SetBreakpoint(addr.Offset);
}

MSP430Proxy::SoftwareBreakpointManager::TranslatedAddr MSP430Proxy::SoftwareBreakpointManager::TranslateAddress( unsigned addr )
{
	TranslatedAddr result;
	result.Valid = false;

	addr &= ~1;

	if (addr < m_FlashStart)
		return result;

	unsigned flashOff = addr - m_FlashStart;
	if (flashOff >= m_FlashSize)
		return result;

	result.Segment = flashOff / MAIN_SEGMENT_SIZE;
	result.Offset = flashOff % MAIN_SEGMENT_SIZE;
	result.Valid = true;

	return result;
}

bool MSP430Proxy::SoftwareBreakpointManager::RemoveBreakpoint( unsigned rawAddr )
{
	TranslatedAddr addr = TranslateAddress(rawAddr);
	if (!addr.Valid)
		return false;

	return m_Segments[addr.Segment].RemoveBreakpoint(addr.Offset);
}

bool MSP430Proxy::SoftwareBreakpointManager::SegmentRecord::SetBreakpoint( unsigned offset )
{
	switch(BpState[offset / 2])
	{
	case BreakpointActive:
	case BreakpointPending:
		return false;	//Breakpoint is already set
	case BreakpointInactive:
		InactiveBreakpointCount--;
		BpState[offset / 2] = BreakpointActive;
		return true;
	case NoBreakpoint:
		PendingBreakpointCount++;
		BpState[offset / 2] = BreakpointPending;
		return true;
	default:
		return false;
	}
}

bool MSP430Proxy::SoftwareBreakpointManager::SegmentRecord::RemoveBreakpoint( unsigned offset )
{
	switch(BpState[offset / 2])
	{
	case BreakpointActive:
		InactiveBreakpointCount++;
		BpState[offset / 2] = BreakpointInactive;
		return true;
	case BreakpointPending:
		PendingBreakpointCount--;
		BpState[offset / 2] = NoBreakpoint;
		return true;
	case BreakpointInactive:
	case NoBreakpoint:
		return false;
	default:
		return false;
	}
}

bool MSP430Proxy::SoftwareBreakpointManager::CommitBreakpoints()
{
	for (size_t i = 0; i < m_Segments.size(); i++)
	{
		if (!m_Segments[i].PendingBreakpointCount)
		{
			if (!m_bInstantCleanup || !m_Segments[i].InactiveBreakpointCount)
				continue;
		}

		unsigned segBase = m_FlashStart + i * MAIN_SEGMENT_SIZE;
		unsigned short data[MAIN_SEGMENT_SIZE / 2], data2[MAIN_SEGMENT_SIZE / 2];
		if (MSP430_Read_Memory(segBase, (char *)data, sizeof(data)) != STATUS_OK)
			return false;

		bool eraseNeeded = false;

		for (size_t j = 0; j < MAIN_SEGMENT_SIZE / 2; j++)
		{
			switch(m_Segments[i].BpState[j])
			{
			case BreakpointInactive:
				m_Segments[i].BpState[j] = NoBreakpoint;
				data[j] = m_Segments[i].OriginalInstructions[j];
				eraseNeeded = true;
				if (m_bVerbose)
					printf("Restoring original FLASH instruction at 0x%x\n", segBase + j * 2);
				break;
			case BreakpointPending:
				m_Segments[i].BpState[j] = BreakpointActive;
				m_Segments[i].OriginalInstructions[j] = data[j];
				
				if ((data[j] & m_BreakInstruction) != m_BreakInstruction)
					eraseNeeded = true;

				if (m_bVerbose)
					printf("Inserting a FLASH breakpoint at 0x%x, sector erase %s\n", segBase + j * 2, eraseNeeded ? "pending" : "not pending");

				data[j] = m_BreakInstruction;
				break;
			}
		}

		for (;;)
		{
			if (eraseNeeded)
			{
				if (m_bVerbose)
					printf("Erasing FLASH segment at 0x%x-0x%x\n", segBase, segBase + sizeof(data) - 1);

				if (MSP430_Erase(ERASE_SEGMENT, segBase, sizeof(data)) != STATUS_OK)
					return false;
			}

			if (MSP430_Write_Memory(segBase, (char *)data, sizeof(data)) != STATUS_OK)
				return false;

			if (MSP430_Read_Memory(segBase, (char *)data2, sizeof(data2)) != STATUS_OK)
				return false;

			if (memcmp(data, data2, sizeof(data)))
			{
				if (!eraseNeeded)
				{
					eraseNeeded = true;
					continue;
				}
				return false;
			}

			break;
		}

		m_Segments[i].PendingBreakpointCount = 0;
		m_Segments[i].InactiveBreakpointCount = 0;
	}

	return true;
}

MSP430Proxy::SoftwareBreakpointManager::BreakpointState MSP430Proxy::SoftwareBreakpointManager::GetBreakpointState( unsigned rawAddr )
{
	TranslatedAddr addr = TranslateAddress(rawAddr);
	if (!addr.Valid)
		return NoBreakpoint;

	return (BreakpointState)m_Segments[addr.Segment].BpState[addr.Offset / 2];
}

bool MSP430Proxy::SoftwareBreakpointManager::GetOriginalInstruction( unsigned rawAddr, unsigned short *pInsn )
{
	TranslatedAddr addr = TranslateAddress(rawAddr);
	if (!addr.Valid)
		return false;

	switch (m_Segments[addr.Segment].BpState[addr.Offset / 2])
	{
	case BreakpointActive:
	case BreakpointInactive:
		*pInsn = m_Segments[addr.Segment].OriginalInstructions[addr.Offset / 2];
		return true;
	default:
		return false;
	}
}

void MSP430Proxy::SoftwareBreakpointManager::HideOrRestoreBreakpointsInMemorySnapshot( unsigned addr, void *pBlock, size_t length, bool hideBreakpoints )
{
	//Adjust pBlock so that it starts inside the FLASH region or past it
	if (addr < m_FlashStart)
	{
		unsigned delta = m_FlashStart - addr;

		if (delta >= length)
			return;	//Nothing to do, the block does not cover any FLASH addresses

		addr += delta;
		length -= delta;
		pBlock = ((char *)pBlock + delta);
	}

	int bufferOffsetInFlash = addr - m_FlashStart;

	size_t indexInBlock = 0;
	for (size_t seg = bufferOffsetInFlash / MAIN_SEGMENT_SIZE; seg < m_Segments.size(); seg++)
	{
		unsigned baseAddr = m_FlashStart + seg * MAIN_SEGMENT_SIZE;

		size_t i = 0;
		if (baseAddr < addr)
			i = addr - baseAddr;	//First segment covered by the block

		while (i < MAIN_SEGMENT_SIZE)
		{
			if (indexInBlock >= length)
				return;

			switch(m_Segments[seg].BpState[i / 2])
			{
			case BreakpointActive:
			case BreakpointInactive:
				if (hideBreakpoints)
					((char *)pBlock)[indexInBlock] = ((char *)m_Segments[seg].OriginalInstructions)[i];
				else
					((char *)m_Segments[seg].OriginalInstructions)[i] = ((char *)pBlock)[indexInBlock];
				break;
			}

			i++;
			indexInBlock++;
		}
	}
}
