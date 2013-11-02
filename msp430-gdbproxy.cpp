// msp430-gdbproxy.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

// VBoxGDB.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <stdio.h>
#include "MSP430EEMTarget.h"
#include "GlobalSessionMonitor.h"

using namespace BazisLib;
using namespace GDBServerFoundation;

/*class StubWithLogging : public GDBStub
{
	FILE *pLogFile;

	virtual StubResponse HandleRequest(const BazisLib::TempStringA &requestType, char splitterChar, const BazisLib::TempStringA &requestData)
	{
		printf(">> %s%c%s\n", DynamicStringA(requestType).c_str(), splitterChar, DynamicStringA(requestData).c_str());
		fprintf(pLogFile, ">> %s%c%s\n", DynamicStringA(requestType).c_str(), splitterChar, DynamicStringA(requestData).c_str());
		StubResponse response = __super::HandleRequest(requestType, splitterChar, requestData);
		printf("<< %s\n", std::string(response.GetData(), response.GetSize()).c_str());
		fprintf(pLogFile, "<< %s\n", std::string(response.GetData(), response.GetSize()).c_str());
		fflush(pLogFile);
		return response;
	}

public:
	StubWithLogging(ISyncGDBTarget *pTarget)
		: GDBStub(pTarget, true)
	{
		pLogFile = fopen("gdbserver.log", "w");
	}

	~StubWithLogging()
	{
		fclose(pLogFile);
	}
};*/

using namespace MSP430Proxy;

typedef GDBStub StubImpl;

class MSP430StubFactory : public IGDBStubFactory
{
private:
	GlobalSettings m_Settings;

public:
	MSP430StubFactory(const GlobalSettings &settings)
		: m_Settings(settings)
	{
	}

	virtual IGDBStub *CreateStub(GDBServer *pServer)
	{
		MSP430GDBTarget *pTarget;


		if (!m_Settings.EnableEEMMode)
			printf("Warning: Non-EEM mode is no longer supported. Switching to EEM mode.\n");

		pTarget = new MSP430EEMTarget();

		if (!g_SessionMonitor.RegisterSession(pTarget))
		{
			printf("Cannot start a new debugging session before the old session ends.\n");
			return NULL;
		}

		printf("\nIncoming connection from GDB. Starting session...\n");

		if (!pTarget->Initialize(m_Settings))
		{
			printf("Failed to initialize MSP430 debugging engine. Aborting.\n");
			delete pTarget;
			return NULL;
		}

		if (m_Settings.SingleSessionOnly)
			pServer->StopListening();

		return new StubImpl(pTarget);
	}

	virtual void OnProtocolError(const TCHAR *errorDescription)
	{
		_tprintf(_T("Protocol error: %s\n"), errorDescription);
	}
};

#include <msp430.h>
#include "MSP430Util.h"

void ShowHelpScreen()
{
	printf("Usage: msp430-gdbproxy [options]\n\
All options are optional:\n\
  --noeem - Disable EEM mode (required for advanced breakpoints)\n\
  --keepbp - Keep software breakpoints in FLASH (reduces erase cycles)\n\
  --bp_insn=0xNNNN - Override software breakpoint instruction (default 0x4343)\n\
  --bpmode=<mode> - Specifies how to create breakpoints with \"break\" command:\n\
    soft - always create software breakpoints (run \"hbreak\" to override)\n\
    hard - always create hardware breakpoints, fail when out of them\n\
    auto - create hardware breakpoints while available, then software\n\
  --progport=<port> - Specify port for TI FET (default is \"USB\")\n\
  --voltage=<nnnn> - Specify Vcc voltage in mV (default = 3333)\n\
  --tcpport=<n> - Listen on TCP port n (default 2000)\n\
  --keepalive - Keep running after GDB disconnects, wait for next connection\n\
  --autoerase - Erase FLASH when debugging is started\n\
  --nohint - Do not show the 'how to start debugging' message\n\
  --verbose - Enable verbose diagnostic output\n\
  --iface=jtag/sbw/sbwjtag/auto - Specify connection interface\n\
  --ifacespeed=slow/medium/fast - Specify interface speed\n\
");
}

