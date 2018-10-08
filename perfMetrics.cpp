////////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2018 Dawson Dean
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
// CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
// SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
/////////////////////////////////////////////////////////////////////////////
//
// Performance Stats
//
// This will eventually move to the building blocks.
// It is useful for perf tuning, particularly when a system is running 
// in production but still collecting usage statistics.
/////////////////////////////////////////////////////////////////////////////

#if WASM
#include "portableBuildingBlocks.h"
#else
#include "buildingBlocks.h"
#endif

#include "perfMetrics.h"

FILE_DEBUGGING_GLOBALS(LOG_LEVEL_DEFAULT, 0);


////////////////////////////////////////////////
// This is one counter.
class CPerfValueImpl : public CPerfValueAPI {
public:
    CPerfValueImpl() { }
    virtual ~CPerfValueImpl() { }
    
    // CPerfValueAPI
    virtual void SetIntValue(int32 value) { m_Value = value; }
    // These are used by MACROS, and for performance they are commented out in 
    // a non-instrumented build.
    virtual void StartTimer() { m_StartTime = GetTimeSinceBootInMs(); } 
    virtual uint64 StopTimer() {
        uint64 elapsedTime = GetTimeSinceBootInMs() - m_StartTime;
        m_Value = elapsedTime;
        return(m_Value);
    }
    virtual uint64 GetValue() { return(m_Value); }
    virtual void PrintValue(const char *pPrefixStr);

    int32           m_CounterType;
    const char      *m_pName;
    int32           m_NameLength;

    // For counter, this is just the total sum of all values.
    // For timers, this is the total time with this timer on.
    uint64          m_Value;

    // Start time is only used for timers, and is the most recent start time.
    uint64          m_StartTime;

    CPerfValueImpl  *m_pNextValue;
}; // CPerfValueImpl



////////////////////////////////////////////////
class CStatsGroupImpl: public CStatsGroupAPI {
public:
    CStatsGroupImpl();
    virtual ~CStatsGroupImpl();

    // CStatsGroupImpl
    virtual CPerfValueAPI *DeclareMetric(const char *pCounterName, int32 counterType);

    const char      *m_pName;
    int32           m_NameLength;

    CPerfValueImpl  *m_pCounterList;
    CStatsGroupImpl *m_pNextGroup;
}; // CStatsGroupImpl




////////////////////////////////////////////////
// This is the results of all statistics.
class CStatsFileImpl : public CStatsFile {
public:  
    CStatsFileImpl();
    virtual ~CStatsFileImpl();

    // CStatsFile
    virtual CStatsGroupAPI *DeclareGroup(const char *pGroupName);
    virtual void StartNewTest(const char *pTestDataFileName);

    virtual void WriteStats(const char *pStatFileName, int32 fileFormat);

private:
    void FinishCurrentTest();
    ErrVal WriteStatsInTextFileFormat(CSimpleFile *pFile);
    ErrVal WriteStatsInExcelCSVFileFormat(CSimpleFile *pFile);
    void PrintStatsToConsole();

    CStatsGroupImpl    *m_pGroupList;
}; // CStatsFileImpl



#define TEXT_FILE_RECORD_SEPARATOR_STRING   "\r\n\r\n//////////////////////////"
#define TOTALS_SUMMARY_SEPARATOR_STRING     "\r\n\r\n////////////////////////////////////////////////////"
#define NEWLINE_STRING                      "\r\n"

CStatsFile *g_pGlobalPerfStats = NULL;



/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
CStatsFile *
AllocateStatFile() {
    CStatsFileImpl *pNewStatFile = NULL;
    
    pNewStatFile = new CStatsFileImpl;
    if (NULL == pNewStatFile) {
        return(NULL);
    }
    g_pGlobalPerfStats = pNewStatFile;

    return(pNewStatFile);
} // AllocateStatFile





/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
void
FreeStatFile(CStatsFile *pStatFileAPI) {
    CStatsFileImpl *pNewStatFile = (CStatsFileImpl *) pStatFileAPI;

    if (pNewStatFile) {
        if (g_pGlobalPerfStats == pNewStatFile) {
            g_pGlobalPerfStats = NULL;
        }

        delete pNewStatFile;
    }
} // FreeStatFile




/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
CStatsFileImpl::CStatsFileImpl() {
    m_pGroupList = NULL;
} // CStatsFileImpl




/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
CStatsFileImpl::~CStatsFileImpl() {
} // ~CStatsFileImpl





