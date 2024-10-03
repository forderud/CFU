/*++
    MIT License
    
    Copyright (C) Microsoft Corporation. All rights reserved.

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE


Module Name:

    HidCommands.h

Abstract:
    
    Object communicating with a device with the Human Interface Device (HID)
    Application Programming Interface (API).

Environment:

    User mode.

--*/

#pragma once

#include <iostream>
#include <fstream>
#include <list>
#include <time.h>
#include <hidusage.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <cfgmgr32.h>
#include "HidUpdateCfu.h"

#define REPORT_LENGTH_STANDARD 61
#define READ_THREAD_TIMEOUT_MS          1000 //MS Set this value based on something reasonable for your FW device arch
#define READ_THREAD_TIMEOUT_FOREVER_MS  1000000
#define INFINITE_READS           ((ULONG)-1)


typedef struct HidReportIdInfo
{
    UINT8 id;
    USAGE Usage;
    UINT16 size;
    HIDP_REPORT_TYPE inOutFeature;
    PCWSTR Name;
} _HidReportIdInfo;

typedef struct HID_DEVICE
{
    HANDLE hDevice;
    PHIDP_PREPARSED_DATA PreparsedData;
    HIDP_CAPS Caps;
    char* InputReportBuffer;

    HID_DEVICE() noexcept:
        hDevice(INVALID_HANDLE_VALUE),
        PreparsedData(NULL),
        InputReportBuffer(NULL),
        Caps({ 0 }) {}

    ~HID_DEVICE()
    {
        if (PreparsedData != NULL)
        {
            HidD_FreePreparsedData(PreparsedData);
        }

        if (hDevice != INVALID_HANDLE_VALUE)
        {
            CloseHandle(hDevice);
        }
    }

} _HID_DEVICE, *PHID_DEVICE;


class HidCommands
{
public:
    HidCommands() noexcept { }
    virtual ~HidCommands() { }

    // Returns the last Win32 error, in string format. 
    // Returns an empty string if there is no error.
    static std::string GetLastErrorAsString()
    {
        //Get the error message, if any.
        std::string message;
        UINT32 errorMessageID = ::GetLastError();
        if (errorMessageID == 0) {
            // No error message has been recorded
            return message;
        }

        LPSTR messageBuffer = NULL;
        size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

        message = std::string(messageBuffer, size);

        //Free the buffer.
        LocalFree(messageBuffer);
        return message;
    }

    static BOOL SetOutputReport(
        _In_ HID_DEVICE& Device,
        _In_ char* reportBuffer,
        _In_ UINT32 reportLength)
    {
        BOOL fSucceeded = FALSE;

#if defined(_DEBUG)
        wprintf(L"HidD_SetOutputReport of reportId: %d length: %d\n", 
                reportBuffer[0], reportLength);
#endif
        fSucceeded = HidD_SetOutputReport(Device.hDevice, reportBuffer, reportLength);
        if (fSucceeded)
        {
#if defined(_DEBUG)
            printf("HidD_SetOutputReport %d %d\n", reportBuffer[0], reportLength);
#endif
        }
        else
        {
            printf("error HidD_SetOutputReport %d: %s\n", 
                    GetLastError(), GetLastErrorAsString().c_str());
        }
        return fSucceeded;
    }

