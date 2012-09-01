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
}