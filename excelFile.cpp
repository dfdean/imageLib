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
/////////////////////////////////////////////////////////////////////////////

#if WASM
#include "portableBuildingBlocks.h"
#else
#include "buildingBlocks.h"
#endif

#include "excelFile.h"

FILE_DEBUGGING_GLOBALS(LOG_LEVEL_DEFAULT, 0);

static const char *NEWLINE_STRING = "\r\n";



//////////////////////////////////////
class CTableCell {
public:
    CTableCell();
    virtual ~CTableCell();
    NEWEX_IMPL()

    ErrVal SetStrValue(const char *pStr, const char *pEndValue);

public:
    char        *m_pValueStr;
    float       m_FloatValue;

    bool        m_fCopiedValue;

    CTableCell  *m_pNextValue;
}; // CTableCell



//////////////////////////////////////
class CTableRow {
public:
    CTableRow();
    virtual ~CTableRow();
    NEWEX_IMPL()

    ErrVal AppendStrValue(const char *pStr, const char *pEndValue);

    char        *m_pStartText;
    char        *m_pStopText;

    CTableCell  *m_pValueList;
    CTableCell  *m_pLastValue;

    CTableRow   *m_pNextRow;
}; // CTableRow;






///////////////////////////////////////////////////////
class CExcelFileImpl : public CExcelFile {
public:
    NEWEX_IMPL()
    CExcelFileImpl();
    virtual ~CExcelFileImpl();

    ErrVal ReadExistingFile(const char *pFilePath);
    ErrVal InitializeForNewFile(const char *pFilePath);

    /////////////////////////////
    // class CExcelFile
    virtual void Close();
    virtual ErrVal Save(int32 options);

    // Build up a table by creating an empty grid and then setting values in any order
    virtual ErrVal InitializeEmptyGrid(int32 numColumns, int32 numRows);
    virtual ErrVal SetStringCell(int32 column, int32 row, const char *pValue);
    virtual ErrVal SetFloatCell(int32 column, int32 row, float value);
    virtual ErrVal SetFloatCellEx(int32 column, int32 row, float value);
    virtual ErrVal SetIntCell(int32 column, int32 row, int32 value);
    virtual ErrVal SetUIntCell(int32 column, int32 row, uint32 value);

    // Build up a table by appending rows and then appending values to the bottom row.
    virtual ErrVal AppendNewRow();
    virtual ErrVal AppendStringCell(const char *pValue);
    virtual ErrVal AppendFloatCell(float value);
    virtual ErrVal AppendFloatCellEx(float value);
    virtual ErrVal AppendIntCell(int32 value);

    virtual ErrVal GraphToConsole(int32 columnForX, int32 columnForY);

private:
    void DiscardRuntimeRows();

    CTableRow *GetRowByIndex(int32 index);
    CTableCell *GetCellByIndex(CTableRow *pRow, int32 index);

    ErrVal ParseFileIntoRecords();
    void ParseOneRow(CTableRow *pRow);

    // The file. This is optional, and may be NULL if this is a 
    // memory-only object.
    char                *m_pFilePathName;
    CSimpleFile         m_File;

    // The total contents of a file we ParseFileIntoRecordsd.
    char                *m_pFileContents;
    uint64              m_FileLength;

    CTableRow           *m_pRowList;
    CTableRow           *m_pLastRow;
    int32               m_NumRows;
}; // CExcelFileImpl





///////////////////////////////////////////////////////
class CHistogramImpl : public CHistogram {
public:
    CHistogramImpl();
    virtual ~CHistogramImpl();
    ErrVal Initialize(int32 numBuckets, uint32 minValue, uint32 maxValue);
    NEWEX_IMPL()

    // CHistogram 
    virtual void ClearCounters();
    virtual void AddSample(uint32 value);
    virtual ErrVal PrintToExcelRow(const char *pLabel, CExcelFile *pExcel);

private:
    uint32    m_MinValue;
    uint32    m_MaxValue;

    int32     m_NumBuckets;
    uint32    m_ValueRangePerBucket;
    int32     *m_pValueCounts;
}; // CHistogram





////////////////////////////////////////////////////////////////////////////////
//                              GRAPHING
////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////
class CGraphEntry {
public:
    int32           m_Value;
}; // CGraphEntry


///////////////////////////////////////////////////////
class CGraphBitMap {
public:
    CGraphBitMap();
    virtual ~CGraphBitMap();

    void SetValue(int32 x, int32 y, int32 value);
    void DrawToConsole();

    int32           m_MaxXPos;
    int32           m_MaxYPos;
    CGraphEntry     *m_pValueTable;
}; // CGraphBitMap

ErrVal AllocateGraph(int32 maxX, int32 maxY, CGraphBitMap **ppResult);









