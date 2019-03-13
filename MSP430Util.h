#pragma once
#include "TI/Inc/msp430.h"

//! Returns the string representation of the last error reported by the MSP430 API
static const char *GetLastMSP430Error()
{
	return MSP430_Error_String(MSP430_Error_Number());
}