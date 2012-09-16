#pragma once
#include "../../GDBServerFoundation/GDBServer.h"
#include "../../GDBServerFoundation/GDBStub.h"
#include "../../GDBServerFoundation/IGDBTarget.h"
#include <MSP430_Debug.h>

#include "registers-msp430.h"
#include <vector>
#include "settings.h"

enum MSP430_MSG;

namespace MSP430Proxy
{
	using namespace GDBServerFoundation;

	//! Implements debugging functionality without the EEM API (not supporting data breakpoints and software breakpoints).
	/*!
		\remarks After creating an instance of this class please call the Initialize() method.
	*/
	class MSP430GDBTarget : public ISyncGDBTarget, public IFLASHProgrammer
	{
	protected:
		DEVICE_T m_DeviceInfo;

	private:
		bool m_bClosePending, m_bValid;
		std::vector<bool> m_UsedBreakpoints;
		
		bool m_bFLASHErased;

	protected:
		bool m_BreakInPending, m_bFLASHCommandsUsed;

	protected:
		virtual bool WaitForJTAGEvent();
		void ReportLastMSP430Error(const char *pHint);
		virtual bool DoResumeTarget(RUN_MODES_t mode);

	public:
		MSP430GDBTarget()
			: m_bClosePending(false)
			, m_bValid(false)
			, m_BreakInPending(false)
			, m_bFLASHErased(false)
			, m_bFLASHCommandsUsed(false)
		{
		}

		~MSP430GDBTarget();

		//! Starts debugging session
		virtual bool Initialize(const GlobalSettings &settings);

		virtual GDBStatus GetLastStopRecord(TargetStopRecord *pRec);
		virtual GDBStatus ResumeAndWait(int threadID);
		virtual GDBStatus Step(int threadID);
		virtual GDBStatus SendBreakInRequestAsync();

		virtual const PlatformRegisterList *GetRegisterList()
		{
			return &GDBServerFoundation::MSP430::RegisterList;
		}

	public:	//Register accessing API
		virtual GDBStatus ReadFrameRelatedRegisters(int threadID, RegisterSetContainer &registers);
		virtual GDBStatus ReadTargetRegisters(int threadID, RegisterSetContainer &registers);
		virtual GDBStatus WriteTargetRegisters(int threadID, const RegisterSetContainer &registers);

	public: //Memory accessing API
		virtual GDBStatus ReadTargetMemory(ULONGLONG Address, void *pBuffer, size_t *pSizeInBytes);
		virtual GDBStatus WriteTargetMemory(ULONGLONG Address, const void *pBuffer, size_t sizeInBytes);

	public:	//Optional methods, can be left unimplemented
		virtual GDBStatus GetDynamicLibraryList(std::vector<DynamicLibraryRecord> &libraries);
		virtual GDBStatus GetThreadList(std::vector<ThreadRecord> &threads);
		virtual GDBStatus SetThreadModeForNextCont(int threadID, DebugThreadMode mode, OUT bool *pNeedRestoreCall, IN OUT INT_PTR *pRestoreCookie);
		virtual GDBStatus Terminate();

		virtual GDBStatus CreateBreakpoint(BreakpointType type, ULONGLONG Address, unsigned kind, OUT INT_PTR *pCookie);
		virtual GDBStatus RemoveBreakpoint(BreakpointType type, ULONGLONG Address, INT_PTR Cookie);

		virtual GDBStatus ExecuteRemoteCommand(const std::string &command, std::string &output);

		virtual IFLASHProgrammer *GetFLASHProgrammer() {return this;}

	public:
		virtual GDBStatus GetEmbeddedMemoryRegions(std::vector<EmbeddedMemoryRegion> &regions);
		virtual GDBStatus EraseFLASH(ULONGLONG addr, size_t length);
		virtual GDBStatus WriteFLASH(ULONGLONG addr, const void *pBuffer, size_t length);
		virtual GDBStatus CommitFLASHWrite();
		virtual void CloseSessionSafely() {}
	};
}
