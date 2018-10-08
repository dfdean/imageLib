https://trac.ffmpeg.org/wiki/Create%20a%20thumbnail%20image%20every%20X%20seconds%20of%20the%20video

wiki:Create a thumbnail image every X seconds of the video
-vframes option
Output a single frame from the video into an image file: 
ffmpeg -i input.flv -ss 00:00:14.435 -vframes 1 out.png

This example will seek to the position of 0h:0m:14sec:435msec and output one frame (-vframes 1) from that position into a PNG file. 
fps video filter
Output one image every second, named out1.png, out2.png, out3.png, etc. 
ffmpeg -i input.flv -vf fps=1 out%d.png

Output one image every minute, named img001.jpg, img002.jpg, img003.jpg, etc. The %03d dictates that the ordinal number of each output image will be formatted using 3 digits. 
ffmpeg -i myvideo.avi -vf fps=1/60 img%03d.jpg

Output one image every ten minutes: 
ffmpeg -i test.flv -vf fps=1/600 thumb%04d.bmp

select video filter
Output one image for every I-frame: 
ffmpeg -i input.flv -vf "select='eq(pict_type,PICT_TYPE_I)'" -vsync vfr thumb%04d.png






ffmpeg -i input.avi -vf fps=1 out%03d.bmp




ffmpeg -i Ceus_rat3_dean_Lumason_20170407_130558_clip.avi -vf fps=1 out%03d.bmp







You can do this from the command line with ffmpeg. See this part of the documentation. For example,

ffmpeg -i infile.avi -f image2 image-%03d.jpg
will save all frames from infile.avi as numbered jpegs (image-001.jpg, image-002.jpg,...). You can then use other command line options to get just the frames you want or do some other post processing like resizing or deinterlacing.



ffmpeg -i myfile.avi -f image2 image-%05d.bmp
to split myfile.avi into frames stored as .bmp files. It seemed to work except not quite. When recording my video, I recorded at a rate of 1000fps and the video turned out to be 2min29sec long. If my math is correct, that should amount to a total of 149,000 frames for the entire video. However, when I ran



ffmpeg -i myfile.avi -f image2 image-%05d.bmp



49
down vote
It's very simple with ffmpeg, and it can output one frame every N seconds without extra scripting. To export as an image sequence just use myimage_%04d.png or similar as the output. The %0xd bit is converted to a zero-padded integer x digits long - the example I gave gets output as myimage_0000.png myimage_0001.png etc.. You can use lots of still image formats, png, jpeg, tga, whatever (see ffmpeg -formats for a full list).

Ok so to export as an image sequence we can use an image format as the output with a sequential filename, but we don't want to export every single frame. So simply change the frame rate of the output to whatever we want using the -r n option where n is the number of frames per second. 1 frame per second would be -r 1, one frame every four seconds would be -r 0.25, one frame every ten seconds would be -r 0.1 and so on.

So to put it all together, this is how it would look to save one frame of input.mov every four seconds to output_0000.png, output_0001.png etc.:

ffmpeg -i input.mov -r 0.25 output_%04d.png
Change the %xd to however many digits you need, e.g. if the command would create more than 10,000 frames change the %04d to %05d. This also works for input files that are image sequence. Read more here.

Windows users: On the command line use %

example: ffmpeg -i inputFile.mp4 -r 1 outputFile_%02d.png

In CMD and BAT Scripts use %%

example: ffmpeg -i inputFile.mp4 -r 1 outputFile %%02d.png

Don't use just one % in scripts, and don't use %% in command line. Both situations will generate an error.





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
// AVI File Parser
//
// For a useful description, see the following:
//    https://en.wikipedia.org/wiki/Audio_Video_Interleave
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


////////////////////////////////////////////////////////////////////////////////
//
//                           VIDEO OBJECTS
//
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////
// This is a video object that contains a series of frames.
class CSimpleMovieAPI {
public:
    virtual void Close() = 0;
    virtual ErrVal GoToFrame(int32 frameNum) = 0;
}; // CSimpleMovieAPI


ErrVal OpenMovieFromFile(
                const char *pImageFileName,
                int32 options, 
                CSimpleMovieAPI **ppResult);
void DeleteMovieObject(CSimpleMovieAPI *pParserInterface);