/////////////////////////////////////////////////////////////////////////////
//
// [OpenExcelFile]
//
/////////////////////////////////////////////////////////////////////////////
CExcelFile *
OpenExcelFile(const char *pFilePath) {
    ErrVal err = ENoErr;
    CExcelFileImpl *pExcelFileImpl = NULL;

    if (NULL == pFilePath) {
        gotoErr(EFail);
    }

    pExcelFileImpl = newex CExcelFileImpl;
    if (NULL == pExcelFileImpl) {
        gotoErr(EFail);
    }

    err = pExcelFileImpl->ReadExistingFile(pFilePath);
    if (err) {
        gotoErr(err);
    }

    return(pExcelFileImpl);

abort:
    delete pExcelFileImpl;
    return(NULL);
} // OpenExcelFile






/////////////////////////////////////////////////////////////////////////////
//
// [MakeNewExcelFile]
//
/////////////////////////////////////////////////////////////////////////////
CExcelFile *
MakeNewExcelFile(const char *pNewFilePath) {
    ErrVal err = ENoErr;
    CExcelFileImpl *pExcelFileImpl = NULL;

    pExcelFileImpl = newex CExcelFileImpl;
    if (NULL == pExcelFileImpl) {
        gotoErr(EFail);
    }

    err = pExcelFileImpl->InitializeForNewFile(pNewFilePath);
    if (err) {
        gotoErr(err);
    }

    return(pExcelFileImpl);

abort:
    delete pExcelFileImpl;
    return(NULL);
} // MakeNewExcelFile






/////////////////////////////////////////////////////////////////////////////
//
// [DeleteExcelFileObject]
//
/////////////////////////////////////////////////////////////////////////////
void
DeleteExcelFileObject(CExcelFile *pExcelFileAPI) {
    CExcelFileImpl *pExcelFileImpl = (CExcelFileImpl *) pExcelFileAPI;

    if (pExcelFileImpl) {
        pExcelFileImpl->Close();
        delete pExcelFileImpl;
    }
} // DeleteExcelFileObject





/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
CExcelFileImpl::CExcelFileImpl() {
    m_pFilePathName = NULL;

    m_pFileContents = NULL;
    m_FileLength = 0;

    m_pRowList = NULL;
    m_pLastRow = NULL;
    m_NumRows = 0;
} // CExcelFileImpl





/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
CExcelFileImpl::~CExcelFileImpl() {
    Close();
}




/////////////////////////////////////////////////////////////////////////////
//
// [ReadExistingFile]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
CExcelFileImpl::ReadExistingFile(const char *pFilePath) {
    ErrVal err = ENoErr;
    int32 numBytesRead;

    if (NULL == pFilePath) {
        gotoErr(EFail);
    }
    Close();

    err = m_File.OpenExistingFile(pFilePath, 0);
    if (err) {
        gotoErr(err);
    }
    
    m_pFilePathName = strdupex(pFilePath);
    if (NULL == m_pFilePathName) {
        gotoErr(EFail);
    }

    err = m_File.GetFileLength(&m_FileLength);
    if (err) {
        gotoErr(err);
    }
    m_pFileContents = (char *) memAlloc((int32) (m_FileLength + 1));
    if (NULL == m_pFileContents) {
        gotoErr(EFail);
    }

    err = m_File.Seek(0, CSimpleFile::SEEK_START);
    if (err) {
        gotoErr(err);
    }
    err = m_File.Read(m_pFileContents, (int32) m_FileLength, &numBytesRead);
    if (err) {
        gotoErr(err);
    }
    m_pFileContents[m_FileLength] = 0;

    err = ParseFileIntoRecords();
    if (err) {
        gotoErr(err);
    }

abort:
    returnErr(err);
} // ReadExistingFile






/////////////////////////////////////////////////////////////////////////////
//
// [InitializeForNewFile]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
CExcelFileImpl::InitializeForNewFile(const char *pFilePath) {
    ErrVal err = ENoErr;

    Close();

    if (pFilePath) {
        CSimpleFile::DeleteFile(pFilePath);
        err = m_File.OpenOrCreateEmptyFile(pFilePath, 0);
        if (err) {
            gotoErr(err);
        }

        // Save a copy of the file name so we can reopen it and change it later.
        m_pFilePathName = strdupex(pFilePath);
        if (NULL == m_pFilePathName) {
            gotoErr(EFail);
        }
    } // if (pFilePath)

abort:
    returnErr(err);
} // InitializeForNewFile






/////////////////////////////////////////////////////////////////////////////
//
// [Close]
//
/////////////////////////////////////////////////////////////////////////////
void
CExcelFileImpl::Close() {
    DiscardRuntimeRows();

    memFree(m_pFilePathName);
    m_pFilePathName = NULL;
    m_File.Close();
} // Close






/////////////////////////////////////////////////////////////////////////////
//
// [DiscardRuntimeRows]
//
/////////////////////////////////////////////////////////////////////////////
void
CExcelFileImpl::DiscardRuntimeRows() {
    while (m_pRowList) {
        CTableRow *pNextRow = m_pRowList->m_pNextRow;
        delete m_pRowList;
        m_pRowList = pNextRow;
    }
    m_pLastRow = NULL;
    m_NumRows = 0;

    memFree(m_pFileContents);
    m_pFileContents = NULL;
    m_FileLength = 0;
} // DiscardRuntimeRows