/////////////////////////////////////////////////////////////////////////////
//
// [FinishCurrentTest]
//
/////////////////////////////////////////////////////////////////////////////
void
CStatsFileImpl::FinishCurrentTest() {
} // FinishCurrentTest





/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
void
CStatsFileImpl::StartNewTest(const char *pTestDataFileName) {
    UNUSED_PARAM(pTestDataFileName);

    // If there is a previous test, then save the state to this test.
    FinishCurrentTest();
} // StartNewTest
 






/////////////////////////////////////////////////////////////////////////////
//
// [WriteStats]
//
/////////////////////////////////////////////////////////////////////////////
void
CStatsFileImpl::WriteStats(const char *pStatFileNameStem, int32 fileFormat) {
    ErrVal err = ENoErr;
    CSimpleFile file;
    char *pStatFileName = NULL;

    if (STAT_FILE_FORMAT_CONSOLE == fileFormat) {
        PrintStatsToConsole();
        goto abort;
    }


    if (NULL == pStatFileNameStem) {
        return;
    }

    // If the stats describe all files in a directory, then make the name for a stand-alone file.
    if (CSimpleFile::IsDirectory(pStatFileNameStem)) {
        switch (fileFormat) {
        case STAT_FILE_FORMAT_TEXT:
            pStatFileName = strCatEx(pStatFileNameStem, "/stats.txt");
            break;
        case STAT_FILE_FORMAT_EXCEL:
            pStatFileName = strCatEx(pStatFileNameStem, "/stats.csv");
            break;
        };
    // If the stats describe a single file, then make a name relative to the file name.
    } else {
        switch (fileFormat) {
        case STAT_FILE_FORMAT_TEXT:
            pStatFileName = strCatEx(pStatFileNameStem, ".stats.txt");
            break;
        case STAT_FILE_FORMAT_EXCEL:
            pStatFileName = strCatEx(pStatFileNameStem, ".stats.csv");
            break;
        };
    }
    if (NULL == pStatFileName) {
        gotoErr(EFail);
    }

    // Create a new file to write the stats into.
    err = file.OpenOrCreateFile(pStatFileName, 0);
    if (err) {
        gotoErr(err);
    }

    /////////////////////////////////////////
    switch (fileFormat) {
    case STAT_FILE_FORMAT_TEXT:
        err = WriteStatsInTextFileFormat(&file);
        if (err) {
            gotoErr(err);
        }
        break;
    case STAT_FILE_FORMAT_EXCEL:
        err = WriteStatsInExcelCSVFileFormat(&file);
        if (err) {
            gotoErr(err);
        }
        break;
    };

    err = file.Flush();
    if (err) {
        gotoErr(err);
    }

abort:
    memFree(pStatFileName);
    file.Close();
} // WriteStats




/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////
void
CStatsFileImpl::PrintStatsToConsole() {
    char formatBuffer[4096];
    char *pDestPtr;
    char *pEndDestPtr;
    CStatsGroupImpl *pGroup = NULL;
    CPerfValueImpl *pValue = NULL;

    pDestPtr = formatBuffer;
    pEndDestPtr = formatBuffer + sizeof(formatBuffer) - 1;

    pGroup = m_pGroupList;
    while (pGroup) {
        pDestPtr = formatBuffer;

        //////////////////////////
        pDestPtr += snprintf(
                    pDestPtr,
                    pEndDestPtr - pDestPtr,
                    "%s%sTestFile: %s",
                    TEXT_FILE_RECORD_SEPARATOR_STRING,
                    NEWLINE_STRING,
                    pGroup->m_pName);

        // Write each counter.
        pValue = pGroup->m_pCounterList;
        while (pValue) {
            pDestPtr += snprintf(
                            pDestPtr,
                            pEndDestPtr - pDestPtr,
                            "%s %s=" INT64FMT,
                            NEWLINE_STRING,
                            pValue->m_pName,
                            pValue->m_Value);

            if (COUNTER_TYPE_TIMER == pValue->m_CounterType) {
                pDestPtr += snprintf(pDestPtr, pEndDestPtr - pDestPtr, " ms");
            }

            pValue = pValue->m_pNextValue;        
        } // while (pValue)

        *pDestPtr = 0;
        ::printf(formatBuffer);

        pGroup = pGroup->m_pNextGroup;        
    } // while (pGroup)
} // PrintStatsToConsole