////////////////////////////////////////////////
// This is a video object that contains a series of frames.
class CAVIMovie : public CSimpleMovieAPI {
public:
    NEWEX_IMPL()
    CAVIMovie();
    virtual ~CAVIMovie();
    ErrVal ReadMovieFile(const char *pFilePath, int32 options);
    ErrVal InitializeForNewFile(const char *pFilePath);

    // CSimpleMovieAPI
    virtual void Close();
    virtual ErrVal GoToFrame(int32 frameNum);

private:
    enum CAVIConstants {
        FILE_TYPE_UNKNOWN   = -1,
        FILE_TYPE_AVI       = 0,
    };

    virtual ErrVal GoToFilePosition(int64 position, int32 minBytes);

    // The file. This is optional, and may be NULL if this is a 
    // memory-only object.
    CSimpleFile             m_File;
    char                    *m_pFilePathName;
    uint64                  m_FileLength;

    // The actual contents.
    char                    *m_pBuffer;
    int32                   m_BufferLength;
    int32                   m_NumValidBytesInBuffer;
    int64                   m_BufferPosInFile;
    int32                   m_ReadChunkSize;
    int64                   m_ReadChunkMask;

    // Pointers into m_pBuffer with the parsed sections.
    char                    *m_pPtr;
    char                    *m_pEndValidBytes;

    // The list of frames
    int32                   m_FileType;
    uint64                  m_RIFFChunkPosInFile;
    uint64                  m_MovieHeaderChunkPosInFile;
    uint64                  m_FrameIndexChunkPosInFile;
    uint64                  m_FirstFrameChunkPosInFile;

    // Information about each frame
    int32                   m_MicroSecPerFrame;
    int32                   m_FileSizeIncrement;
    int32                   m_TotalNumFrames;
    int32                   m_FrameWidth;
    int32                   m_FrameHeight;
}; // CAVIMovie






///////////////////////////////////////////////////////
// All chunks have the following format:
class CRIFFChunkHeader {
public:
    int32   m_ChunkType;
    int32   m_ChunkLength; // Little-endian
    // Note, the chunk is followed by an optional pad byte, if m_ChunkLength is not even.
}; // CRIFFChunkHeader



///////////////////////////////////////////////////////
// Some chunks contain sub-chunks. A list of subchunks starts with this.
class CSubChunkListHeader {
public:
    int32   m_SubChunkListType; // "AVI " or "WAVE"
}; // CSubChunkListHeader


///////////////////////////////////////////////////////
// This is the contents of a movie header sub-chunk, type "hdrl".
class CMovieFrameListHeader {
public:
    int32   m_ChunkType;
    int32   m_ChunkLength; // Little-endian
    int32   dwMicroSecPerFrame; //
    int32   dwMaxBytesPerSec; // max transfer rate
    int32   dwPaddingGranularity; // pad the entire file to a multiple of this size
    int32   dwFlags; // 
    int32   dwTotalFrames; // Number of frames in RIFF-AVI list. Not the number of frames in the file
    int32   dwInitialFrames; // Ignored
    int32   dwStreams; // Number of streams in the file
    int32   dwSuggestedBufferSize; // Ignore
    int32   dwWidth;
    int32   dwHeight;
    int32   dwReserved[4];
}; // CMovieFrameListHeader





/////////////////////////////////////////////////////////////////////////////
//
// [OpenMovieFromFile]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
OpenMovieFromFile(
                const char *pFilePath,
                int32 options, 
                CSimpleMovieAPI **ppResult) {
    ErrVal err = ENoErr;
    CAVIMovie *pParser = NULL;

    if (NULL == ppResult) {
        gotoErr(EFail);
    }

    pParser = newex CAVIMovie;
    if (NULL == pParser) {
        gotoErr(EFail);
    }

    err = pParser->ReadMovieFile(pFilePath, options);
    if (err) {
        gotoErr(err);
    }

    *ppResult = pParser;
    pParser = NULL;

abort:
    delete pParser;
    returnErr(err);
} // OpenMovieFromFile




/////////////////////////////////////////////////////////////////////////////
//
// [DeleteMovieObject]
//
/////////////////////////////////////////////////////////////////////////////
void
DeleteMovieObject(CSimpleMovieAPI *pParserInterface) {
    CAVIMovie *pParser = (CAVIMovie *) pParserInterface;

    if (pParser) {
        pParser->Close();
        delete pParser;
    }
} // DeleteImageObject