/////////////////////////////////////////////////////////////////////////////
//
// [Save]
//
// Write the file buffer, as it is in memory, straight to the file.
// Any changes we make to the file are done directly to the memory-resident
// image, so we don't have to translate between memory-resident data structures
// and the file image.
/////////////////////////////////////////////////////////////////////////////
ErrVal
CExcelFileImpl::Save(int32 options) {
    ErrVal err = ENoErr;
    char formatBuffer[32000];
    CTableRow *pRow;
    CTableCell *pCell;
    char *pDestPtr;
    char *pEndDestPtr;
    int32 bufferLength;
    int32 numLinesWritten;
    UNUSED_PARAM(options);

    err = m_File.SetFileLength(0);
    if (err) {
        gotoErr(EFail);
    }
    
    pRow = m_pRowList;
    numLinesWritten = 0;
    while (pRow) {
        pDestPtr = formatBuffer;
        pEndDestPtr = formatBuffer + sizeof(formatBuffer) - 1;

        pCell = pRow->m_pValueList;
        while (pCell) {
            if (pCell != pRow->m_pValueList) {
                pDestPtr += snprintf(pDestPtr, pEndDestPtr - pDestPtr, ", ");
            }
            pDestPtr += snprintf(pDestPtr, pEndDestPtr - pDestPtr, pCell->m_pValueStr);
            pCell = pCell->m_pNextValue;
        } // while (pCell)

        pDestPtr += snprintf(pDestPtr, pEndDestPtr - pDestPtr, NEWLINE_STRING);
        bufferLength = pDestPtr - formatBuffer;
        err = m_File.Write(formatBuffer, bufferLength);
        if (err) {
            gotoErr(err);
        } 

        pRow = pRow->m_pNextRow;
        numLinesWritten += 1;
    } // while (pRow)

    err = m_File.Flush();
    if (err) {
        gotoErr(err);
    }

abort:
    returnErr(err);
} // Save






/////////////////////////////////////////////////////////////////////////////
//
// [ParseFileIntoRecords]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
CExcelFileImpl::ParseFileIntoRecords() {
    ErrVal err = ENoErr;
    char *pPtr;
    char *pEndFile;
    char *pCurrentLine;
    char *pEndCurrentLine;

    if ((NULL == m_pFileContents) || (m_FileLength <= 0)) {
        gotoErr(EFail);
    }

    m_pRowList = NULL;
    m_pLastRow = NULL;
    m_NumRows = 0;
    pEndFile = m_pFileContents + m_FileLength;
    

    // Each iteration processes one line.
    pPtr = m_pFileContents;
    pCurrentLine = m_pFileContents;
    while (pPtr < pEndFile) {
        // Check if this ends the current line.
        if (('\n' == *pPtr) || ('\r' == *pPtr)) {
            pEndCurrentLine = pPtr;

            // Now, consume all \r\n or \n\r sequences that end a single line.
            // Be careful, there may be several lines.
            *pPtr = 0;
            pPtr += 1;
            while ((pPtr < pEndFile) && (('\n' == *pPtr) || ('\r' == *pPtr))) {
                pPtr += 1;
            } // Hit a line end.

            // Allocate a record structure for this line.
            err = AppendNewRow();
            if (err) {
                gotoErr(EFail);
            }
            m_pLastRow->m_pStartText = pCurrentLine;
            m_pLastRow->m_pStopText = pEndCurrentLine;
            m_NumRows += 1;

            // Process 1 line.
            ParseOneRow(m_pLastRow);

            pCurrentLine = pPtr;
        } // Hit a line end.
        else {
            // This was a normal character.
            pPtr += 1;
        }
    } // while (pPtr < pEndFile)

abort:
    returnErr(err);
} // ParseFileIntoRecords