/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
CStatsFileImpl::WriteStatsInTextFileFormat(CSimpleFile *pFile) {
    ErrVal err = ENoErr;
    char formatBuffer[4096];
    char *pDestPtr;
    char *pEndDestPtr;
    int32 bufferLength;
    CStatsGroupImpl *pGroup = NULL;
    CPerfValueImpl *pValue = NULL;


    pDestPtr = formatBuffer;
    pEndDestPtr = formatBuffer + sizeof(formatBuffer) - 1;

    pGroup = m_pGroupList;
    while (pGroup) {
        pDestPtr = formatBuffer;

        //////////////////////////
        pDestPtr += snprintf(
                    pDestPtr,
                    pEndDestPtr - pDestPtr,
                    "%s%sTestFile: %s",
                    TEXT_FILE_RECORD_SEPARATOR_STRING,
                    NEWLINE_STRING,
                    pGroup->m_pName);

        // Write each counter.
        pValue = pGroup->m_pCounterList;
        while (pValue) {
            pDestPtr += snprintf(
                            pDestPtr,
                            pEndDestPtr - pDestPtr,
                            "%s %s=" INT64FMT,
                            NEWLINE_STRING,
                            pValue->m_pName,
                            pValue->m_Value);

            if (COUNTER_TYPE_TIMER == pValue->m_CounterType) {
                pDestPtr += snprintf(pDestPtr, pEndDestPtr - pDestPtr, " ms");
            }

            pValue = pValue->m_pNextValue;        
        } // while (pValue)

        bufferLength = pDestPtr - formatBuffer;
        err = pFile->Write(formatBuffer, bufferLength);
        if (err) {
            gotoErr(err);
        }

        pGroup = pGroup->m_pNextGroup;        
    } // while (pGroup)

abort:
    returnErr(err);
} // WriteStatsInTextFileFormat








/////////////////////////////////////////////////////////////////////////////
//
// [WriteStatsInExcelCSVFileFormat]
//
// Each line describes a single text file. This is a faily long line, which is subdivided
// into several subgroups of values.
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
CStatsFileImpl::WriteStatsInExcelCSVFileFormat(CSimpleFile *pFile) {
    ErrVal err = ENoErr;
    char formatBuffer[4096];
    char *pDestPtr;
    char *pEndDestPtr;
    int32 bufferLength;
    CStatsGroupImpl *pGroup = NULL;
    CPerfValueImpl *pValue = NULL;
    
    pDestPtr = formatBuffer;
    pEndDestPtr = formatBuffer + sizeof(formatBuffer) - 1;
    
    //////////////////////////
    // Write the header row.
    if (m_pGroupList) {
        pDestPtr = formatBuffer;
        pDestPtr += snprintf(
                    pDestPtr,
                    pEndDestPtr - pDestPtr,
                    "%s File",
                    NEWLINE_STRING);

        pValue = m_pGroupList->m_pCounterList;
        while (pValue) {
            pDestPtr += snprintf(
                            pDestPtr,
                            pEndDestPtr - pDestPtr,
                            ", %s",
                            pValue->m_pName);
            if (COUNTER_TYPE_TIMER == pValue->m_CounterType) {
                pDestPtr += snprintf(pDestPtr, pEndDestPtr - pDestPtr, " (ms)");
            }

            pValue = pValue->m_pNextValue;        
        } // while (pValue)

        bufferLength = pDestPtr - formatBuffer;
        err = pFile->Write(formatBuffer, bufferLength);
        if (err) {
            gotoErr(err);
        }
    } // if (m_pGroupList)



    ///////////////////////////////////
    // Write each test.
    pGroup = m_pGroupList;
    while (pGroup) {
        pDestPtr = formatBuffer;

        //////////////////////////
        pDestPtr += snprintf(
                    pDestPtr,
                    pEndDestPtr - pDestPtr,
                    "%s %s",
                    NEWLINE_STRING,
                    pGroup->m_pName);

        // Write each counter.
        pValue = pGroup->m_pCounterList;
        while (pValue) {
            pDestPtr += snprintf(
                            pDestPtr,
                            pEndDestPtr - pDestPtr,
                            ", " INT64FMT,
                            pValue->m_Value);

            pValue = pValue->m_pNextValue;        
        } // while (pValue)

        bufferLength = pDestPtr - formatBuffer;
        err = pFile->Write(formatBuffer, bufferLength);
        if (err) {
            gotoErr(err);
        }

        pGroup = pGroup->m_pNextGroup;        
    } // while (pGroup)

abort:
    returnErr(err);
} // WriteStatsInExcelCSVFileFormat