/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
CAVIMovie::CAVIMovie() {
    m_pFilePathName = NULL;
    m_FileLength = 0;

    m_pBuffer = NULL;
    m_BufferLength = 0;
    m_BufferPosInFile = 0;
    m_NumValidBytesInBuffer = -1;
    m_ReadChunkSize = 4 * 1024;
    m_ReadChunkMask = m_ReadChunkSize - 1;
    m_pPtr = NULL;
    m_pEndValidBytes = NULL;

    m_RIFFChunkPosInFile = 0;
    m_MovieHeaderChunkPosInFile = 0;
    m_FrameIndexChunkPosInFile = 0;
    m_FirstFrameChunkPosInFile = 0;
    m_FileType = FILE_TYPE_UNKNOWN;

    m_MicroSecPerFrame = 0;
    m_FileSizeIncrement = 0;
    m_TotalNumFrames = 0;
    m_FrameWidth = 0;
    m_FrameHeight = 0;
} // CAVIMovie





/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
CAVIMovie::~CAVIMovie() {
    Close();
}




/////////////////////////////////////////////////////////////////////////////
//
// [ReadMovieFile]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
CAVIMovie::ReadMovieFile(const char *pFilePath, int32 options) {
    ErrVal err = ENoErr;
    uint64 position;
    int32 minBytes;
    CRIFFChunkHeader *pCurrentChunk;
    CSubChunkListHeader *pChunkListHeader;
    char typeStr[6];
    UNUSED_PARAM(options);


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

    m_BufferLength = 128 * 1024;
    m_BufferPosInFile = 0;
    m_NumValidBytesInBuffer = -1;
    m_pBuffer = (char *) memAlloc(m_BufferLength);
    if (NULL == m_pBuffer) {
        gotoErr(EFail);
    }

    ////////////////////////////////////////////////////////
    // Read every chunk in the file until we hit the chunk with the content.
    // A typical .AVI file may contain a single "RIFF" chunk, which un turn contains subchunks
    position = 0;
    m_RIFFChunkPosInFile = 0;
    m_MovieHeaderChunkPosInFile = 0;
    m_FrameIndexChunkPosInFile = 0;
    m_FirstFrameChunkPosInFile = 0;
    while (position < m_FileLength) {
        minBytes = sizeof(CRIFFChunkHeader);
        err = GoToFilePosition(position, minBytes);
        if (err) {
            gotoErr(err);
        }
        pCurrentChunk = (CRIFFChunkHeader *) m_pPtr;
   
        // Multi-character constants will generate warnings in g++ because they are implementation-dependant.
        // So, convert this to a C-string.
        typeStr[0] = ((pCurrentChunk->m_ChunkType) & 0x000000FF);
        typeStr[1] = ((pCurrentChunk->m_ChunkType) & 0x0000FF00) >> 8;
        typeStr[2] = ((pCurrentChunk->m_ChunkType) & 0x00FF0000) >> 16;
        typeStr[3] = ((pCurrentChunk->m_ChunkType) & 0xFF000000) >> 24;
        typeStr[4] = 0;

        // Chunks of type "RIFF" and "LIST" contain subchunks.
        // Multi-character constants will generate warnings in g++ because they are implementation-dependant.
        if (0 == strcasecmpex(typeStr, "RIFF")) {
            m_RIFFChunkPosInFile = position + sizeof(CRIFFChunkHeader);
            break;
        } else if (0 == strcasecmpex(typeStr, "LIST")) {
            //break;
        }

        // Othwewise, skip this chunk and go to the next chunk.
        position = position + sizeof(CRIFFChunkHeader) + pCurrentChunk->m_ChunkLength; // Little-endian
        // Note, the chunk is followed by an optional pad byte, if m_ChunkLength is not even.
        if ((pCurrentChunk->m_ChunkLength) & 0x0001) {
            position += 1;
        }
    } // while (position < m_FileLength)

    // If we didn't find a RIFF chunk, then the file is invalid.
    if (m_RIFFChunkPosInFile <= 0) {
        gotoErr(EFail);
    }


    ////////////////////////////////////////////////////////
    // Read the start of the subchunk list. This is a special subchunk-list header.
    m_FileType = FILE_TYPE_UNKNOWN;
    minBytes = sizeof(CSubChunkListHeader);
    err = GoToFilePosition(m_RIFFChunkPosInFile, minBytes);
    if (err) {
        gotoErr(err);
    }
    pChunkListHeader = (CSubChunkListHeader *) m_pPtr;

    // Multi-character constants will generate warnings in g++ because they are implementation-dependant.
    // So, convert this to a C-string.
    typeStr[0] = ((pChunkListHeader->m_SubChunkListType) & 0x000000FF);
    typeStr[1] = ((pChunkListHeader->m_SubChunkListType) & 0x0000FF00) >> 8;
    typeStr[2] = ((pChunkListHeader->m_SubChunkListType) & 0x00FF0000) >> 16;
    typeStr[3] = ((pChunkListHeader->m_SubChunkListType) & 0xFF000000) >> 24;
    typeStr[4] = 0;
    // "AVI " or "WAVE"
    if (0 == strcasecmpex(typeStr, "AVI ")) {
        m_FileType = FILE_TYPE_AVI;
    } else {
        gotoErr(EFail);
    }

    // Skip the sub-chunk list header.
    m_RIFFChunkPosInFile = m_RIFFChunkPosInFile + sizeof(CSubChunkListHeader);



    ////////////////////////////////////////////////////////
    // Read every sub-chunk in the AVI chunk until we hit the first frame.
    position = m_RIFFChunkPosInFile;
    while (position < m_FileLength) {
        minBytes = sizeof(CRIFFChunkHeader) + sizeof(CMovieFrameListHeader) + sizeof(CSubChunkListHeader);
        err = GoToFilePosition(position, minBytes);
        if (err) {
            gotoErr(err);
        }
        pCurrentChunk = (CRIFFChunkHeader *) m_pPtr;
   
        // Multi-character constants will generate warnings in g++ because they are implementation-dependant.
        // So, convert this to a C-string.
        typeStr[0] = ((pCurrentChunk->m_ChunkType) & 0x000000FF);
        typeStr[1] = ((pCurrentChunk->m_ChunkType) & 0x0000FF00) >> 8;
        typeStr[2] = ((pCurrentChunk->m_ChunkType) & 0x00FF0000) >> 16;
        typeStr[3] = ((pCurrentChunk->m_ChunkType) & 0xFF000000) >> 24;
        typeStr[4] = 0;

        //////////////////////////
        // Chunks of type "RIFF" and "LIST" contain subchunks.
        if (0 == strcasecmpex(typeStr, "LIST")) {
            char *pSubChunkPtr = m_pPtr;
            pSubChunkPtr += sizeof(CRIFFChunkHeader);
            pChunkListHeader = (CSubChunkListHeader *) pSubChunkPtr;

            // Multi-character constants will generate warnings in g++ because they are implementation-dependant.
            // So, convert this to a C-string.
            typeStr[0] = ((pChunkListHeader->m_SubChunkListType) & 0x000000FF);
            typeStr[1] = ((pChunkListHeader->m_SubChunkListType) & 0x0000FF00) >> 8;
            typeStr[2] = ((pChunkListHeader->m_SubChunkListType) & 0x00FF0000) >> 16;
            typeStr[3] = ((pChunkListHeader->m_SubChunkListType) & 0xFF000000) >> 24;
            typeStr[4] = 0;

            // The "hdrl" sub-chunk is metadata such as frame width and height
            if (0 == strcasecmpex(typeStr, "hdrl")) {
                // Skip the sub-chunk list header.
                char *pHdrPtr = pSubChunkPtr + sizeof(CSubChunkListHeader);
                CMovieFrameListHeader *pFrameListHeader = (CMovieFrameListHeader *) pHdrPtr;

                // Multi-character constants will generate warnings in g++ because they are implementation-dependant.
                // So, convert this to a C-string.
                typeStr[0] = ((pFrameListHeader->m_ChunkType) & 0x000000FF);
                typeStr[1] = ((pFrameListHeader->m_ChunkType) & 0x0000FF00) >> 8;
                typeStr[2] = ((pFrameListHeader->m_ChunkType) & 0x00FF0000) >> 16;
                typeStr[3] = ((pFrameListHeader->m_ChunkType) & 0xFF000000) >> 24;
                typeStr[4] = 0;

                m_MicroSecPerFrame = pFrameListHeader->dwMicroSecPerFrame;
                m_FileSizeIncrement = pFrameListHeader->dwPaddingGranularity;
                m_TotalNumFrames = pFrameListHeader->dwTotalFrames;
                m_FrameWidth = pFrameListHeader->dwWidth;
                m_FrameHeight = pFrameListHeader->dwHeight;
            }
        //////////////////////////
        // The "idx1" sub-chunk is offsets to the data chunks within the file.
        } else if (0 == strcasecmpex(typeStr, "idx1")) {
            m_FrameIndexChunkPosInFile = position + sizeof(CRIFFChunkHeader);
        //////////////////////////
        // The "movi" sub-chunk is the actual frames of audio and visual data.
        } else if (0 == strcasecmpex(typeStr, "movi")) {
            m_FirstFrameChunkPosInFile = position + sizeof(CRIFFChunkHeader);
            break;
        }

        // Othwewise, skip this chunk and go to the next chunk.
        position = position + sizeof(CRIFFChunkHeader) + pCurrentChunk->m_ChunkLength; // Little-endian
        // Note, the chunk is followed by an optional pad byte, if m_ChunkLength is not even.
        if ((pCurrentChunk->m_ChunkLength) & 0x0001) {
            position += 1;
        }
    } // while (position < m_FileLength)


// An AVI file may carry audio/visual data inside the chunks in virtually any compression scheme, including Full Frame (Uncompressed), Intel Real Time (Indeo), Cinepak, Motion JPEG, Editable MPEG, VDOWave, ClearVideo / RealVideo, QPEG, and MPEG-4 Video.


abort:
    returnErr(err);
} // ReadImageFile