////////////////////////////////////////////////////////////////////////////////
//
// [ParseOneRow]
//
////////////////////////////////////////////////////////////////////////////////
void
CExcelFileImpl::ParseOneRow(CTableRow *pRow) {
    char *pPtr;
    char *pCurrentField;
    CTableCell *pNewCell = NULL;

    if (NULL == pRow) {
        return;
    }
    pRow->m_pLastValue = NULL;

    // Each iteration processes one char.
    pPtr = pRow->m_pStartText;
    pCurrentField = pPtr;
    while (*pPtr) {
        ////////////////////////////////////////
        // Check if this starts a quoted string. This is important, because a quoted string
        // may contain commas that are not field separators.
        if (('\"' == *pPtr) || ('\'' == *pPtr)) {
            char startQuoteChar = *pPtr;
            pPtr += 1;
            while ((*pPtr) && (startQuoteChar != *pPtr)) {
                pPtr += 1;
            }
            if (startQuoteChar == *pPtr) {
                pPtr += 1;
            }
        ////////////////////////////////////////
        // Check if this ends the current field.
        } else if (('\n' == *pPtr) || ('\r' == *pPtr) || (0 == *pPtr) || (',' == *pPtr)) {
            pNewCell = newex CTableCell;
            if (NULL == pNewCell) {
                goto abort;
            }
            pNewCell->m_pValueStr = pCurrentField;
            pNewCell->m_fCopiedValue = false;
            pNewCell->m_pNextValue = NULL;

            if (NULL == pRow->m_pValueList) {
                pRow->m_pValueList = pNewCell;
            }
            if (pRow->m_pLastValue) {
                pRow->m_pLastValue->m_pNextValue = pNewCell;
            }
            pRow->m_pLastValue = pNewCell;
            
            if (0 == *pPtr) {
                break;
            } else if (',' == *pPtr) {
                *pPtr = 0;
                pPtr += 1;
            }

            pCurrentField = pPtr;
        ////////////////////////////////////////
        } else {
            // This was a normal character.
            pPtr += 1;
        }
    } // while (*pPtr)

    // Record the last field in the line.
    pNewCell = newex CTableCell;
    if (NULL == pNewCell) {
        goto abort;
    }
    pNewCell->m_pValueStr = pCurrentField;
    pNewCell->m_fCopiedValue = false;
    pNewCell->m_pNextValue = NULL;

    if (NULL == pRow->m_pValueList) {
        pRow->m_pValueList = pNewCell;
    }
    if (pRow->m_pLastValue) {
        pRow->m_pLastValue->m_pNextValue = pNewCell;
    }
    pRow->m_pLastValue = pNewCell;

abort:
    return;
} // ParseOneRow






/////////////////////////////////////////////////////////////////////////////
//
// [InitializeEmptyGrid]
//
// Build up a table by creating an empty grid and then setting values in any order
/////////////////////////////////////////////////////////////////////////////
ErrVal
CExcelFileImpl::InitializeEmptyGrid(int32 numColumns, int32 numRows) {
    ErrVal err = ENoErr;
    CTableCell *pNewCell = NULL;
    CTableRow *pRow = NULL;
    int32 rowNum;
    int32 colNum;

    DiscardRuntimeRows();

    for (rowNum = 0; rowNum < numRows; rowNum++) {
        // Allocate a record structure for this line.
        pRow = newex CTableRow;
        if (NULL == pRow) {
            gotoErr(EFail);
        }

        // Initialize the new row with the correct number of columns.
        for (colNum = 0; colNum < numColumns; colNum++) {
            pNewCell = newex CTableCell;
            if (NULL == pNewCell) {
                gotoErr(EFail);
            }

            pNewCell->m_pValueStr = NULL;
            pNewCell->m_fCopiedValue = true;
            pNewCell->m_pNextValue = NULL;

            if (NULL == pRow->m_pValueList) {
                pRow->m_pValueList = pNewCell;
            }
            if (pRow->m_pLastValue) {
                pRow->m_pLastValue->m_pNextValue = pNewCell;
            }
            pRow->m_pLastValue = pNewCell;
            pNewCell = NULL;
        } // for (colNum = 0; colNum < numColumns; colNum++)

        pRow->m_pNextRow = NULL;
        if (m_pLastRow) {
            m_pLastRow->m_pNextRow = pRow;
        } else {
            m_pRowList = pRow;
        }
        m_pLastRow = pRow;
        m_NumRows += 1;
        pRow = NULL;
    } // for (rowNum = 0; rowNum < numRows; rowNum++)

abort:
    delete pRow;
    delete pNewCell;

    returnErr(err);
} // InitializeEmptyGrid







/////////////////////////////////////////////////////////////////////////////
//
// [GetRowByIndex]
//
// Build up a table by creating an empty grid and then setting values in any order
/////////////////////////////////////////////////////////////////////////////
CTableRow *
CExcelFileImpl::GetRowByIndex(int32 index) {
    CTableRow *pRow = NULL;
    int32 rowNum;

    pRow = m_pRowList;
    rowNum = 0;
    while ((pRow) && (rowNum < index)) {
        pRow = pRow->m_pNextRow;
        rowNum += 1;
    }

    return(pRow);
} // GetRowByIndex






/////////////////////////////////////////////////////////////////////////////
//
// [GetCellByIndex]
//
/////////////////////////////////////////////////////////////////////////////
CTableCell *
CExcelFileImpl::GetCellByIndex(CTableRow *pRow, int32 index) {
    CTableCell *pCell = NULL;
    int32 colNum;

    if (NULL == pRow) {
        return(NULL);
    }

    pCell = pRow->m_pValueList;
    colNum = 0;
    while ((pCell) && (colNum < index)) {
        pCell = pCell->m_pNextValue;
        colNum += 1;
    }

    return(pCell);
} // GetCellByIndex