    static BOOL GetFeatureReport(
        _In_ HID_DEVICE& Device,
        _In_ USAGE UsagePage,
        _In_ USAGE Usage,
        _Out_ char* reportBuffer,
        _In_ UINT32 maxReportSize,
        _Out_ UINT32& reportLength)
    {
        PHIDP_VALUE_CAPS valCaps = NULL;
        char* readReportBuffer = NULL;
        UINT16 featureValCapsCount = Device.Caps.NumberFeatureValueCaps;
        NTSTATUS status;
        BOOL fSucceeded = FALSE;
        reportLength = 0;

        if (reportBuffer == NULL)
        {
            printf("reportBuffer is NULL. This is a failure.\n");
            goto exit;
        }

        memset(reportBuffer, 0, maxReportSize);

        // make sure we have at least 1 feature capability
        if (featureValCapsCount == 0)
        {
            goto exit;
        }

        // Allocate enough VALUE_CAPS for the total amount of feature Caps
        valCaps = new HIDP_VALUE_CAPS[featureValCapsCount];
        if (valCaps == NULL)
        {
            printf("Failed to allocate space for valCaps %d\n", featureValCapsCount);
            goto exit;
        }

        // Retrieving Specific Value Caps that match our needed Usage
        status = HidP_GetSpecificValueCaps(
            HidP_Feature,
            UsagePage,
            0,
            Usage,
            valCaps,
            &featureValCapsCount,
            Device.PreparsedData);
        if (!NT_SUCCESS(status))
        {
            // Specific value cap not found.  
            // Not an error but means we aren't talking to the correct device
            goto exit;
        }

        //Check if the new filtered list has 1 report to query for
        if (featureValCapsCount != 1)
        {
            goto exit;
        }

        UINT32 featureReportByteLengthMax = Device.Caps.FeatureReportByteLength;
 
        // Create buffer equal to the Caps Report Length maximum
        readReportBuffer = new char[featureReportByteLengthMax];
        if (readReportBuffer == NULL)
        {
            printf("Failed to allocate space for readReportBuffer %d\n", 
                    featureReportByteLengthMax);
            goto exit;
        }

        // Populate the first byte of the buffer with the reportID related 
        // to the cap we filtered for. Without this HidD_GetFeature doesn't 
        // know which usage to retrieve.
        readReportBuffer[0] = valCaps[0].ReportID;

        if (!HidD_GetFeature(Device.hDevice, readReportBuffer, featureReportByteLengthMax))
        {
            printf("error HidD_GetFeature %d: %s\n", 
                    GetLastError(), 
                    GetLastErrorAsString().c_str());
            goto exit;
        }

        reportLength = Device.Caps.FeatureReportByteLength;
        memcpy_s(reportBuffer, maxReportSize, readReportBuffer, reportLength);
        fSucceeded = TRUE;

    exit:
        SAFE_DELETEA(valCaps);
        SAFE_DELETEA(readReportBuffer);
        return fSucceeded;
    }

    static void printBuffer(
        _In_ char* reportBuffer,
        _In_ UINT32 reportLength)
    {
        // Print the reportId on its own line
        printf("0x%02X", (UINT8)reportBuffer[0]);

        // Print the rest of the data in 8 bytes per line
        for (UINT i = 1; i < reportLength; i++)
        {
            if( (i-1) % 8 == 0) printf("\n");
            printf("0x%02X ", (UINT8)reportBuffer[i]);
        }
        printf("\n\n");
    }

    typedef struct _READ_THREAD_CONTEXT
    {
        PHID_DEVICE HidDevice;
        ULONG       NumberOfReads;
        BOOL        TerminateThread;
        HANDLE      readEvent;
    } READ_THREAD_CONTEXT, *PREAD_THREAD_CONTEXT;

    static BOOL
        ReadOverlapped(
            PHID_DEVICE     HidDevice,
            HANDLE          CompletionEvent,
            LPOVERLAPPED    pOverlap
        )
       
        //++
        //RoutineDescription:
        //Given a struct _HID_DEVICE, obtain a read report and unpack the values
        //into the InputData array.
        //++
    {
        DWORD       bytesRead;
        BOOL        readStatus;

        // Setup the overlap structure using the completion event passed in to
        // to use for signaling the completion of the Read

        memset(pOverlap, 0, sizeof(*pOverlap));
        pOverlap->hEvent = CompletionEvent;

        // Execute the read call saving the return code to determine how to
        // proceed (ie. the read completed synchronously or not).

        readStatus = ReadFile(HidDevice->hDevice,
            HidDevice->InputReportBuffer,
            HidDevice->Caps.InputReportByteLength,
            &bytesRead,
            pOverlap);

        // If the readStatus is FALSE, then one of two cases occurred.
        //  1. ReadFile call succeeded but the Read is an overlapped one.  Here,
        //     we should return TRUE to indicate that the Read succeeded.  However,
        //     the calling thread should be blocked on the completion event
        //     which means it won't continue until the read actually completes
        //
        //  or
        //
        //  2. The ReadFile call failed for some unknown reason...In this case,
        //     the return code will be FALSE

        if (!readStatus)
        {
            if (ERROR_IO_PENDING == GetLastError() || ERROR_OPERATION_ABORTED == GetLastError())
            {
                return TRUE;
            }
            else
            {
                wprintf(L"ReadFile failed %d\n", GetLastError());
                return FALSE;
            }
        }

        // If readStatus is TRUE, then the ReadFile call completed synchronously,
        // since the calling thread is probably going to wait on the completion
        // event, signal the event so it knows it can continue.
        else
        {
            SetEvent(CompletionEvent);
            return (TRUE);
        }
    }