/////////////////////////////////////////////////////////////////////////////
//
// [InitializeForNewFile]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
CAVIMovie::InitializeForNewFile(const char *pFilePath) {
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
// [GoToFilePosition]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
CAVIMovie::GoToFilePosition(int64 position, int32 minBytes) {
    ErrVal err = ENoErr;
    int64 stopPosition = position + minBytes;
    int64 readSize;
    int32 offsetFromBufferStart;

    // Check if we already have the desired bytes in the buffer.
    if ((m_pPtr)
            && (m_pEndValidBytes)
            && (position >= m_BufferPosInFile) 
            && (stopPosition < (m_BufferPosInFile + m_NumValidBytesInBuffer))) {
        offsetFromBufferStart = position - m_BufferPosInFile;
        m_pPtr = m_pBuffer + offsetFromBufferStart;
        returnErr(ENoErr);
    }

    // Otherwise, goto to the start of the buffer.
    m_BufferPosInFile = (position & m_ReadChunkMask);
    readSize = m_FileLength - m_BufferPosInFile;
    if (readSize > m_BufferLength) {
        readSize = m_BufferLength;
    }

    err = m_File.Seek(m_BufferPosInFile, CSimpleFile::SEEK_START);
    if (err) {
        gotoErr(err);
    }
    err = m_File.Read(m_pBuffer, (int32) readSize, &m_NumValidBytesInBuffer);
    if (err) {
        gotoErr(err);
    }

    offsetFromBufferStart = position - m_BufferPosInFile;
    m_pPtr = m_pBuffer + offsetFromBufferStart;
    m_pEndValidBytes = m_pBuffer + m_NumValidBytesInBuffer;


abort:
    returnErr(err);
} // GoToFilePosition






/////////////////////////////////////////////////////////////////////////////
//
// [Close]
//
// CSimpleMovieAPI
/////////////////////////////////////////////////////////////////////////////
void
CAVIMovie::Close() {
    memFree(m_pFilePathName);
    m_pFilePathName = NULL;
    m_FileLength = 0;

    memFree(m_pBuffer);
    m_pBuffer = NULL;
    m_BufferLength = 0;
    m_BufferPosInFile = 0;
    m_NumValidBytesInBuffer = -1;

    m_pPtr = NULL;
    m_pEndValidBytes = NULL;

    m_File.Close();
} // Close





    

/////////////////////////////////////////////////////////////////////////////
//
// [GoToFrame]
//
// CSimpleMovieAPI
/////////////////////////////////////////////////////////////////////////////
ErrVal
CAVIMovie::GoToFrame(int32 frameNum) {
    ErrVal err = ENoErr;
    UNUSED_PARAM(frameNum);

    returnErr(err);
} // GoToFrame



