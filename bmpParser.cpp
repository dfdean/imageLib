////////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2010-2017 Dawson Dean
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
// BMP Parser
//
// For a useful description, see http://en.wikipedia.org/wiki/BMP_file_format
//
// (0,0) is the top-left corner. This is a relic from CRT-tubes, and is widely 
// used in computer file formats and graphics tools.
/////////////////////////////////////////////////////////////////////////////

#if WASM
#include "portableBuildingBlocks.h"
#else
#include "buildingBlocks.h"
#endif

#include "imageLib.h"
#include "imageLibInternal.h"

FILE_DEBUGGING_GLOBALS(LOG_LEVEL_DEFAULT, 0);

#define LITTLE_ENDIAN_NUMBERS 1

typedef struct {
  unsigned char magic[2];
} CBMPImageFileSignature;
 

typedef struct {
  uint32    filesz;
  uint16    creator1;
  uint16    creator2;
  uint32    bmpOffset;
} CBMPImageFileHeader;


typedef struct {
  uint32    headerSize;
  int32     imageWidthInPixels;
  int32     imageHeightInPixels;
  uint16    numPlanes;
  uint16    bitsPerPixel;
  uint32    compressType;
  uint32    bmpSizeInBytes;
  int32     horizontalRes;
  int32     verticalRes;
  uint32    numColors;
  uint32    numImportantColors;
} CBMPBitMapHeader;




///////////////////////////////////////////////////////
class CBMPImageFile : public CImageFile
{
public:
    NEWEX_IMPL()
    CBMPImageFile();
    virtual ~CBMPImageFile();
    ErrVal InitializeForNewFile(const char *pFilePath);

    /////////////////////////////
    // class CImageFile
    virtual ErrVal ReadImageFile(const char *pFilePath);
    virtual ErrVal InitializeFromBitMap(
                            char *pSrcBitMap, 
                            const char *pBitmapFormat, 
                            int32 widthInPixels, 
                            int32 heightInPixels, 
                            int32 bitsPerPixel);
    virtual ErrVal InitializeFromSource(CImageFile *pDest, uint32 value);
    virtual void Close();
    virtual void CloseOnDiskOnly();

    virtual ErrVal SaveAs(const char *pNewPathName, int32 options);
    virtual ErrVal Save(int32 options);

    virtual ErrVal GetImageInfo(int32 *pMaxXPos, int32 *pMaxYPos);
    virtual ErrVal GetBitMap(char **ppBitMap, int32 *pBitmapLength);

    virtual ErrVal GetPixel(int32 xPos, int32 yPos, uint32 *pResult);
    virtual ErrVal SetPixel(int32 xPos, int32 yPos, uint32 value);

    virtual void ParsePixel(uint32 value, uint32 *pBlue, uint32 *pGreen, uint32 *pRed);
    virtual uint32 ConvertGrayScaleToPixel(uint32 grayScaleValue);

    virtual bool RowOperationsAreFast() { return(true); }
    virtual ErrVal CopyPixelRow(int32 srcX, int32 srcY, int32 destX, int32 destY, int32 numPixels);
    virtual ErrVal CropImage(int32 newWidth, int32 newHeight);

private:
    enum CIOBufferOp {
        FILE_COMPRESSION_TYPE_RGB       = 0,
        FILE_COMPRESSION_TYPE_RLE8      = 1,
        FILE_COMPRESSION_TYPE_RLE4      = 2,
        FILE_COMPRESSION_TYPE_BITFIELDS = 3,
        FILE_COMPRESSION_TYPE_JPEG      = 4,
    };

    ErrVal Parse();

    // The file. This is optional, and may be NULL if this is a 
    // memory-only object.
    CSimpleFile             m_File;
    char                    *m_pFilePathName;
    bool                    m_fReadFromBitMap;

    // The actual contents.
    char                    *m_pBuffer;
    uint64                  m_FileLength;

    // Pointers int m_pBuffer with the parsed sections.
    CBMPImageFileSignature  *m_pFileSignature;
    CBMPImageFileHeader     *m_pFileHeader;
    CBMPBitMapHeader        *m_pBitMapHeader;
    uint32                  *m_pColorTable;
    uint32                  m_NumColorsInColorTable;
    char                    *m_pPixelTable;
    uint32                  m_NumColorTableEntriesWritten;

    int32                   m_BytesPerRowInPixelTable;
    int32                   m_BytesInColorTable;
    int32                   m_BytesInPixelArray;
    bool                    m_fRowsAreUpsideDown;
    int32                   m_BytesToReadPerPixel;

    uint32                  m_MaskPreservingLowerBits[8];
}; // CBMPImageFile


#define MAX_OVERWRITTEN_COLORS      32
#define FIRST_OVERWRITTEN_COLOR     64





/////////////////////////////////////////////////////////////////////////////
//
// [OpenBMPFile]
//
/////////////////////////////////////////////////////////////////////////////
CImageFile *
OpenBMPFile(const char *pFilePath) {
    ErrVal err = ENoErr;
    CBMPImageFile *pParser = NULL;

    if (NULL == pFilePath) {
        gotoErr(EFail);
    }

    pParser = newex CBMPImageFile;
    if (NULL == pParser) {
        gotoErr(EFail);
    }

    err = pParser->ReadImageFile(pFilePath);
    if (err) {
        gotoErr(err);
    }

    return(pParser);

abort:
    delete pParser;
    return(NULL);
} // OpenBMPFile





/////////////////////////////////////////////////////////////////////////////
//
// [OpenBitmapImage]
//
/////////////////////////////////////////////////////////////////////////////
CImageFile *
OpenBitmapImage(char *pSrcBitMap, const char *pBitmapFormat, 
                int32 widthInPixels, int32 heightInPixels, int32 bitsPerPixel) {
    ErrVal err = ENoErr;
    CBMPImageFile *pParser = NULL;

    if ((NULL == pSrcBitMap) || (NULL == pBitmapFormat)) {
        gotoErr(EFail);
    }

    pParser = newex CBMPImageFile;
    if (NULL == pParser) {
        gotoErr(EFail);
    }

    err = pParser->InitializeFromBitMap(
                        pSrcBitMap, 
                        pBitmapFormat,
                        widthInPixels,
                        heightInPixels,
                        bitsPerPixel);
    if (err) {
        gotoErr(err);
    }

    return(pParser);

abort:
    delete pParser;
    return(NULL);
} // OpenBitmapImage






