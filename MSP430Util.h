#pragma once
#include <MSP430.h>

static const char *GetLastMSP430Error()
{
	return MSP430_Error_String(MSP430_Error_Number());
}