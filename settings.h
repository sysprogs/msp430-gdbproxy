#pragma once

namespace MSP430Proxy
{
	enum BreakpointPolicy
	{
		HardwareOnly,
		HardwareThenSoftware,
		SoftwareOnly,
	};

	enum HardwareInterface
	{
		UnspecifiedInterface,
		Jtag,
		SpyBiWare,
		JtagOverSpyBiWare,
		AutomaticInterface,
	};

	enum HardwareInterfaceSpeed
	{
		UnspecifiedSpeed,
		Fast,
		Medium,
		Slow
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
		bool Verbose;
		HardwareInterface Interface;
		HardwareInterfaceSpeed InterfaceSpeed;
		bool Emulate32BitRegisters;

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
			Verbose = false;
			Interface = UnspecifiedInterface;
			InterfaceSpeed = UnspecifiedSpeed;
			Emulate32BitRegisters = false;
		}
	};
}