/////////////////////////////////////////////////////////////////////////////
//
// [MakeNewBMPImage]
//
/////////////////////////////////////////////////////////////////////////////
CImageFile *
MakeNewBMPImage(const char *pNewFilePath) {
    ErrVal err = ENoErr;
    CBMPImageFile *pParser = NULL;

    pParser = newex CBMPImageFile;
    if (NULL == pParser) {
        gotoErr(EFail);
    }

    err = pParser->InitializeForNewFile(pNewFilePath);
    if (err) {
        gotoErr(err);
    }

    return(pParser);

abort:
    delete pParser;
    return(NULL);
} // MakeNewBMPImage






/////////////////////////////////////////////////////////////////////////////
//
// [DeleteImageObject]
//
/////////////////////////////////////////////////////////////////////////////
void
DeleteImageObject(CImageFile *pParserInterface) {
    CBMPImageFile *pParser = (CBMPImageFile *) pParserInterface;

    if (pParser) {
        pParser->Close();
        delete pParser;
    }
} // DeleteImageObject





/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
CBMPImageFile::CBMPImageFile() {
    int32 bitNum;

    m_pFilePathName = NULL;
    m_fReadFromBitMap = false;

    m_pBuffer = NULL;
    m_FileLength = 0;

    m_pFileSignature = NULL;
    m_pFileHeader = NULL;
    m_pBitMapHeader = NULL;

    m_pColorTable = NULL;
    m_pPixelTable = NULL;
    m_NumColorsInColorTable = 0;
    m_NumColorTableEntriesWritten = 0;

    m_BytesPerRowInPixelTable = 0;
    m_BytesInColorTable = 0;
    m_BytesInPixelArray = 0;
    m_fRowsAreUpsideDown = false;
    m_BytesToReadPerPixel = 0;

    for (bitNum = 0; bitNum < 8; bitNum++) {
        m_MaskPreservingLowerBits[bitNum] = (1 << bitNum) - 1;
    }
} // CBMPImageFile





/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
CBMPImageFile::~CBMPImageFile() {
    Close();
}




/////////////////////////////////////////////////////////////////////////////
//
// [ReadImageFile]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
CBMPImageFile::ReadImageFile(const char *pFilePath) {
    ErrVal err = ENoErr;
    int32 numBytesRead;

    if (NULL == pFilePath) {
        gotoErr(EFail);
    }
    Close();
    m_fReadFromBitMap = false;

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
    m_pBuffer = (char *) memAlloc((int32) m_FileLength);
    if (NULL == m_pBuffer) {
        gotoErr(EFail);
    }

    err = m_File.Seek(0, CSimpleFile::SEEK_START);
    if (err) {
        gotoErr(err);
    }
    err = m_File.Read(m_pBuffer, (int32) m_FileLength, &numBytesRead);
    if (err) {
        gotoErr(err);
    }

    err = Parse();
    if (err) {
        gotoErr(err);
    }

abort:
    returnErr(err);
} // ReadImageFile




/////////////////////////////////////////////////////////////////////////////
//
// [InitializeFromBitMap]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
CBMPImageFile::InitializeFromBitMap(
                char *pSrcBitMap,
                const char *pBitmapFormat, 
                int32 widthInPixels, 
                int32 heightInPixels, 
                int32 bitsPerPixel) {
    ErrVal err = ENoErr;
    int32 bytesPerPixel;
    int32 bitmapSizeInBytes;
    char *pDestPtr;

    if ((NULL == pSrcBitMap) 
            || (NULL == pBitmapFormat)
            || (widthInPixels <= 0)
            || (heightInPixels <= 0)
            || (bitsPerPixel <= 0)) {
        gotoErr(EFail);
    }
    Close();
    // Leave the filename. We may create an image file then reset its contents
    // from another file.
    // m_pFilePathName = NULL;
    m_fReadFromBitMap = true;

    // Make a copy of the bitmap
    bytesPerPixel = bitsPerPixel / 8;
    bitmapSizeInBytes = (bytesPerPixel * widthInPixels * heightInPixels);
    m_FileLength = sizeof(CBMPImageFileSignature)
                    + sizeof(CBMPImageFileHeader)
                    + sizeof(CBMPBitMapHeader)
                    + bitmapSizeInBytes;
    m_pBuffer = (char *) memAlloc((int32) m_FileLength);
    if (NULL == m_pBuffer) {
        gotoErr(EFail);
    }

    pDestPtr = m_pBuffer + sizeof(CBMPImageFileSignature)
                         + sizeof(CBMPImageFileHeader)
                         + sizeof(CBMPBitMapHeader);
#if WASM
    WASMmemcpy(pDestPtr, pSrcBitMap, bitmapSizeInBytes);
#else
    memcpy(pDestPtr, pSrcBitMap, bitmapSizeInBytes);
#endif

    // There is no file, so make private copies of the headers.
    pDestPtr = m_pBuffer;
    m_pFileSignature = (CBMPImageFileSignature *) pDestPtr;
    pDestPtr = pDestPtr + sizeof(CBMPImageFileSignature);
    m_pFileSignature->magic[0] = 'B';
    m_pFileSignature->magic[1] = 'M';

    m_pFileHeader = (CBMPImageFileHeader *) pDestPtr;
    pDestPtr = pDestPtr + sizeof(CBMPImageFileHeader);
    m_pFileHeader->filesz = m_FileLength;
    m_pFileHeader->creator1 = 0;
    m_pFileHeader->creator2 = 0;
    m_pFileHeader->bmpOffset = sizeof(CBMPImageFileSignature)
                                + sizeof(CBMPImageFileHeader)
                                + sizeof(CBMPBitMapHeader);

    m_pBitMapHeader = (CBMPBitMapHeader *) pDestPtr;
    pDestPtr = pDestPtr + sizeof(CBMPBitMapHeader);
    m_pBitMapHeader->headerSize = sizeof(CBMPBitMapHeader);
    m_pBitMapHeader->imageHeightInPixels = heightInPixels;
    m_pBitMapHeader->imageWidthInPixels = widthInPixels;
    m_pBitMapHeader->numPlanes = 1;
    m_pBitMapHeader->bitsPerPixel = bitsPerPixel;
    m_pBitMapHeader->compressType = 0;
    m_pBitMapHeader->bmpSizeInBytes = bitmapSizeInBytes;
    m_pBitMapHeader->horizontalRes = 0;
    m_pBitMapHeader->verticalRes = 0;
    m_pBitMapHeader->numColors = 0;
    m_pBitMapHeader->numImportantColors = 0;

    // The color table is optional.
    m_pColorTable = NULL;
    m_NumColorsInColorTable = 0;
    m_BytesInColorTable = 0;

    m_pPixelTable = m_pBuffer;
    m_fRowsAreUpsideDown = false;

    // Pixels are packed in rows. Rows are then stored sequentially.
    m_BytesPerRowInPixelTable = m_pBitMapHeader->imageWidthInPixels * bytesPerPixel;
    // The pixel array is just a series of rows.
    m_BytesInPixelArray = m_BytesPerRowInPixelTable * m_pBitMapHeader->imageHeightInPixels;
    m_BytesToReadPerPixel = bytesPerPixel;

abort:
    returnErr(err);
} // InitializeFromBitMap