/////////////////////////////////////////////////////////////////////////////
//
// [SetStringCell]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
CExcelFileImpl::SetStringCell(int32 column, int32 row, const char *pValue) {
    ErrVal err = ENoErr;
    const char *pEndValue = NULL;
    CTableRow *pRow;
    CTableCell *pCell;

    if (NULL == pValue) {
        gotoErr(EFail);
    }
    pEndValue = pValue + strlen(pValue);

    pRow = GetRowByIndex(row);
    if (NULL == pRow) {
        gotoErr(EFail);
    }
    pCell = GetCellByIndex(pRow, column);
    if (NULL == pCell) {
        gotoErr(EFail);
    }

    err = pCell->SetStrValue(pValue, pEndValue);
    if (err) {
        gotoErr(err);
    }

abort:
    returnErr(err);
} // SetStringCell



/////////////////////////////////////////////////////////////////////////////
//
// [SetFloatCell]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
CExcelFileImpl::SetFloatCell(int32 column, int32 row, float value) {
    ErrVal err = ENoErr;
    char valueBuffer[64];

    ::snprintf(valueBuffer, (int32) (sizeof(valueBuffer)), "%.1f", value);
    err = SetStringCell(column, row, valueBuffer);
    returnErr(err);
} // SetFloatCell






/////////////////////////////////////////////////////////////////////////////
//
// [SetFloatCellEx]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
CExcelFileImpl::SetFloatCellEx(int32 column, int32 row, float value) {
    ErrVal err = ENoErr;
    char valueBuffer[64];

    ::snprintf(valueBuffer, (int32) (sizeof(valueBuffer)), "%.3f", value);
    err = SetStringCell(column, row, valueBuffer);
    returnErr(err);
} // SetFloatCellEx







/////////////////////////////////////////////////////////////////////////////
//
// [SetIntCell]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
CExcelFileImpl::SetIntCell(int32 column, int32 row, int32 value) {
    ErrVal err = ENoErr;
    char valueBuffer[64];

    ::snprintf(valueBuffer, (int32) (sizeof(valueBuffer)), "%d", value);
    err = SetStringCell(column, row, valueBuffer);
    returnErr(err);
} // SetIntCell






/////////////////////////////////////////////////////////////////////////////
//
// [SetUIntCell]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
CExcelFileImpl::SetUIntCell(int32 column, int32 row, uint32 value) {
    ErrVal err = ENoErr;
    char valueBuffer[64];

    ::snprintf(valueBuffer, (int32) (sizeof(valueBuffer)), "%d", value);
    err = SetStringCell(column, row, valueBuffer);
    returnErr(err);
} // SetUIntCell




/////////////////////////////////////////////////////////////////////////////
//
// [AppendNewRow]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
CExcelFileImpl::AppendNewRow() {
    ErrVal err = ENoErr;
    CTableRow *pRow = NULL;

    // Allocate a record structure for this line.
    pRow = newex CTableRow;
    if (NULL == pRow) {
        gotoErr(EFail);
    }

    pRow->m_pNextRow = NULL;
    if (m_pLastRow) {
        m_pLastRow->m_pNextRow = pRow;
    } else {
        m_pRowList = pRow;
    }
    m_pLastRow = pRow;
    m_NumRows += 1;

abort:
    returnErr(err);
} // AppendNewRow





/////////////////////////////////////////////////////////////////////////////
//
// [AppendStringCell]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
CExcelFileImpl::AppendStringCell(const char *pValue) {
    ErrVal err = ENoErr;
    const char *pEndValue = NULL;

    if (NULL == pValue) {
        gotoErr(EFail);
    }
    pEndValue = pValue + strlen(pValue);

    if (NULL == m_pLastRow) {
        gotoErr(EFail);
    }

    err = m_pLastRow->AppendStrValue(pValue, pEndValue);
    if (err) {
        gotoErr(err);
    }

abort:
    returnErr(err);
} // AppendStringCell





/////////////////////////////////////////////////////////////////////////////
//
// [AppendFloatCell]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
CExcelFileImpl::AppendFloatCell(float value) {
    ErrVal err = ENoErr;
    char valueBuffer[64];

    ::snprintf(valueBuffer, (int32) (sizeof(valueBuffer)), "%.1f", value);
    err = AppendStringCell(valueBuffer);
    returnErr(err);
} // AppendFloatCell





/////////////////////////////////////////////////////////////////////////////
//
// [AppendFloatCellEx]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
CExcelFileImpl::AppendFloatCellEx(float value) {
    ErrVal err = ENoErr;
    char valueBuffer[64];

    ::snprintf(valueBuffer, (int32) (sizeof(valueBuffer)), "%.3f", value);
    err = AppendStringCell(valueBuffer);
    returnErr(err);
} // AppendFloatCellEx







/////////////////////////////////////////////////////////////////////////////
//
// [AppendIntCell]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
CExcelFileImpl::AppendIntCell(int32 value) {
    ErrVal err = ENoErr;
    char valueBuffer[64];

    ::snprintf(valueBuffer, (int32) (sizeof(valueBuffer)), "%d", value);
    err = AppendStringCell(valueBuffer);
    returnErr(err);
} // AppendIntCell