/////////////////////////////////////////////////////////////////////////////
//
// [DeclareGroup]
//
/////////////////////////////////////////////////////////////////////////////
CStatsGroupAPI *
CStatsFileImpl::DeclareGroup(const char *pGroupName) {
    ErrVal err = ENoErr;
    CStatsGroupImpl *pLastGroup = NULL;
    CStatsGroupImpl *pResult = NULL;

    if (NULL == pGroupName) {
        gotoErr(EFail);
    }

    // Look to see if this group is already in use. If so, then use the currently
    // existing counter. This is slow, but should be called rarely.
    pResult = m_pGroupList;
    while (pResult) {
        if (0 == strcasecmpex(pGroupName, pResult->m_pName)) {
            goto abort;
        }

        pLastGroup = pResult;
        pResult = pResult->m_pNextGroup;        
    } // while (pResult)

    pResult = new CStatsGroupImpl;
    if (NULL == pResult) {
        gotoErr(EFail);
    }

    // Allocate a new counter.
    pResult->m_NameLength = strlen(pGroupName);
    pResult->m_pName = strdupex(pGroupName);
    if (NULL == pResult->m_pName) {
        return(NULL);
    }
    pResult->m_pNextGroup = NULL;
    if (NULL == m_pGroupList) {
        m_pGroupList = pResult;
    }
    if (pLastGroup) {
        pLastGroup->m_pNextGroup = pResult;
    }

abort:
    return(pResult);
} // DeclareGroup





/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
CStatsGroupImpl::CStatsGroupImpl() {
    m_pName = NULL;
    m_NameLength = 0;

    m_pCounterList = NULL;
    
    m_pNextGroup = NULL;
} // CStatsGroupImpl






/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
CStatsGroupImpl::~CStatsGroupImpl() {
    while (m_pCounterList) {
        CPerfValueImpl *pNextValue = m_pCounterList->m_pNextValue;
        delete m_pCounterList;
        m_pCounterList = pNextValue;        
    }
} // ~CStatsGroupImpl






/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
CPerfValueAPI *
CStatsGroupImpl::DeclareMetric(const char *pCounterName, int32 counterType) {
    ErrVal err = ENoErr;
    CPerfValueImpl *pLastValue = NULL;
    CPerfValueImpl *pResult = NULL;

    if (NULL == pCounterName) {
        gotoErr(EFail);
    }

    // Look to see if this metric is already in use. If so, then use the currently
    // existing counter.
    // This is slow, but should be called rarely.
    pResult = m_pCounterList;
    while (pResult) {
        if (0 == strcasecmpex(pCounterName, pResult->m_pName)) {
            if (pResult->m_CounterType != counterType) {
                err = EFail;
                goto abort;
            }
            // Not an error, we are done.
            goto abort;
        }

        pLastValue = pResult;
        pResult = pResult->m_pNextValue;        
    } // while (pResult)

    pResult = new CPerfValueImpl;
    if (NULL == pResult) {
        gotoErr(EFail);
    }

    // Allocate a new counter.
    pResult->m_NameLength = strlen(pCounterName);
    pResult->m_pName = strdupex(pCounterName);
    if (NULL == pResult->m_pName) {
        return(NULL);
    }
    pResult->m_CounterType = counterType;
    pResult->m_Value = 0;
    pResult->m_StartTime = 0;
    pResult->m_pNextValue = NULL;
    if (NULL == m_pCounterList) {
        m_pCounterList = pResult;
    }
    if (pLastValue) {
        pLastValue->m_pNextValue = pResult;
    }

abort:
    return(pResult);
} // DeclareMetric





/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
void
Perf_PrintCurrentDate() {
    CDateTime now;
    char timeStr[256];
    char *pDestPtr;

    now.GetLocalDateAndTime();
    now.PrintToString(timeStr, sizeof(timeStr) - 1, CDateTime::PRINT_SECONDS, &pDestPtr);
    *pDestPtr = 0;
    ::printf(timeStr);
} // Perf_PrintCurrentDate




/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
void
CPerfValueImpl::PrintValue(const char *pPrefixStr) {
    if (pPrefixStr) {
        ::printf(pPrefixStr);
    }
   ::printf(INT64FMT "\n", m_Value);
} // PrintValue

