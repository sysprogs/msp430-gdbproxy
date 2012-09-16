#pragma once

namespace MSP430Proxy
{
	enum BreakpointPolicy
	{
		HardwareOnly,
		HardwareThenSoftware,
		SoftwareOnly,
	};

	struct GlobalSettings
	{
		bool EnableEEMMode;
		bool InstantBreakpointCleanup;
		unsigned short BreakpointInstruction;
		BreakpointPolicy SoftBreakPolicy;
		const char *PortName;
		unsigned Voltage;
		int ListenPort;
		bool SingleSessionOnly;
		bool AutoErase;
		bool NoHint;

		GlobalSettings()
		{
			EnableEEMMode = true;
			InstantBreakpointCleanup = true;
			BreakpointInstruction = 0x4343;
			SoftBreakPolicy = HardwareThenSoftware;
			PortName = "USB";
			Voltage = 3333;
			ListenPort = 2000;
			SingleSessionOnly = true;
			AutoErase = false;
			NoHint = false;
		}
	};
}