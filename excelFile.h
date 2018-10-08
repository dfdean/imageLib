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

#ifndef _EXCEL_FILE_LIB_H_
#define _EXCEL_FILE_LIB_H_



////////////////////////////////////////////////////////////////////////////////
class CExcelFile {
public:
    virtual void Close() = 0;
    virtual ErrVal Save(int32 options) = 0;

    // Build up a table by creating an empty grid and then setting values in any order
    virtual ErrVal InitializeEmptyGrid(int32 numColumns, int32 numRows) = 0;
    virtual ErrVal SetStringCell(int32 column, int32 row, const char *pValue) = 0;
    virtual ErrVal SetFloatCell(int32 column, int32 row, float value) = 0;
    virtual ErrVal SetFloatCellEx(int32 column, int32 row, float value) = 0;
    virtual ErrVal SetIntCell(int32 column, int32 row, int32 value) = 0;
    virtual ErrVal SetUIntCell(int32 column, int32 row, uint32 value) = 0;

    // Build up a table by appending rows and then appending values to the bottom row.
    virtual ErrVal AppendNewRow() = 0;
    virtual ErrVal AppendStringCell(const char *pValue) = 0;
    virtual ErrVal AppendFloatCell(float value) = 0;
    virtual ErrVal AppendFloatCellEx(float value) = 0;
    virtual ErrVal AppendIntCell(int32 value) = 0;

    virtual ErrVal GraphToConsole(int32 columnForX, int32 columnForY) = 0;
}; // CExcelFile

CExcelFile *OpenExcelFile(const char *pFilePath);
CExcelFile *MakeNewExcelFile(const char *pNewFilePath);
void DeleteExcelFileObject(CExcelFile *pParserInterface);





////////////////////////////////////////////////////////////////////////////////
class CHistogram {
public:
    virtual void ClearCounters() = 0;

    virtual void AddSample(uint32 value) = 0;

    virtual ErrVal PrintToExcelRow(const char *pLabel, CExcelFile *pExcel) = 0;
}; // CHistogram

CHistogram *AllocateHistogram(int32 numBuckets, uint32 minValue, uint32 maxValue);
void Delete_Histogram(CHistogram *pHistogram);



#endif // _EXCEL_FILE_LIB_H_






