#pragma once
#include "../../GDBServerFoundation/GDBRegisters.h"

namespace GDBServerFoundation
{
	namespace MSP430
	{
		//! Registers are numbered from r0 to r15
		typedef int RegisterIndex;

		static RegisterEntry _RawRegisterList[] = {
			{0, "r0", 16},
			{1, "r1", 16},
			{2, "r2", 16},
			{3, "r3", 16},
			{4, "r4", 16},
			{5, "r5", 16},
			{6, "r6", 16},
			{7, "r7", 16},
			{8, "r8", 16},
			{9, "r9", 16},
			{10, "r10", 16},
			{11, "r11", 16},
			{12, "r12", 16},
			{13, "r13", 16},
			{14, "r14", 16},
			{15, "r15", 16},
		};

		static PlatformRegisterList RegisterList = 
		{
			sizeof(_RawRegisterList) / sizeof(_RawRegisterList[0]),
			_RawRegisterList,
		};

	}

	//gdb 7.7 and onward requires 32-bit register values
	namespace MSP430_32bitRegs
	{
		//! Registers are numbered from r0 to r15
		typedef int RegisterIndex;

		static RegisterEntry _RawRegisterList[] = {
			{0, "r0", 32},
			{1, "r1", 32},
			{2, "r2", 32},
			{3, "r3", 32},
			{4, "r4", 32},
			{5, "r5", 32},
			{6, "r6", 32},
			{7, "r7", 32},
			{8, "r8", 32},
			{9, "r9", 32},
			{10, "r10", 32},
			{11, "r11", 32},
			{12, "r12", 32},
			{13, "r13", 32},
			{14, "r14", 32},
			{15, "r15", 32},
		};

		static PlatformRegisterList RegisterList = 
		{
			sizeof(_RawRegisterList) / sizeof(_RawRegisterList[0]),
			_RawRegisterList,
		};

	}
}