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



class SimpleStub : public GDBStub
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
	SimpleStub(ISyncGDBTarget *pTarget)
		: GDBStub(pTarget, true)
	{
		pLogFile = fopen("gdbserver.log", "w");
	}

	~SimpleStub()
	{
		fclose(pLogFile);
	}
};

using namespace MSP430Proxy;

class MSP430StubFactory : public IGDBStubFactory
{
private:
	std::string m_Port;

public:
	MSP430StubFactory(const char *pPort)
		: m_Port(pPort)
	{
	}

	virtual IGDBStub *CreateStub()
	{
		MSP430GDBTarget *pTarget = new MSP430EEMTarget();
//		MSP430GDBTarget *pTarget = new MSP430GDBTarget();

		if (!g_SessionMonitor.RegisterSession(pTarget))
		{
			printf("Cannot start a new debugging session before the old session ends.\n");
			return NULL;
		}

		if (!pTarget->Initialize(m_Port.c_str()))
		{
			printf("Failed to initialize MSP430 debugging engine. Aborting.\n");
			delete pTarget;
			return NULL;
		}
		printf("New GDB debugging session started.\n");
		return new SimpleStub(pTarget);
	}

	virtual void OnProtocolError(const TCHAR *errorDescription)
	{
		_tprintf(_T("Protocol error: %s\n"), errorDescription);
	}
};

#include <msp430.h>
#include "MSP430Util.h"

int _tmain(int argc, _TCHAR* argv[])
{
	const char *pPort = "USB";

	LONG version = 0;
	STATUS_T status = MSP430_Initialize((char *)pPort, &version);
	if (status != STATUS_OK)
	{
		printf("Cannot initalize MSP430.DLL on port %s: %s\n", pPort, GetLastMSP430Error());
		return 1;
	}
	MSP430_Close(FALSE);

	GDBServer srv(new MSP430StubFactory(pPort));
	srv.Start(2000);
	Sleep(INFINITE);

	return 0;
}