/////////////////////////////////////////////////////////////////////////////
//
// [InitializeForNewFile]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
CBMPImageFile::InitializeForNewFile(const char *pFilePath) {
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
CBMPImageFile::Close() {
    memFree(m_pFilePathName);
    m_pFilePathName = NULL;

    memFree(m_pBuffer);
    m_pBuffer = NULL;
    m_FileLength = 0;

    m_pFileSignature = NULL;
    m_pFileHeader = NULL;
    m_pBitMapHeader = NULL;

    m_pColorTable = NULL;
    m_pPixelTable = NULL;
    m_NumColorsInColorTable = 0;
    m_NumColorTableEntriesWritten = 0;

    m_BytesPerRowInPixelTable = 0;
    m_BytesInColorTable = 0;
    m_BytesInPixelArray = 0;
    m_fRowsAreUpsideDown = false;
    m_BytesToReadPerPixel = 0;

    m_File.Close();
} // Close





    
/////////////////////////////////////////////////////////////////////////////
//
// [CloseOnDiskOnly]
//
/////////////////////////////////////////////////////////////////////////////
void
CBMPImageFile::CloseOnDiskOnly() {
    m_File.Close();
} // CloseOnDiskOnly





/////////////////////////////////////////////////////////////////////////////
//
// [SaveAs]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
CBMPImageFile::SaveAs(const char *pNewPathName, int32 options) {
    ErrVal err = ENoErr;

    if (NULL == pNewPathName) {
        gotoErr(EFail);
    }

    CloseOnDiskOnly();

    if (pNewPathName) {
        CSimpleFile::DeleteFile(pNewPathName);
        err = m_File.OpenOrCreateEmptyFile(pNewPathName, 0);
        if (err) {
            gotoErr(err);
        }

        // Save a copy of the file name so we can reopen it and change it later.
        memFree(m_pFilePathName);
        m_pFilePathName = strdupex(pNewPathName);
        if (NULL == m_pFilePathName) {
            gotoErr(EFail);
        }
    } // if (pFilePath)

    err = Save(options);
    if (err) {
        gotoErr(err);
    }

abort:
    returnErr(err);
} // SaveAs






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
CBMPImageFile::Save(int32 options) {
    ErrVal err = ENoErr;

    // If this is a temporary file, then we do nothing to save it.
    if ((NULL == m_pBuffer) || (NULL == m_pFilePathName)) {
        gotoErr(ENoErr);
    }

    if (options & BIOCAD_FILE_MAY_CREATE_FILE) { // (m_File.IsOpen()))
        if (!(options & BIOCAD_FILE_CLOSE_AFTER_SAVE)) {
            gotoErr(ENoErr);
        }
        if (NULL == m_pFilePathName) {
            gotoErr(EFail);
        }
        m_File.Close();
        CSimpleFile::DeleteFile(m_pFilePathName);
        err = m_File.OpenOrCreateEmptyFile(m_pFilePathName, 0);
        if (err) {
            gotoErr(err);
        }
    }
    
    err = m_File.Seek(0, CSimpleFile::SEEK_START);
    if (err) {
        gotoErr(err);
    }
    err = m_File.Write(m_pBuffer, (int32) m_FileLength);
    if (err) {
        gotoErr(err);
    }
    err = m_File.Flush();
    if (err) {
        gotoErr(err);
    }
    err = m_File.SetFileLength(m_FileLength);
    if (err) {
        gotoErr(err);
    }

    if (options & BIOCAD_FILE_CLOSE_AFTER_SAVE) {
        m_File.Close();
    }

abort:
    returnErr(err);
} // Save






/////////////////////////////////////////////////////////////////////////////
//
// [Parse]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
CBMPImageFile::Parse() {
    ErrVal err = ENoErr;
    char *pSrcPtr;
    char *pEndSrcPtr;
    int32 bitsPerRowInPixelTable = 0;
    int32 qWordsPerRowInPixelTable = 0;


    if ((NULL == m_pBuffer) || (m_FileLength <= 0)) {
        gotoErr(EFail);
    }
    pSrcPtr = m_pBuffer;
    pEndSrcPtr = m_pBuffer + m_FileLength;

    // The file is layed out as follows:
    //
    // -------------------------------------------------------------------
    // | fileSignature | fileHeader | bitmapHeader | ColorTable | Pixels |
    // -------------------------------------------------------------------

    // Read the headers.
    m_pFileSignature = (CBMPImageFileSignature *) pSrcPtr;
    pSrcPtr += sizeof(*m_pFileSignature);
    if (('B' != (char) m_pFileSignature->magic[0]) || ('M' != (char) m_pFileSignature->magic[1])) {
        gotoErr(EFail);
    }

    m_pFileHeader = (CBMPImageFileHeader *) pSrcPtr;
    pSrcPtr += sizeof(*m_pFileHeader);

    m_pBitMapHeader = (CBMPBitMapHeader *) pSrcPtr;
    pSrcPtr += sizeof(*m_pBitMapHeader);
    if (sizeof(*m_pBitMapHeader) != m_pBitMapHeader->headerSize) {
        gotoErr(EFail);
    }

    // For now, I ignore compressed files.
    // <> Fix this.
    if (FILE_COMPRESSION_TYPE_RGB != m_pBitMapHeader->compressType) {
        gotoErr(EFail);
    }

    // Validate the header a little bit.
    // Currently, I do not support 64-bits per pixel.
    if ((1 != m_pBitMapHeader->bitsPerPixel)
            && (2 != m_pBitMapHeader->bitsPerPixel)
            && (4 != m_pBitMapHeader->bitsPerPixel)
            && (8 != m_pBitMapHeader->bitsPerPixel)
            && (16 != m_pBitMapHeader->bitsPerPixel)
            && (24 != m_pBitMapHeader->bitsPerPixel)
            && (32 != m_pBitMapHeader->bitsPerPixel)) {
        gotoErr(EFail);
    }

    // The color table is optional and is not present in most files.
    // If the pixels start right after the headers, then there is no color table.
    m_pColorTable = (uint32 *) pSrcPtr;
    m_pPixelTable = m_pBuffer + m_pFileHeader->bmpOffset;
    if ((char *) m_pColorTable == m_pPixelTable) {
        m_pColorTable = NULL;
    }
    m_NumColorsInColorTable = m_pBitMapHeader->numColors;
    // Any bitmap must have at least 1 color. So, 0 is reserved to mean 2**n colors.
    if (0 == m_NumColorsInColorTable) {
        m_NumColorsInColorTable = 1 << m_pBitMapHeader->bitsPerPixel;
    }

    // Windows bitmaps arrange rows opposite the normal order if Image Height < 0.
    if (m_pBitMapHeader->imageHeightInPixels < 0) {
        m_fRowsAreUpsideDown = true;
        m_pBitMapHeader->imageHeightInPixels = -(m_pBitMapHeader->imageHeightInPixels);
    }

    // Pixels are packed in rows. Rows are then stored sequentially.
    // Each row is rounded up to a multiple of 4 bytes.
    // This is the number of pixels.
    bitsPerRowInPixelTable = m_pBitMapHeader->bitsPerPixel * m_pBitMapHeader->imageWidthInPixels;
    qWordsPerRowInPixelTable = ((int32) (bitsPerRowInPixelTable / 32));
    // Each row is rounded up to a multiple of 4 bytes.
    while ((qWordsPerRowInPixelTable * 32) < bitsPerRowInPixelTable) {
        qWordsPerRowInPixelTable += 1;
    }
    m_BytesPerRowInPixelTable = qWordsPerRowInPixelTable * 4;

    // The pixel array is just a series of rows.
    m_BytesInPixelArray = m_BytesPerRowInPixelTable * m_pBitMapHeader->imageHeightInPixels;
    if ((m_pPixelTable + m_BytesInPixelArray) > pEndSrcPtr) {
        gotoErr(EFail);
    }

    m_BytesInColorTable = sizeof(int32) * (1 << m_pBitMapHeader->bitsPerPixel);
    if ((m_pColorTable)
        && ((((char *) m_pColorTable) + m_BytesInColorTable) > m_pPixelTable)) {
        gotoErr(EFail);
    }

    m_BytesToReadPerPixel = m_pBitMapHeader->bitsPerPixel / 8;
    // Round up, since part of a byte will still need a full byte.
    if ((m_BytesToReadPerPixel * 8) < m_pBitMapHeader->bitsPerPixel) {
        m_BytesToReadPerPixel += 1;
    }

abort:
    returnErr(err);
} // Parse





/////////////////////////////////////////////////////////////////////////////
//
// [GetImageInfo]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
CBMPImageFile::GetImageInfo(int32 *pMaxXPos, int32 *pMaxYPos) {
    ErrVal err = ENoErr;

    if (NULL == m_pBitMapHeader) {
        gotoErr(EFail);
    }

    if (pMaxXPos) {
        *pMaxXPos = m_pBitMapHeader->imageWidthInPixels;
    }
    if (pMaxYPos) {
        *pMaxYPos = m_pBitMapHeader->imageHeightInPixels;
    }

abort:
    returnErr(err);
} // GetImageInfo







/////////////////////////////////////////////////////////////////////////////
//
// [GetBitMap]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
CBMPImageFile::GetBitMap(char **ppBitMap, int32 *pBitmapLength) {
    ErrVal err = ENoErr;

    if ((NULL == m_pBitMapHeader) || (NULL == m_pBuffer)) {
        gotoErr(EFail);
    }

    if (ppBitMap) {
        *ppBitMap = m_pBuffer;
    }
    if (pBitmapLength) {
        *pBitmapLength = m_pBitMapHeader->bmpSizeInBytes;
    }

abort:
    returnErr(err);
} // GetBitMap







/////////////////////////////////////////////////////////////////////////////
//
// [GetPixel]
//
// (0,0) is the top-left corner. This is a relic from CRT-tubes, and is widely 
// used in computer file formats and graphics tools.
//
// The byte layout of BMP pixels is:
//    Blue is the byte 0, bits 0-7
//    Green is the byte 1, bits 8-15
//    Red is the byte 2, bits 16-23
// Please see a full explanation in the comments in imageLibInternal.h
/////////////////////////////////////////////////////////////////////////////
ErrVal
CBMPImageFile::GetPixel(int32 xPos, int32 yPos, uint32 *pResult) {
    ErrVal err = ENoErr;
    uchar *pPixelRow;
    uchar *pPixelBytes;
    int32 firstBitNumber;
    int32 firstByteNumber;
    uint32 tempPixel;
    uchar byteValue;
    int32 byteNum;

    if (NULL == pResult) {
        gotoErr(EFail);
    }
    *pResult = 0;

    // Validate the parameters.
    if ((xPos < 0) 
        || (yPos < 0)
        || (xPos > m_pBitMapHeader->imageWidthInPixels)
        || (yPos > m_pBitMapHeader->imageHeightInPixels)) {
        gotoErr(EFail);
    }

    // By default, pixel rows are stored so row (Height-1) comes first
    // in the Pixel array, and row 0 comes last.
    pPixelRow = (uchar *) m_pPixelTable + m_BytesInPixelArray;
    pPixelRow = pPixelRow - ((yPos + 1) * m_BytesPerRowInPixelTable);
    // Optionally, BMP-files can arrange rows in the opposite order.
    if (m_fRowsAreUpsideDown) {
        pPixelRow = (uchar *) m_pPixelTable + (yPos * m_BytesPerRowInPixelTable);
    }

    // Pixels are arranged in a row from left to right.
    firstBitNumber = xPos * m_pBitMapHeader->bitsPerPixel;
    firstByteNumber = firstBitNumber / 8;
    pPixelBytes = pPixelRow + firstByteNumber;

    // Read each byte in the word.
    tempPixel = 0;
    for (byteNum = 0; byteNum < m_BytesToReadPerPixel; byteNum++) {
        uint32 byteInShiftedPosition;

        // Shift up any bits we collected from the previous loop iterations.
#if !LITTLE_ENDIAN_NUMBERS
        tempPixel = tempPixel << 8;
#endif

        // Get the next byte of data.
        byteValue = *pPixelBytes;
        pPixelBytes += 1;

        byteInShiftedPosition = (uint32) byteValue;

        // The values are stored in Little-Endian order. This means the first
        // byte address in memory will hold the least significant byte.
        // Each subsequent byte we read as we go from lower to higher
        // memory addresses is a more significant byte.
#if LITTLE_ENDIAN_NUMBERS
        for (int32 index = 0; index < byteNum; index++) {
            byteInShiftedPosition = byteInShiftedPosition << 8;
        }
#endif // LITTLE_ENDIAN_NUMBERS

        // Add it into the total we are accumulating.
        tempPixel = tempPixel | byteInShiftedPosition;
    }

    // If the pixel is less than a complete byte, then we have to throw out
    // any extra data we collected.
    if (m_pBitMapHeader->bitsPerPixel < 8) {
        ASSERT_UNTESTED();
        
        int32 numPixelsPerByte = 8 / m_pBitMapHeader->bitsPerPixel;
        int32 firstPixelRead = firstByteNumber * numPixelsPerByte;
        int32 lastPixelRead = firstPixelRead + (numPixelsPerByte - 1);

        //int32 numExtraPixelsLeftOfTarget = xPos - firstPixelRead;
        int32 numExtraPixelsRightOfTarget = lastPixelRead - xPos;

        // Discard pixels after the one we want.
        for (int32 pixelNum = 0; pixelNum < numExtraPixelsRightOfTarget; pixelNum++) {
            tempPixel = tempPixel >> m_pBitMapHeader->bitsPerPixel;
        }

        // Discard any pixels before the ones we one.
        tempPixel = tempPixel & m_MaskPreservingLowerBits[m_pBitMapHeader->bitsPerPixel];
    } // if (m_pBitMapHeader->bitsPerPixel < 8)


    // If there is a color table, then the pixel is actually just an index into that 
    // table.
    if (m_pColorTable) {
        uint32 colorNumber = tempPixel;
        uint32 colorTableValue;

        // colorNumber is a uint, so it cannot be negative.
        if (colorNumber < m_NumColorsInColorTable) {
            // Each entry in the table is usually 4 bytes, with the format: "blue, green, red, 0x00"
            // But, in Intel Format (which is little endian) reverse the bytes.
            colorTableValue = m_pColorTable[colorNumber];
            tempPixel = colorTableValue & 0x00FFFFFF;
        }
    } // if (m_pColorTable)

    *pResult = tempPixel;

abort:
    returnErr(err);
} // GetPixel






/////////////////////////////////////////////////////////////////////////////
//
// [SetPixel]
//
// The byte layout of BMP pixels is:
//    Blue is the byte 0, bits 0-7
//    Green is the byte 1, bits 8-15
//    Red is the byte 2, bits 16-23
// Please see a full explanation in the comments in imageLibInternal.h
/////////////////////////////////////////////////////////////////////////////
ErrVal
CBMPImageFile::SetPixel(int32 xPos, int32 yPos, uint32 value) {
    ErrVal err = ENoErr;
    uchar *pPixelRow;
    uchar *pPixelBytes;
    int32 firstBitNumber;
    int32 firstByteNumber;
    uint32 tempPixel;
    int32 byteNum;

    // Validate the parameters.
    if ((xPos < 0) 
        || (yPos < 0)
        || (xPos > m_pBitMapHeader->imageWidthInPixels)
        || (yPos > m_pBitMapHeader->imageHeightInPixels)) {
        gotoErr(EFail);
    }

    // If there is a color table, then the pixel we will store is actually 
    // just an index into that table. Find the color that corresponds
    // to what we want to store.
    if (m_pColorTable) {
        uint32 colorNum;
        uint32 colorTableValue;

        for (colorNum = 0; colorNum < m_NumColorsInColorTable; colorNum++) {
            // Each entry in the color table is 4 bytes (in typical file formats)
            colorTableValue = m_pColorTable[colorNum];

            // Each entry in the table is usually 4 bytes, with the format: "blue, green, red, 0x00"
            // But, in Intel Format (which is little endian) reverse the bytes.
            colorTableValue = colorTableValue & 0x00FFFFFF;

            if (colorTableValue == value) {
                value = colorNum;
                break;
            }
        }

        // If this is an invalid color, then define it.
        // A typical color table will be gray scale, which is a bit inconvenient.
        // So, just STEP on a color in the middle.
        // Overwrite colors in the middle, since the extremes tend to be black and
        // white, and we need those.
        if (colorNum >= m_NumColorsInColorTable) {
            if (m_NumColorTableEntriesWritten < MAX_OVERWRITTEN_COLORS) {
                colorNum = FIRST_OVERWRITTEN_COLOR + m_NumColorTableEntriesWritten;
                m_NumColorTableEntriesWritten += 1;
            
                // Each entry in the table is usually 4 bytes, with the format: "blue, green, red, 0x00"
                colorTableValue = value | 0xFF000000;

                m_pColorTable[colorNum] = colorTableValue;
                value = colorNum;
            }
        }
    } // if (m_pColorTable)


    // By default, pixel rows are stored so row (Height-1) comes first
    // in the Pixel array, and row 0 comes last.
    pPixelRow = (uchar *) m_pPixelTable + m_BytesInPixelArray;
    pPixelRow = pPixelRow - ((yPos + 1) * m_BytesPerRowInPixelTable);
    // Optionally, BMP-files can arrange rows in the opposite order.
    if (m_fRowsAreUpsideDown) {
        pPixelRow = (uchar *) m_pPixelTable + (yPos * m_BytesPerRowInPixelTable);
    }

    // Pixels are arranged in a row from left to right.
    firstBitNumber = xPos * m_pBitMapHeader->bitsPerPixel;
    firstByteNumber = firstBitNumber / 8;
    pPixelBytes = pPixelRow + firstByteNumber;

    tempPixel = value;
#if !LITTLE_ENDIAN_NUMBERS
    // Shift all of the pixel data so it is left-aligned.
    for (byteNum = m_BytesToReadPerPixel; byteNum < 4; byteNum++) {
        tempPixel = tempPixel << 8;
    }
#endif

    // If the pixel is less than a complete byte, then we have to throw out
    // any extra data we collected.
    if (m_pBitMapHeader->bitsPerPixel < 8) {
        ASSERT_UNTESTED();

        int32 numPixelsPerByte = 8 / m_pBitMapHeader->bitsPerPixel;

        int32 firstPixelRead = firstByteNumber * numPixelsPerByte;
        int32 lastPixelRead = firstPixelRead + (numPixelsPerByte - 1);

        int32 numExtraPixelsLeftOfTarget = xPos - firstPixelRead;
        int32 numExtraPixelsRightOfTarget = lastPixelRead - xPos;

        uint8 numBitsLeftOfTarget = numExtraPixelsLeftOfTarget * m_pBitMapHeader->bitsPerPixel;
        uint8 numBitsRightOfTarget = numExtraPixelsRightOfTarget * m_pBitMapHeader->bitsPerPixel;

        uint8 leftPixelsMask = (1 << numBitsLeftOfTarget) - 1;
        leftPixelsMask = leftPixelsMask << (numBitsRightOfTarget + m_pBitMapHeader->bitsPerPixel);

        uint8 rightPixelsMask = (1 << numBitsRightOfTarget) - 1;

        // We will trim both down. First, read the complete byte.
        uint8 extraPixelsToLeft = *pPixelBytes;
        extraPixelsToLeft = extraPixelsToLeft & leftPixelsMask;

        uint8 extraPixelsToRight = *pPixelBytes;
        extraPixelsToRight = extraPixelsToRight & rightPixelsMask;
        
        uchar byteToWrite = *pPixelBytes;
        // Discard the stuff to the left and the right.
        byteToWrite = byteToWrite & ~(leftPixelsMask);
        byteToWrite = byteToWrite & ~(rightPixelsMask);
    
        // Combine all of the values that we *DO* want.
        byteToWrite = extraPixelsToLeft | byteToWrite | extraPixelsToRight;

        // Now, put the assembled byte in the pixel code that is read byte-at-a-time
        tempPixel = byteToWrite;
    } // if (m_pBitMapHeader->bitsPerPixel < 8)


    // Copy each byte of the pixel value into the memory array.
    // This uses little-endian order - so the lowest memory address is the least
    // significant byte in the value.
    for (byteNum = 0; byteNum < m_BytesToReadPerPixel; byteNum++) {
        uint32 byteToWriteInInt32;
        uchar byteToWrite;

        // Get the least significant byte.
#if LITTLE_ENDIAN_NUMBERS
        byteToWriteInInt32 = tempPixel & 0x000000FF;
#else
        byteToWriteInInt32 = tempPixel >> 24;
#endif
        byteToWrite = (uchar) byteToWriteInInt32;

        *pPixelBytes = byteToWrite;
        pPixelBytes += 1;

        // Shift so the next byte to write will be in the least significant position.
        // This uses little-endian order - so the lowest memory address is the least
        // significant byte in the value.
#if LITTLE_ENDIAN_NUMBERS
        tempPixel = tempPixel >> 8;
#else
        tempPixel = tempPixel << 8;
#endif
    }

abort:
    returnErr(err);
} // SetPixel







/////////////////////////////////////////////////////////////////////////////
//
// [CopyPixelRow]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
CBMPImageFile::CopyPixelRow(int32 srcX, int32 srcY, int32 destX, int32 destY, int32 numPixels) {
    ErrVal err = ENoErr;
    uchar *pSrcPixelRow;
    uchar *pDestPixelRow;
    uchar *pSrcPixelBytes;
    uchar *pDestPixelBytes;
    int32 firstByteNumber;
    int32 numBytesToCopy;

    // Validate the parameters.
    if ((srcX < 0)
        || (srcX >= m_pBitMapHeader->imageWidthInPixels)
        || (srcY < 0) 
        || (srcY >= m_pBitMapHeader->imageHeightInPixels)
        || (destX < 0)
        || (destX >= m_pBitMapHeader->imageWidthInPixels)
        || (destY < 0)
        || (destY >= m_pBitMapHeader->imageHeightInPixels)
        || (numPixels < 0)
        || (numPixels >= m_pBitMapHeader->imageWidthInPixels)) {
        gotoErr(EFail);
    }

    // Clip the copy to the size of the image.
    if ((srcX + numPixels) >= m_pBitMapHeader->imageWidthInPixels) {
        numPixels = m_pBitMapHeader->imageWidthInPixels - srcX;
    }
    if ((destX + numPixels) >= m_pBitMapHeader->imageWidthInPixels) {
        numPixels = m_pBitMapHeader->imageWidthInPixels - destX;
    }

    // By default, pixel rows are stored so row (Height-1) comes first
    // in the Pixel array, and row 0 comes last.
    pSrcPixelRow = (uchar *) m_pPixelTable + m_BytesInPixelArray;
    pSrcPixelRow = pSrcPixelRow - ((srcY + 1) * m_BytesPerRowInPixelTable);
    pDestPixelRow = (uchar *) m_pPixelTable + m_BytesInPixelArray;
    pDestPixelRow = pDestPixelRow - ((destY + 1) * m_BytesPerRowInPixelTable);
    // Optionally, BMP-files can arrange rows in the opposite order.
    if (m_fRowsAreUpsideDown) {
        pSrcPixelRow = (uchar *) m_pPixelTable + (srcY * m_BytesPerRowInPixelTable);
        pDestPixelRow = (uchar *) m_pPixelTable + (destY * m_BytesPerRowInPixelTable);
    }

    // Pixels are arranged in a row from left to right.
    firstByteNumber = srcX * m_BytesToReadPerPixel;
    pSrcPixelBytes = pSrcPixelRow + firstByteNumber;

    firstByteNumber = destX * m_BytesToReadPerPixel;
    pDestPixelBytes = pDestPixelRow + firstByteNumber;

    numBytesToCopy = numPixels * m_BytesToReadPerPixel;
    memcpy(pDestPixelBytes, pSrcPixelBytes, numBytesToCopy);

abort:
    returnErr(err);
} // CopyPixelRow







/////////////////////////////////////////////////////////////////////////////
//
// [CropImage]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
CBMPImageFile::CropImage(int32 newWidth, int32 newHeight) {
    ErrVal err = ENoErr;
    uchar *pSrcPixelRow;
    uchar *pDestPixelRow;
    int32 currentRowNum;
    int32 newNumBytesInRow;
    int32 numRowsCopied;
    int32 qWordsPerRowInPixelTable = 0;
    int32 bitsPerRowInPixelTable = 0;


    // Validate the parameters.
    if ((newWidth < 0)
            || (newWidth >= m_pBitMapHeader->imageWidthInPixels)
            || (newHeight < 0) 
            || (newHeight >= m_pBitMapHeader->imageHeightInPixels)) {
        gotoErr(EFail);
    }

    // Pixels are packed in rows. Rows are then stored sequentially.
    // Each row is rounded up to a multiple of 4 bytes. This is the number of pixels.
    bitsPerRowInPixelTable = m_pBitMapHeader->bitsPerPixel * newWidth;
    qWordsPerRowInPixelTable = ((int32) (bitsPerRowInPixelTable / 32));
    // Each row is rounded up to a multiple of 4 bytes.
    while ((qWordsPerRowInPixelTable * 32) < bitsPerRowInPixelTable) {
        qWordsPerRowInPixelTable += 1;
    }
    newNumBytesInRow = qWordsPerRowInPixelTable * 4;

    // Compact the PixelMap, so we will use the same buffer.
    // This will make things more tricky because the first bytes in memory may be
    // either the start of Row 0, or Row (Height - 1)
    //
    // We copy in memory order, not by rows.
    // We will also only copy bytes 0 - newWidth from each row. 
    // Because we are shortening each row, and because row N+1 starts
    // immediately after row N finishes, we will have to copy all rows
    // to compact the image.
    //
    // To avoid clobbering the image, we need to start with copying the first
    // row that appears in memory.
    // This means, depending on how the rows are arranged, we will either
    // copy rows newHeight-1, newHeight-2,....0, or rows 0,1,2,....newHeight-1.
    pDestPixelRow = (uchar *) m_pPixelTable;
    // By default, pixel rows are stored so row (Height-1) comes first
    // in the Pixel array, and row 0 comes last.
    currentRowNum = newHeight - 1;
    pSrcPixelRow = (uchar *) m_pPixelTable + m_BytesInPixelArray;
    pSrcPixelRow = pSrcPixelRow - ((currentRowNum + 1) * m_BytesPerRowInPixelTable);
    // Optionally, BMP-files can arrange rows in the opposite order.
    // In this case, row 0 comes first, so we will count up.
    if (m_fRowsAreUpsideDown) {
        currentRowNum = 0;
        pSrcPixelRow = (uchar *) m_pPixelTable;
    }

    // Iterate for newHeight-1 rows.
    // This copies in the order they are in memory.
    // This means, depending on how the rows are arranged, we will either
    // copy rows newHeight-1, newHeight-2,....0, or rows 0,1,2,....newHeight-1.
    for (numRowsCopied = 0; numRowsCopied < newHeight; numRowsCopied++) {
        memcpy(pDestPixelRow, pSrcPixelRow, newNumBytesInRow);

        pDestPixelRow += newNumBytesInRow;
        pSrcPixelRow += m_BytesPerRowInPixelTable;
    } // for (numRowsCopied = 0; numRowsCopied < newHeight; numRowsCopied++)


    // Update the state.
    m_BytesPerRowInPixelTable = newNumBytesInRow;
    m_BytesInPixelArray = newNumBytesInRow * newHeight;
    m_FileLength = m_pFileHeader->bmpOffset + m_BytesInPixelArray;

    m_pFileHeader->filesz = m_FileLength;
    m_pBitMapHeader->imageWidthInPixels = newWidth;
    m_pBitMapHeader->imageHeightInPixels = newHeight;
    m_pBitMapHeader->bmpSizeInBytes = m_BytesInPixelArray;
 
abort:
    returnErr(err);
} // CropImage







/////////////////////////////////////////////////////////////////////////////
//
// [InitializeFromSource]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
CBMPImageFile::InitializeFromSource(CImageFile *pSourceAPI, uint32 value) {
    ErrVal err = ENoErr;
    uchar *pPixelRow;
    uchar *pDestPixel;
    uchar *pFirstPixelRow;
    int32 rowNum;
    int32 byteNum;
    CBMPImageFile *pSource;

    if (NULL == pSourceAPI) {
        gotoErr(EFail);
    }
    pSource = (CBMPImageFile *) pSourceAPI;


    memFree(m_pBuffer);
    m_pBuffer = NULL;
    m_FileLength = 0;


    m_FileLength = pSource->m_FileLength;
    m_pBuffer = (char *) memAlloc((int32) (m_FileLength));
    if (NULL == m_pBuffer) {
        gotoErr(EFail);
    }
#if WASM
    WASMmemcpy(m_pBuffer, pSource->m_pBuffer, (int32) (m_FileLength));
#else
    memcpy(m_pBuffer, pSource->m_pBuffer, (int32) (m_FileLength));
#endif

    if (m_File.IsOpen()) {
        err = m_File.SetFileLength(m_FileLength);
        if (err) {
            gotoErr(err);
        }
    }

    err = Parse();
    if (err) {
        gotoErr(err);
    }

    if (NULL == m_pBitMapHeader) {
        gotoErr(EFail);
    }

    // If there is a color table, then the pixel we will store is actually 
    // just an index into that table. Find the color that corresponds
    // to what we want to store.
    if (m_pColorTable) {
        uint32 colorNum;

        for (colorNum = 0; colorNum < m_NumColorsInColorTable; colorNum++) {
            if (m_pColorTable[colorNum] == value) {
                value = colorNum;
                break;
            }
        }

        // If this is an invalid color, then define it.
        // A typical color table will be gray scale, which is a bit inconvenient.
        // So, just STEP on a color in the middle.
        // Overwrite colors in the middle, since the extremes tend to be black and
        // white, and we need those.
        if (colorNum >= m_NumColorsInColorTable) {
            if (m_NumColorTableEntriesWritten < MAX_OVERWRITTEN_COLORS) {
                int32 colorTableValue;

                colorNum = FIRST_OVERWRITTEN_COLOR + m_NumColorTableEntriesWritten;
                m_NumColorTableEntriesWritten += 1;
            
                // Each entry in the table is usually 4 bytes, with the format: "blue, green, red, 0x00"
                colorTableValue = value | 0xFF000000;

                m_pColorTable[colorNum] = colorTableValue;
                value = colorNum;
            }
        }
    } // if (m_pColorTable)

    // Fill out the first row. This may be a bit slow.
    pFirstPixelRow = (uchar *) m_pPixelTable;
    pDestPixel = pFirstPixelRow;
    for (byteNum = 0; byteNum < m_BytesPerRowInPixelTable; byteNum++) {
        *pDestPixel = value;
        pDestPixel += 1;
    }

    // Make every subsequent row a copy of the first.
    for (rowNum = 1; rowNum < m_pBitMapHeader->imageHeightInPixels; rowNum++) {
        pPixelRow = (uchar *) m_pPixelTable + (rowNum * m_BytesPerRowInPixelTable);
#if WASM
        WASMmemcpy(pPixelRow, pFirstPixelRow, m_BytesPerRowInPixelTable);
#else
        memcpy(pPixelRow, pFirstPixelRow, m_BytesPerRowInPixelTable);
#endif
    }

abort:
    returnErr(err);
} // InitializeFromSource





/////////////////////////////////////////////////////////////////////////////
//
// [ParsePixel]
//
// This is the actual pixel. If there is a color table, then pixelValue has
// already been translated.
//
// The byte layout of BMP pixels is:
//    Blue is the byte 0, bits 0-7
//    Green is the byte 1, bits 8-15
//    Red is the byte 2, bits 16-23
// Please see a full explanation in the comments in imageLibInternal.h
/////////////////////////////////////////////////////////////////////////////
void
CBMPImageFile::ParsePixel(uint32 pixelValue, uint32 *pBlue, uint32 *pGreen, uint32 *pRed) {
    uint32 blue = 0;
    uint32 green = 0;
    uint32 red = 0;

    if (NULL == m_pBitMapHeader) {
        goto abort;
    }  


    // If there is a colorTable, then it is only used internally. This means pixelValue is the 
    // actual 24-bit pixel. If there is a color table, then pixelValue was already translated
    // when we read it with GetPixel(). More importantly, pixelValue is 24 bits, even though
    // m_pBitMapHeader->bitsPerPixel may be much smaller since in this case m_pBitMapHeader->bitsPerPixel
    // is the size of the index number used to look in the colorTable.
    if (m_pColorTable) {
        blue = pixelValue & 0x000000FF;
        green = (pixelValue >> 8) & 0x000000FF;
        red = (pixelValue >> 16) & 0x000000FF;
        goto abort;
    }

    switch (m_pBitMapHeader->bitsPerPixel) {
    //////////////////////////////////////////
    case 1:
    case 2:
    case 4:
    case 8:
        // These smaller values are indexes into a color table,
        // and the color table stores store the pixel as 8.8.8.0.8 RGBAX
        // format. See http://en.wikipedia.org/wiki/RGBAX#RGBAX for more.
        // The 5 numbers in 8.8.8.0.8 represent:
        //    Bits in Red
        //    Bits in Green
        //    Bits in Blue
        //    Bits in Alpha blending code
        //    Unused bits
        red = pixelValue & 0x000000FF;
        green = (pixelValue >> 8) & 0x000000FF;
        blue = (pixelValue >> 16) & 0x000000FF;
        break;

    //////////////////////////////////////////
    case 16:
        // This also uses the 5.5.5.0.1 RGBAX format.
        red = pixelValue & 0x0000001F;
        green = (pixelValue >> 5) & 0x0000001F;
        blue = (pixelValue >> 10) & 0x0000001F;
        break;

    //////////////////////////////////////////
    case 24:
        // This also uses the 8.8.8.0.8 RGBAX format.
        red = pixelValue & 0x000000FF;
        green = (pixelValue >> 8) & 0x000000FF;
        blue = (pixelValue >> 16) & 0x000000FF;
        break;

    //////////////////////////////////////////
    case 32:
        // I *think* this also uses the 8.8.8.0.8 RGBAX format, but there is
        // a valid value for the alpha blending. 
        red = pixelValue & 0x000000FF;
        green = (pixelValue >> 8) & 0x000000FF;
        blue = (pixelValue >> 16) & 0x000000FF;
        break;

    //////////////////////////////////////////
    default:
        blue = 0;
        green = 0;
        red = 0;
        break;
    } // switch (m_pBitMapHeader->bitsPerPixel)

    
abort:
    if (pBlue) {
        *pBlue = blue;
    }
    if (pGreen) {
        *pGreen = green;
    }
    if (pRed) {
        *pRed = red;
    }
} // ParsePixel





/////////////////////////////////////////////////////////////////////////////
//
// [ConvertGrayScaleToPixel]
//
/////////////////////////////////////////////////////////////////////////////
uint32
CBMPImageFile::ConvertGrayScaleToPixel(uint32 grayScaleValue) {
    uint32 red;
    uint32 green;
    uint32 blue;
    uint32 pixelValue = 0;

    if (NULL == m_pBitMapHeader) {
        return(pixelValue);
    }

    red = grayScaleValue;
    green = grayScaleValue;
    blue = grayScaleValue;


    // If there is a color table, then pixelValue is in 24-bit color. It will be
    // translated when it is stored.
    if (m_pColorTable) {
        pixelValue = red;
        pixelValue = pixelValue << 8;
        pixelValue = pixelValue | green;
        pixelValue = pixelValue << 8;
        pixelValue = pixelValue | blue;
        goto abort;
    }


    switch (m_pBitMapHeader->bitsPerPixel) {
    //////////////////////////////////////////
    case 1:
    case 2:
    case 4:
    case 8:
        // These smaller values are indexes into a color table,
        // and the color table stores store the pixel as 8.8.8.0.8 RGBAX
        // format. See http://en.wikipedia.org/wiki/RGBAX#RGBAX for more.
        // The 5 numbers in 8.8.8.0.8 represent:
        //    Bits in Red
        //    Bits in Green
        //    Bits in Blue
        //    Bits in Alpha blending code
        //    Unused bits
        pixelValue = red;
        pixelValue = pixelValue << 8;
        pixelValue = pixelValue | green;
        pixelValue = pixelValue << 8;
        pixelValue = pixelValue | blue;
        break;

    //////////////////////////////////////////
    case 16:
        // This also uses the 5.5.5.0.1 RGBAX format.
        pixelValue = red;
        pixelValue = pixelValue << 5;
        pixelValue = pixelValue | green;
        pixelValue = pixelValue << 5;
        pixelValue = pixelValue | blue;

        break;

    //////////////////////////////////////////
    case 24:
        // This also uses the 8.8.8.0.8 RGBAX format.
        pixelValue = red;
        pixelValue = pixelValue << 8;
        pixelValue = pixelValue | green;
        pixelValue = pixelValue << 8;
        pixelValue = pixelValue | blue;
        break;

    //////////////////////////////////////////
    case 32:
        // I *think* this also uses the 8.8.8.0.8 RGBAX format, but there is
        // a valid value for the alpha blending.
        pixelValue = red;
        pixelValue = pixelValue << 8;
        pixelValue = pixelValue | green;
        pixelValue = pixelValue << 8;
        pixelValue = pixelValue | blue;
        break;

    //////////////////////////////////////////
    default:
        pixelValue = 0;
        break;
    } // switch (m_pBitMapHeader->bitsPerPixel)

abort:
    return(pixelValue);
} // ConvertGrayScaleToPixel