void ParseOptions(int argc, char* argv[], GlobalSettings &settings)
{
	for (int i = 1; i < argc; i++)
	{
		std::string arg;
		char *sep = strchr(argv[i], '=');
		char *val = NULL;
		if (sep)
			arg = std::string(argv[i], sep - argv[i]), val = sep + 1;
		else
			arg = argv[i];

		if (arg.length() < 2 || arg[0] != '-' || arg[1] != '-')
		{
			printf("Warning: unknown option: %s\n", arg.c_str());
			continue;
		}

		arg = arg.substr(2);

		if (arg == "noeem")
			settings.EnableEEMMode = false;
		else if (arg == "keepbp")
			settings.InstantBreakpointCleanup = false;
		else if (arg == "verbose")
			settings.Verbose = true;
		else if (arg == "bp_insn")
		{
			if (strlen(val) < 3 || _memicmp(val, "0x", 2))
			{
				printf("Warning: wrong bp_insn format\n");
				continue;
			}
			int insn = 0;
			sscanf(val, "%x", &insn);
			settings.BreakpointInstruction = insn;
		}
		else if (arg =="bpmode")
		{
			if (!val)
				continue;
			if (!strcmp(val, "soft"))
				settings.SoftBreakPolicy = SoftwareOnly;
			else if (!strcmp(val, "hard"))
				settings.SoftBreakPolicy = HardwareOnly;
			else if (!strcmp(val, "auto"))
				settings.SoftBreakPolicy = HardwareThenSoftware;
		}
		else if (arg == "progport")
			settings.PortName = val;
		else if (arg == "tcpport")
			settings.ListenPort = atoi(val);
		else if (arg == "voltage")
			settings.Voltage = atoi(val);
		else if (arg == "keepalive")
			settings.SingleSessionOnly = false;
		else if (arg == "autoerase")
			settings.AutoErase = true;
		else if (arg == "nohint")
			settings.NoHint = true;
		else if (arg =="iface")
		{
			if (!val)
				continue;
			if (!strcmp(val, "jtag"))
				settings.Interface = Jtag;
			else if (!strcmp(val, "sbw"))
				settings.Interface = SpyBiWare;
			else if (!strcmp(val, "sbwjtag"))
				settings.Interface = JtagOverSpyBiWare;
			else if (!strcmp(val, "auto"))
				settings.Interface = AutomaticInterface;
		}
		else if (arg =="ifacespeed")
		{
			if (!val)
				continue;
			if (!strcmp(val, "fast"))
				settings.InterfaceSpeed = Fast;
			else if (!strcmp(val, "medium"))
				settings.InterfaceSpeed = Medium;
			else if (!strcmp(val, "slow"))
				settings.InterfaceSpeed = Slow;
		}
	}
}

int main(int argc, char* argv[])
{
	GlobalSettings settings;
	setbuf(stdout, NULL); 

	if (argc >= 2 && !strcmp(argv[1], "--help"))
	{
		ShowHelpScreen();
		return 0;
	}

	ParseOptions(argc, argv, settings);

	LONG version = 0;
	STATUS_T status = MSP430_Initialize((char *)settings.PortName, &version);
	if (settings.Verbose)
		printf("MSP430_Initialize(%s) => status = %d, version = %d\n", settings.PortName, status, version);

	if (status != STATUS_OK)
	{
		printf("Cannot initalize MSP430.DLL on port %s:\n\t%s\n", settings.PortName, GetLastMSP430Error());
		printf("\nRun msp430-gdbproxy --help for usage instructions.\n");
		return 1;
	}

	status = MSP430_Close(FALSE);
	printf("MSP430_Close() returned %d\n", status);

	GDBServer srv(new MSP430StubFactory(settings));
	ActionStatus st = srv.Start(settings.ListenPort);
	if (!st.Successful())
	{
		_tprintf(_T("Cannot start listening on port %d: %s\n"), settings.ListenPort, st.GetMostInformativeText().c_str());
		return 1;
	}

	printf("msp430-gdbproxy++ v1.3 [http://gnutoolchains.com/msp430/gdbproxy]\nSuccessfully initialized MSP430.DLL on %s\nListening on port %d.\n", settings.PortName, settings.ListenPort);
	if (!settings.NoHint)
	{
		printf("\nRun \"msp430-gdbproxy --help\" to learn about command line options.\n\
To start debugging:\n\
\t1. Start \"msp430-gdb <yourfile.elf>\"\n\
\t2. Run the \"target remote :%d\" command in GDB.\n\
\t3. Run \"load\" to program the FLASH memory.\n\
\t4. In case of GDB errors, see this window for more info.\n", settings.ListenPort);

		printf("If you don't have msp430-gdb, visit http://gnutoolchains.com/msp430 to get it.\n\n");
	}

	srv.WaitForTermination();
	return 0;
}