/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
CTableCell::CTableCell() {
    m_pValueStr = NULL;
    m_fCopiedValue = false;

    m_pNextValue = NULL;
}


/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
CTableCell::~CTableCell() {
    if ((m_pValueStr) && (m_fCopiedValue)) {
        memFree(m_pValueStr);
    }
}



/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
ErrVal
CTableCell::SetStrValue(const char *pStr, const char *pEndValue) {
    ErrVal err = ENoErr;
    const char *pLastChar;
    int32 length;

    // Discard any old value.
    if ((m_pValueStr) && (m_fCopiedValue)) {
        memFree(m_pValueStr);
        m_pValueStr = NULL;
    }

    // Trim off any whitespace.
    while ((*pStr) && (pStr < pEndValue) 
            && ((' ' == *pStr) || ('\n' == *pStr) || ('\r' == *pStr) || ('\t' == *pStr))) {
        pStr += 1;
    }
    pLastChar = pEndValue - 1;
    while ((*pLastChar) && (pStr <= pLastChar) 
            && ((' ' == *pLastChar) || ('\n' == *pLastChar) || ('\r' == *pLastChar) || ('\t' == *pLastChar))) {
        pLastChar = pLastChar - 1;
    }
    pEndValue = pLastChar + 1;

    length = (int32) (pEndValue - pStr);
    m_pValueStr = (char *) memAlloc(length + 1);
    if (NULL == m_pValueStr) {
        gotoErr(EFail);
    }

    memcpy(m_pValueStr, pStr, length);
    m_pValueStr[length] = 0;
    m_fCopiedValue = true;

abort:
    returnErr(err); 
} // SetStrValue





/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
CTableRow::CTableRow() {
    m_pStartText = NULL;
    m_pStopText = NULL;
    m_pValueList = NULL;
    m_pLastValue = NULL;

    m_pNextRow = NULL;
}




/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
CTableRow::~CTableRow() {
    while (m_pValueList) {
        CTableCell *pNextCell = m_pValueList->m_pNextValue;
        delete m_pValueList;
        m_pValueList = pNextCell;
    }
}




/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
ErrVal
CTableRow::AppendStrValue(const char *pStr, const char *pEndValue) {
    ErrVal err = ENoErr;
    CTableCell *pNewCell = NULL;

    pNewCell = newex CTableCell;
    if (NULL == pNewCell) {
        gotoErr(EFail);
    }

    err = pNewCell->SetStrValue(pStr, pEndValue);
    if (err) {
        gotoErr(err);
    }

    pNewCell->m_pNextValue = NULL;
    if (NULL == m_pValueList) {
        m_pValueList = pNewCell;
    }
    if (m_pLastValue) {
        m_pLastValue->m_pNextValue = pNewCell;
    }
    m_pLastValue = pNewCell;
    pNewCell = NULL;

abort:
    delete pNewCell;
    returnErr(err); 
} // AppendStrValue







/////////////////////////////////////////////////////////////////////////////
//
// [AllocateGraph]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
AllocateGraph(int32 maxX, int32 maxY, CGraphBitMap **ppResult) {
    ErrVal err = ENoErr;
    CGraphBitMap *pGraph = NULL;
    CGraphEntry *pEntry;
    int32 x;
    int32 y;   
    int32 totalEntries;

    if (NULL == ppResult) {
        gotoErr(EFail);
    }
    *ppResult = NULL;

    pGraph = new CGraphBitMap;
    if (NULL == pGraph) {
        gotoErr(EFail);
    }

    pGraph->m_MaxXPos = maxX;
    pGraph->m_MaxYPos = maxY;
    totalEntries = pGraph->m_MaxXPos * pGraph->m_MaxYPos;
    pGraph->m_pValueTable = (CGraphEntry *) memAlloc(sizeof(CGraphEntry) * totalEntries);
    if (NULL == pGraph->m_pValueTable) {
        gotoErr(EFail);
    }

    // Build up a grayscale map of the image.
    // Do this so we only have to compute the grayscale of each pixel once.
    for (x = 0; x < pGraph->m_MaxXPos; x++) {
        for (y = 0; y < pGraph->m_MaxYPos; y++) {
            pEntry = &((pGraph->m_pValueTable)[(y * pGraph->m_MaxXPos) + x]);
            pEntry->m_Value = 0;
        } // for (y = 0; y < pGraph->m_MaxYPos; y++)
    } // for (x = 0; x < m_MaxXPos; x++)

    *ppResult = pGraph;
    pGraph = NULL;

abort:
    delete pGraph;
    returnErr(err);
} // AllocateGraph





/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
CGraphBitMap::CGraphBitMap() {
    m_MaxXPos = 0;
    m_MaxYPos = 0;
    m_pValueTable = NULL;
} // CGraphBitMap




