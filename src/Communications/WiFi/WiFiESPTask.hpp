//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \file       WiFiESPTask.hpp

    \brief      WiFi ESP32 Wireless driver RTOS Task and Events

    Copyright (c) 2023 Sindarin, Inc. - All Right Reserved

    The code contained in this file is confidential and may only be used
    under license and/or written consent of Sindarin.
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------

#pragma once

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#include "WiFiESPDefs.hpp"

//-----------------------------------------------------------------------------
// Defines
//-----------------------------------------------------------------------------
#define WIFIESP_TASK_CORENUMBER 1 // Core for the WIFIESP Task
#define WIFIESP_TASK_STACKSIZE 4096

//-----------------------------------------------------------------------------
// Externals
//-----------------------------------------------------------------------------

/// Post an Event to the task
extern "C" auto WiFiEspTaskPostEvent(wifiesp_event_t *data) -> esp_err_t;

/// Start the Network Task
extern "C" auto WiFiEspTaskStartNetworkEventTask() -> bool;

/// Set Event Status Bit
extern "C" auto WiFiEspTaskSetEventStatusBits(int bits) -> int;

/// Clear Event Status Bit
extern "C" auto WiFiEspTaskClearEventStatusBits(int bits) -> int;

/// Get Event Status Bit
extern "C" auto WiFiEspTaskGetEventStatusBits() -> int;

/// Wait for an Event Status Bit
extern "C" auto WiFiEspTaskWaitEventStatusBits(int bits, uint32_t timeoutMs) -> int;