    static UINT32 WINAPI
        AsynchReadThreadProc(
            PREAD_THREAD_CONTEXT    Context
        )
    {
        HANDLE  completionEvent = NULL;
        BOOL    readResult = FALSE;
        UINT32   waitStatus = 0;
        OVERLAPPED overlap = { 0 };
        DWORD   bytesTransferred = 0;

        // Create the completion event to send to the the OverlappedRead routine
        completionEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

        // If NULL returned, then we cannot proceed any farther so we just exit the thread

        if (NULL == completionEvent)
        {
            goto AsyncRead_End;
        }

        // Now we enter the main read loop, which does the following:
        //  1. Calls ReadOverlapped()
        //  2. Waits for read completion with a timeout just to check if 
        //      the main thread wants us to terminate our the read request
        //  3. If the read fails, we simply break out of the loop
        //      and exit the thread
        //  4. If the read succeeds, we call UnpackReport to get the relevant
        //      info and then post a message to main thread to indicate that
        //      there is new data to display.
        //  5. We then block on the display event until the main thread says
        //      it has properly displayed the new data
        //  6. Look to repeat this loop if we are doing more than one read
        //      and the main thread has yet to want us to terminate

        Context->HidDevice->InputReportBuffer = 
            (char*)malloc(Context->HidDevice->Caps.InputReportByteLength);
        if (!Context->HidDevice->InputReportBuffer)
        {
            printf("Failed to allocate read buffer\n");
            return -1;
        }

        do
        {
            // Call ReadOverlapped() and if the return status is TRUE, the ReadFile
            // succeeded so we need to block on completionEvent, otherwise, we just exit
            readResult = HidCommands::ReadOverlapped(Context->HidDevice, completionEvent, &overlap);
            if (readResult)
            {
                while (!Context->TerminateThread)
                {
                    // Wait for the completion event to be signaled or a timeout
                    waitStatus = WaitForSingleObject(completionEvent, READ_THREAD_TIMEOUT_MS);
                    // If completionEvent was signaled, then a read just completed
                    //   so let's get the status and leave this loop and process the data 
                    if (WAIT_OBJECT_0 == waitStatus)
                    {
                        readResult = 
                            GetOverlappedResult(
                                    Context->HidDevice->hDevice, 
                                    &overlap, 
                                    &bytesTransferred, 
                                    TRUE);
                        SetEvent(Context->readEvent);
                        break;
                    }
                }
            }

            // Check the TerminateThread again...If it is not set, then data has
            // been read.  In this case, we want to Unpack the report into our
            // input info and then send a message to the main thread to display
            // the new data.
            if (Context->TerminateThread)
            {                
                break;
            }
        } while (1);
    AsyncRead_End:
        return (0);
    }

    static BOOL PopulateReportId(
        _In_ HID_DEVICE& device,
        _Inout_ HidReportIdInfo& ReportSettings)
    {
        PHIDP_VALUE_CAPS valCaps = NULL;
        BOOL ret = false;
        UINT16 capCount = 0;

        if (!HidD_GetPreparsedData(device.hDevice, &device.PreparsedData))
        {
            printf("HidD_GetPreparsedData %d\n", GetLastError());
            return false;
        }

        NTSTATUS status = HidP_GetCaps(device.PreparsedData, &device.Caps);
        if (!NT_SUCCESS(status))
        {
            printf("HidP_GetCaps %d\n", status);
            return false;
        }

        capCount = 
            device.Caps.NumberFeatureValueCaps +
            device.Caps.NumberInputValueCaps +
            device.Caps.NumberOutputValueCaps;

        // Allocate enough VALUE_CAPS for the total amount of feature+in+out val Caps
        valCaps = new HIDP_VALUE_CAPS[capCount];
        if (valCaps == NULL)
        {
            printf("Failed to allocate space for valCaps %d\n", capCount);
            return false;
        }

#if defined(_DEBUG)
        printf("grabbing usage %X, type %d from collection of %d\n", 
                ReportSettings.Usage, ReportSettings.inOutFeature, capCount);
#endif
        status = HidP_GetSpecificValueCaps(
            ReportSettings.inOutFeature,
            0,
            0,
            ReportSettings.Usage,
            valCaps,
            &capCount,
            device.PreparsedData);
        if (!NT_SUCCESS(status) || (capCount == 0))
        {
            printf("failed grabbing usage %X, type %d error: %x\n", 
                    ReportSettings.Usage, ReportSettings.inOutFeature, status);
            goto Exit;
        }

        ReportSettings.id = valCaps[0].ReportID;
        ret = true;

    Exit:
        SAFE_DELETEA(valCaps);
        return ret;
    }
};
