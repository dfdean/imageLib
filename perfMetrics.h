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
// See the corresponding .cpp file for a description of this module.
/////////////////////////////////////////////////////////////////////////////

#ifndef _PERF_METRICS_H_
#define _PERF_METRICS_H_

#define COUNTER_TYPE_TIMER      0
#define COUNTER_TYPE_COUNTER    1

// The format of the text file
#define STAT_FILE_FORMAT_TEXT       0
#define STAT_FILE_FORMAT_EXCEL      1
#define STAT_FILE_FORMAT_CONSOLE    2


////////////////////////////////////////////////
// This is one counter.
class CPerfValueAPI {
public:
    virtual void SetIntValue(int32 value) = 0;
    virtual void StartTimer() = 0;
    virtual uint64 StopTimer() = 0;
    virtual uint64 GetValue() = 0;
    virtual void PrintValue(const char *pPrefixStr) = 0;
}; // CPerfValueAPI



////////////////////////////////////////////////
class CStatsGroupAPI {
public:
    virtual CPerfValueAPI *DeclareMetric(const char *pCounterName, int32 counterType) = 0;
}; // CStatsGroupAPI



////////////////////////////////////////////////
// A text file that records all statistics.
class CStatsFile {
public:
    virtual CStatsGroupAPI *DeclareGroup(const char *pGroupName) = 0;
    virtual void StartNewTest(const char *pTestDataFileName) = 0;

    virtual void WriteStats(const char *pStatFileName, int32 fileFormat) = 0;
}; // CStatsFile

// The global state.
CStatsFile *AllocateStatFile();
void FreeStatFile(CStatsFile *pStatFileAPI);
extern CStatsFile *g_pGlobalPerfStats;


// These are MACROS, used for timers. For performance they are commented 
// out in a non-debug build. Really, this should still compile in a 
// non-debug build that includes timer information, but that is for 
// another day.
// 
#if DD_DEBUG
#define ProfilerDeclareGroup(timerGroup, pName) if (g_pGlobalPerfStats) { timerGroup = g_pGlobalPerfStats->DeclareGroup(pName); }
#define ProfilerDeclareTimer(timerGroup, pName, timerValue) if (timerGroup) { timerValue = timerGroup->DeclareMetric(pName, COUNTER_TYPE_TIMER); }
#define ProfilerSetValue(timerValue, val) if (timerValue) { timerValue->SetIntValue(val); }
#define ProfilerStartTimer(timerValue) if (timerValue) { timerValue->StartTimer(); }
#define ProfilerStopTimer(timerValue) if (timerValue) { timerValue->StopTimer(); }
#define ProfilerGetValue(timerValue) if (timerValue) { timerValue->GetValue(); } else { 0; }
#define ProfilerPrintValue(timerValue, pPrefixStr) if (timerValue) { timerValue->PrintValue(pPrefixStr); }
#define ProfilerPrintAll(options, pFileName) if (g_pGlobalPerfStats) { WriteStats(pFileName, STAT_FILE_FORMAT_TEXT); }
#else
#define ProfilerDeclareGroup(pName, timerGroup) 
#define ProfilerDeclareTimer(timerGroup, pName, timerValue) 
#define ProfilerSetValue(timerValue, val) 
#define ProfilerStartTimer(timerValue)
#define ProfilerStopTimer(timerValue)
#define ProfilerGetValue(timerValue) 0
#define ProfilerPrintValue(timerValue, pPrefixStr)
#define ProfilerPrintAll(options, pFileName)
#endif // DD_DEBUG


extern void Perf_PrintCurrentDate();




/////////////////////////////////////////////////////////////////////////////
//
//                            AUTO TIMER
//
// This will start and stop a timer when a variable enters and leaves scope.
// It's useful for computing the time spent in a procedure, but can be used for
// any nested scope.
/////////////////////////////////////////////////////////////////////////////
class CAutoTimerImpl {
public:
    /////////////////////////////
    CAutoTimerImpl(CStatsGroupAPI *pProfilerGroup, CPerfValueAPI **ppTimerValue) {
        m_pProfilerGroup = pProfilerGroup;
        m_ppTimerValue = ppTimerValue;
    } // CAutoTimerImpl

    /////////////////////////////
    ~CAutoTimerImpl() {
    } // ~CAutoTimerImpl

private:
    CStatsGroupAPI  *m_pProfilerGroup;
    CPerfValueAPI   **m_ppTimerValue;
}; // CAutoTimerImpl

#if DD_DEBUG
#define ProfilerTimeScope(pProfilerGroup, pName) static CPerfValueAPI *_pAutoTimerValue = NULL; CAutoTimerImpl _autoTimerVar(pProfilerGroup, &_pAutoTimerValue, pName)
#else
#define ProfilerTimeScope(pProfilerGroup, pName)
#endif // DD_DEBUG

#endif // _PERF_METRICS_H_