/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
CGraphBitMap::~CGraphBitMap() {
    memFree(m_pValueTable);
} // ~CGraphBitMap




/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
void
CGraphBitMap::SetValue(int32 x, int32 y, int32 value) {
    CGraphEntry *pEntry;

    if (x < 0) {
        x = 0;
    }
    if (y < 0) {
        y = 0;
    }
    if (x >= m_MaxXPos) {
        x = m_MaxXPos - 1;
    }
    if (y >= m_MaxYPos) {
        y = m_MaxYPos - 1;
    }

    pEntry = &((m_pValueTable)[(y * m_MaxXPos) + x]);
    pEntry->m_Value = value;
 } // SetValue





/////////////////////////////////////////////////////////////////////////////
//
// [DrawToConsole]
//
/////////////////////////////////////////////////////////////////////////////
void
CGraphBitMap::DrawToConsole() {
    CGraphEntry *pEntry;
    int32 x;
    int32 y;   

    // Draw each row of the graph.
    for (y = m_MaxYPos - 1; y >= 0; y = y - 1) {
        // This is the Y-axis.
        printf("\n%3d |", y);

        // This is each x value for this Y.
        for (x = 0; x < m_MaxXPos; x++) {
            pEntry = &((m_pValueTable)[(y * m_MaxXPos) + x]);
            if (pEntry->m_Value) {
                printf("x");
            } else {
                printf(" ");
            }
        } // for (x = 0; x < m_MaxXPos; x++)
    } // for (y = m_MaxYPos; y >= 0; y = y - 1)


    // Draw the X-axis.
    printf("\n    ");
    for (x = 0; x < m_MaxXPos; x++) {
        printf("_");
    } // for (x = 0; x < m_MaxXPos; x++)
    printf("\n");
} // DrawToConsole





/////////////////////////////////////////////////////////////////////////////
//
// [GraphToConsole]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
CExcelFileImpl::GraphToConsole(int32 columnForX, int32 columnForY) {
    ErrVal err = ENoErr;
    CGraphBitMap *pGraph = NULL;
    CTableRow *pRow;
    CTableCell *pCell;
    CTableCell *pXCell;
    CTableCell *pYCell;
    int32 maxX;
    int32 maxY;
    int32 columnNumber;
    int32 scaledX;
    int32 scaledY;
    float xValueFloat;
    float yValueFloat;

    maxX = 90;
    maxY = 70;

    err = AllocateGraph(maxX, maxY, &pGraph);
    if (err) {
        gotoErr(err);
    }

    // Each row is a tuple of values. Examine each tuple for a new (x,y)
    // value pair to graph.
    pRow = m_pRowList;

    // Skip the header row.
    if (pRow) {
        pRow = pRow->m_pNextRow;
    }

    while (pRow) {
        // Look for the X and Y values.
        pXCell = NULL;
        pYCell = NULL;

        columnNumber = 0;
        pCell = pRow->m_pValueList;
        while (pCell) {
            if (columnNumber == columnForX) {
                pXCell = pCell;
            }
            if (columnNumber == columnForY) {
                pYCell = pCell;
            }
            // Stop looking when we have found both the X and Y values.
            if ((pXCell) && (pYCell)) {
                break;
            }
            pCell = pCell->m_pNextValue;
            columnNumber += 1;
        } // while (pCell)

        if ((pXCell) && (pYCell)) {
            ::sscanf(pXCell->m_pValueStr, "%f", &xValueFloat);
            ::sscanf(pYCell->m_pValueStr, "%f", &yValueFloat);

            scaledX = (int32) (xValueFloat);
            scaledY = (int32) (yValueFloat);

            printf("scaledX=%d, scaledY=%d\n", scaledX, scaledY);

            pGraph->SetValue(scaledX, scaledY, 1);
        }

        // Go to the next row.
        pRow = pRow->m_pNextRow;
    } // while (pRow)


    pGraph->DrawToConsole();

abort:
    delete pGraph;
    returnErr(err);
} // GraphToConsole








#if 0
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
    int32 index;
    char formatBuffer[4096];
    char *pDestPtr;
    char *pEndDestPtr;
    int32 bufferLength;
    CTestResultGroup *pTestInfo;
    CPerfMetric *pFileCounter = NULL;
    
    pDestPtr = formatBuffer;
    pEndDestPtr = formatBuffer + sizeof(formatBuffer) - 1;
    
    //////////////////////////
    // Write the header row.
    pTestInfo = g_pTestList;
    if (pTestInfo) {
        pDestPtr = formatBuffer;

        // Write each counter header.
        for (index = 0; index < g_NumDeclaredPerfMetrics; index++) {
            pFileCounter = &(pTestInfo->m_ValueList[index]);

            pDestPtr += snprintf(
                            pDestPtr,
                            pEndDestPtr - pDestPtr,
                            "%s",
                            pFileCounter->m_pName);
            if ((index + 1) < MAX_PERF_METRICS) {
                pDestPtr += snprintf(pDestPtr, pEndDestPtr - pDestPtr, ", ");
            }
        } // for (index = 0; index < g_NumDeclaredPerfMetrics; index++)
    } // if (pTestInfo)

    bufferLength = pDestPtr - formatBuffer;
    err = pFile->Write(formatBuffer, bufferLength);
    if (err) {
        gotoErr(err);
    } 

    //////////////////////////
    // Write one row of values for each test.
    pTestInfo = g_pTestList;
    while (pTestInfo) {
        pDestPtr = formatBuffer;
        pDestPtr += snprintf(
                        pDestPtr,
                        pEndDestPtr - pDestPtr,
                        "%s",
                        NEWLINE_STRING);

        // Write each counter value.
        for (index = 0; index < g_NumDeclaredPerfMetrics; index++) {
            pFileCounter = &(pTestInfo->m_ValueList[index]);

            pDestPtr += snprintf(
                            pDestPtr,
                            pEndDestPtr - pDestPtr,
                            INT64FMT,
                            pFileCounter->m_Value);
            if ((index + 1) < MAX_PERF_METRICS) {
                pDestPtr += snprintf(pDestPtr, pEndDestPtr - pDestPtr, ", ");
            }
        } // for (index = 0; index < g_NumDeclaredPerfMetrics; index++)
    } // for (index = 0; index < g_NumDeclaredPerfMetrics; index++)

    bufferLength = pDestPtr - formatBuffer;
    err = pFile->Write(formatBuffer, bufferLength);
    if (err) {
        gotoErr(err);
    } 

abort:
    returnErr(err);
} // WriteStatsInExcelCSVFileFormat

#endif













/////////////////////////////////////////////////////////////////////////////
//
// [AllocateHistogram]
//
/////////////////////////////////////////////////////////////////////////////
CHistogram *
AllocateHistogram(int32 numBuckets, uint32 minValue, uint32 maxValue) {
    ErrVal err = ENoErr;
    CHistogramImpl *pHistogramImpl = NULL;

    pHistogramImpl = newex CHistogramImpl;
    if (NULL == pHistogramImpl) {
        gotoErr(EFail);
    }

    err = pHistogramImpl->Initialize(numBuckets, minValue, maxValue);
    if (err) {
        gotoErr(err);
    }

    return(pHistogramImpl);

abort:
    delete pHistogramImpl;
    return(NULL);
} // AllocateHistogram





/////////////////////////////////////////////////////////////////////////////
//
// [DeleteHistogram]
//
/////////////////////////////////////////////////////////////////////////////
void
DeleteHistogram(CHistogram *pHistogram) {
    CHistogramImpl *pHistogramImpl = (CHistogramImpl *) pHistogram;

    if (pHistogramImpl) {
        delete pHistogramImpl;
    }
} // DeleteHistogram






////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
CHistogramImpl::CHistogramImpl() {
    m_MinValue = 0;
    m_MaxValue = 0;

    m_NumBuckets = 0;
    m_ValueRangePerBucket = 0;
    m_pValueCounts = NULL;
} // CHistogramImpl




////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
CHistogramImpl::~CHistogramImpl() {
    memFree(m_pValueCounts);
} // ~CHistogramImpl




////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
ErrVal 
CHistogramImpl::Initialize(int32 numBuckets, uint32 minValue, uint32 maxValue) {
    ErrVal err = ENoErr;

    m_MinValue = minValue;
    m_MaxValue = maxValue;

    m_NumBuckets = numBuckets;
    m_ValueRangePerBucket = (maxValue - minValue) / numBuckets;

    m_pValueCounts = (int32 *) memCalloc(sizeof(int32) * m_NumBuckets);
    if (NULL == m_pValueCounts) {
        gotoErr(EFail);
    }

abort:
    returnErr(err);
} // Initialize




////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void
CHistogramImpl::ClearCounters() {
    int32 bucketNum;

    for (bucketNum = 0; bucketNum < m_NumBuckets; bucketNum++) {
        m_pValueCounts[bucketNum] = 0;
    }
} // ClearCounters





////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void
CHistogramImpl::AddSample(uint32 value) {
    uint32 bucketNum = value / m_ValueRangePerBucket;
    m_pValueCounts[bucketNum] = m_pValueCounts[bucketNum] + 1;
} // AddSample





////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
ErrVal 
CHistogramImpl::PrintToExcelRow(const char *pLabel, CExcelFile *pExcelFile) {
    ErrVal err = ENoErr;
    int32 bucketNum;

    if (NULL == pExcelFile) {
        gotoErr(EFail);
    }

    pExcelFile->AppendNewRow();
    if (pLabel) {
        pExcelFile->AppendStringCell(pLabel);
    }

    for (bucketNum = 0; bucketNum < m_NumBuckets; bucketNum++) {
        err = pExcelFile->AppendIntCell(m_pValueCounts[bucketNum]);
        if (err) {
            gotoErr(err);
        }
    }

abort:
    returnErr(err);
} // Initialize


