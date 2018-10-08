////////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2010-2018 Dawson Dean
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
// Image Analyzer
//
/////////////////////////////////////////////////////////////////////////////

#if WASM
#include "portableBuildingBlocks.h"
#else
#include "buildingBlocks.h"
#endif

#include "imageLib.h"
#include "imageLibInternal.h"

FILE_DEBUGGING_GLOBALS(LOG_LEVEL_DEFAULT, 0);


#define WRITE_ALWAYS                    1
#define WRITE_IFF_NEED_MORE_SPACE       2

#define MAX_POINTS_PER_POLYGON          4



///////////////////////////////////////////////////////
class CVertex {
public:
    NEWEX_IMPL()

    int32       m_x;
    int32       m_y;
    int32       m_z;

    int32       m_Red;
    int32       m_Blue;
    int32       m_Green;

    int32       m_index;

    CVertex     *m_pNext;
}; // CVertex




///////////////////////////////////////////////////////
class CLine {
public:
    NEWEX_IMPL()

    int32       m_Point1;
    int32       m_Point2;

    int32       m_Red;
    int32       m_Blue;
    int32       m_Green;

    CLine       *m_pNextLine;
}; // CLine



///////////////////////////////////////////////////////
class CPolygon {
public:
    NEWEX_IMPL()

    int32       m_NumPoints;
    int32       *m_PointList;
    int32       m_BuiltInPointList[MAX_POINTS_PER_POLYGON];

    int32       m_Red;
    int32       m_Blue;
    int32       m_Green;

    CPolygon    *m_pNextPolygon;
}; // CPolygon




///////////////////////////////////////////////////////
class CPLY3DModelFile : public C3DModelFile {
public:
    NEWEX_IMPL()
    CPLY3DModelFile();
    virtual ~CPLY3DModelFile();

    /////////////////////////////
    // class C3DModelFile
    virtual void Close();
    virtual void CloseOnDiskOnly();
    virtual ErrVal Save();

    virtual ErrVal AddVertex(int32 x, int32 y, int32 z, int32 index);
    virtual ErrVal AddColoredVertex(int32 x, int32 y, int32 z, int32 index, int32 red, int32 blue, int32 green);
    virtual ErrVal AddLine(int32 numPoints, int32 pointID1, int32 pointID2);
    virtual ErrVal AddColoredLine(int32 pointID1, int32 pointID2, int32 red, int32 blue, int32 green);
    virtual ErrVal AddPolygon(int32 numPoints, int32 pointID1, int32 pointID2, int32 pointID3, int32 pointID4);
    virtual ErrVal AddColoredPolygon(int32 numPoints, int32 pointID1, int32 pointID2, int32 pointID3, int32 pointID4, int32 red, int32 blue, int32 green);
    virtual ErrVal StartPolygon(int32 numPoints);
    virtual ErrVal AddPointToPolygon(int32 index, int32 pointID);
    
    virtual ErrVal InitializeForNewFile(const char *pFilePath);

private:
    ErrVal WriteToFile(int32 opCode, int32 neededSpace);
    
    CSimpleFile         m_File;

    CVertex             *m_pVertexList;
    CVertex             *m_pLastVertex;
    int32               m_NumVertices;

    CLine               *m_pLineList;
    int32               m_NumLines;

    CPolygon            *m_pPolygonList;
    int32               m_NumPolygons;
    CPolygon            *m_pCurrentPolygon;

    char                *m_pBuffer;
    uint32              m_MaxBufferSize;
    char                *m_pDestPtr;
    char                *m_pEndDestPtr;
}; // CPLY3DModelFile




/////////////////////////////////////////////////////////////////////////////
//
// [CreateNewPLYFile]
//
/////////////////////////////////////////////////////////////////////////////
C3DModelFile *
CreateNewPLYFile(const char *pFilePath) {
    ErrVal err = ENoErr;
    CPLY3DModelFile *pParser = NULL;
    
    if (NULL == pFilePath) {
        gotoErr(EFail);
    }

    pParser = newex CPLY3DModelFile;
    if (NULL == pParser) {
        gotoErr(EFail);
    }

    err = pParser->InitializeForNewFile(pFilePath);
    if (err) {
        gotoErr(err);
    }

    return(pParser);

abort:
    delete pParser;
    return(NULL);
} // CreateNewPLYFile






/////////////////////////////////////////////////////////////////////////////
//
// [Delete3DFileRuntimeState]
//
/////////////////////////////////////////////////////////////////////////////
void
Delete3DFileRuntimeState(C3DModelFile *pFile) {    
    if (pFile) {
        CPLY3DModelFile *pParser = (CPLY3DModelFile *) pFile;
        delete pParser;
    }
} // Delete3DFileRuntimeState






/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
CPLY3DModelFile::CPLY3DModelFile() {
    m_pVertexList = NULL;
    m_pLastVertex = NULL;
    m_NumVertices = 0;
              
    m_pLineList = NULL;
    m_NumLines = 0;

    m_pPolygonList = NULL;
    m_NumPolygons = 0;
    m_pCurrentPolygon = NULL;

    m_pBuffer = NULL;
    m_MaxBufferSize = 0;

    m_pDestPtr = NULL;
    m_pEndDestPtr = NULL;
} // CPLY3DModelFile






/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
CPLY3DModelFile::~CPLY3DModelFile() {
    Close();
}




/////////////////////////////////////////////////////////////////////////////
//
// [AddVertex]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
CPLY3DModelFile::AddVertex(int32 x, int32 y, int32 z, int32 index) {
    ErrVal err = AddColoredVertex(x, y, z, index, 255, 0, 0);
    return(err);
}




/////////////////////////////////////////////////////////////////////////////
//
// [AddColoredVertex]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
CPLY3DModelFile::AddColoredVertex(int32 x, int32 y, int32 z, int32 index, int32 red, int32 blue, int32 green) {
    ErrVal err = ENoErr;
    CVertex *pVertex;

    pVertex = newex CVertex;
    if (NULL == pVertex) {
        gotoErr(EFail);
    }

    pVertex->m_x = x;
    pVertex->m_y = y;
    pVertex->m_z = z;
    
    pVertex->m_Red = red;
    pVertex->m_Blue = blue;
    pVertex->m_Green = green;

    pVertex->m_index = index;

    // Add this to the END of the vertex list.
    // The Vertex ID's index this list in order, so we must preserve the order.
    pVertex->m_pNext = NULL;
    if (m_pLastVertex) {
        m_pLastVertex->m_pNext = pVertex;
    }
    m_pLastVertex = pVertex;
    if (NULL == m_pVertexList) {
        m_pVertexList = pVertex;
    }
    m_NumVertices += 1;

abort:
    returnErr(err);
} // AddColoredVertex





              


/////////////////////////////////////////////////////////////////////////////
//
// [AddLine]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
CPLY3DModelFile::AddLine(int32 numPoints, int32 pointID1, int32 pointID2) {
    ErrVal err = ENoErr;
    UNUSED_PARAM(numPoints);
    
    err = AddColoredLine(pointID1, pointID2, 255, 255, 255);
    returnErr(err);
} // AddLine





/////////////////////////////////////////////////////////////////////////////
//
// [AddColoredLine]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
CPLY3DModelFile::AddColoredLine(int32 pointID1, int32 pointID2, int32 red, int32 blue, int32 green) {
    ErrVal err = ENoErr;
    CLine *pLine;

    pLine = newex CLine;
    if (NULL == pLine) {
        gotoErr(EFail);
    }

    pLine->m_Point1 = pointID1;
    pLine->m_Point2 = pointID2;

    pLine->m_Red = red;
    pLine->m_Blue = blue;
    pLine->m_Green = green;

    pLine->m_pNextLine = m_pLineList;
    m_pLineList = pLine;
    m_NumLines += 1;

abort:
    returnErr(err);
} // AddColoredLine






/////////////////////////////////////////////////////////////////////////////
//
// [AddPolygon]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
CPLY3DModelFile::AddPolygon(int32 numPoints, int32 pointID1, int32 pointID2, int32 pointID3, int32 pointID4) {
    ErrVal err = ENoErr;
    err = AddColoredPolygon(numPoints, pointID1, pointID2, pointID3, pointID4, 255, 255, 255);
    returnErr(err);
} // AddPolygon




/////////////////////////////////////////////////////////////////////////////
//
// [AddColoredPolygon]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
CPLY3DModelFile::AddColoredPolygon(int32 numPoints, int32 pointID1, int32 pointID2, int32 pointID3, int32 pointID4, int32 red, int32 blue, int32 green) {
    ErrVal err = ENoErr;
    CPolygon *pPolygon;

    pPolygon = newex CPolygon;
    if (NULL == pPolygon) {
        gotoErr(EFail);
    }

    pPolygon->m_PointList = &(pPolygon->m_BuiltInPointList[0]);
    pPolygon->m_NumPoints = numPoints;
    pPolygon->m_PointList[0] = pointID1;
    pPolygon->m_PointList[1] = pointID2;
    pPolygon->m_PointList[2] = pointID3;
    pPolygon->m_PointList[3] = pointID4;

    pPolygon->m_Red = red;
    pPolygon->m_Blue = blue;
    pPolygon->m_Green = green;

    pPolygon->m_pNextPolygon = m_pPolygonList;
    m_pPolygonList = pPolygon;
    m_NumPolygons += 1;

abort:
    returnErr(err);
} // AddColoredPolygon






/////////////////////////////////////////////////////////////////////////////
//
// [StartPolygon]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
CPLY3DModelFile::StartPolygon(int32 numPoints)
{
    ErrVal err = ENoErr;

    m_pCurrentPolygon = newex CPolygon;
    if (NULL == m_pCurrentPolygon) {
        gotoErr(EFail);
    }

    m_pCurrentPolygon->m_NumPoints = numPoints;
    m_pCurrentPolygon->m_PointList = (int32 *) (memAlloc(sizeof(int32) * numPoints));
    if (NULL == m_pCurrentPolygon->m_PointList) {
        gotoErr(EFail);
    }
    
    m_pCurrentPolygon->m_Red = 255;
    m_pCurrentPolygon->m_Blue = 255;
    m_pCurrentPolygon->m_Green = 255;

    m_pCurrentPolygon->m_pNextPolygon = m_pPolygonList;
    m_pPolygonList = m_pCurrentPolygon;
    m_NumPolygons += 1;

abort:
    returnErr(err);
} // StartPolygon





/////////////////////////////////////////////////////////////////////////////
//
// [StartPolygon]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
CPLY3DModelFile::AddPointToPolygon(int32 index, int32 pointID)
{
    ErrVal err = ENoErr;
    if (m_pCurrentPolygon) {
        m_pCurrentPolygon->m_PointList[index] = pointID;
    }
    returnErr(err);
} // StartPolygon





/////////////////////////////////////////////////////////////////////////////
//
// [InitializeForNewFile]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
CPLY3DModelFile::InitializeForNewFile(const char *pFilePath)
{
    ErrVal err = ENoErr;

    Close();

    if (pFilePath) {
        CSimpleFile::DeleteFile(pFilePath);
        err = m_File.OpenOrCreateEmptyFile(pFilePath, 0);
        if (err) {
            gotoErr(err);
        }
    }

abort:
    returnErr(err);
} // InitializeForNewFile





/////////////////////////////////////////////////////////////////////////////
//
// [Close]
//
/////////////////////////////////////////////////////////////////////////////
void
CPLY3DModelFile::Close()
{
    CVertex *pNextVertex;
    CPolygon *pNextPolygon;


    if (m_pBuffer) {
        memFree(m_pBuffer);
        m_pBuffer = NULL;
    }
    m_MaxBufferSize = 0;

    while (m_pVertexList) {
        pNextVertex = m_pVertexList->m_pNext;
        delete m_pVertexList;
        m_pVertexList = pNextVertex;
    } // while (m_pVertexList)
    
    while (m_pPolygonList) {
        pNextPolygon = m_pPolygonList->m_pNextPolygon;
        delete m_pPolygonList;
        m_pPolygonList = pNextPolygon;
    } // while (m_pPolygonList)
    
    m_File.Close();
} // Close





/////////////////////////////////////////////////////////////////////////////
//
// [CloseOnDiskOnly]
//
/////////////////////////////////////////////////////////////////////////////
void
CPLY3DModelFile::CloseOnDiskOnly() {
    m_File.Close();
} // CloseOnDiskOnly




/////////////////////////////////////////////////////////////////////////////
//
// [Save]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
CPLY3DModelFile::Save()
{
    ErrVal err = ENoErr;
    CVertex *pVertex;
    CLine *pLine;
    CPolygon *pPolygon;
    int32 index;


    if (!(m_File.IsOpen())) {
        gotoErr(ENoErr);
    }    
    err = m_File.Seek(0, CSimpleFile::SEEK_START);
    if (err) {
        gotoErr(err);
    }


    m_MaxBufferSize = 16000;
    m_pBuffer = (char *) memAlloc(m_MaxBufferSize);
    if (NULL == m_pBuffer) {
        gotoErr(EFail);
    }
    m_pDestPtr = m_pBuffer;
    m_pEndDestPtr = m_pBuffer + m_MaxBufferSize;

    // Global file headers.
    m_pDestPtr += snprintf(m_pDestPtr, (m_pEndDestPtr - m_pDestPtr), "ply\n");
    m_pDestPtr += snprintf(m_pDestPtr, (m_pEndDestPtr - m_pDestPtr), "format ascii 1.0\n");
    
    // Declare the vertices.
    m_pDestPtr += snprintf(m_pDestPtr, (m_pEndDestPtr - m_pDestPtr), "element vertex %d\n", m_NumVertices);
    m_pDestPtr += snprintf(m_pDestPtr, (m_pEndDestPtr - m_pDestPtr), "property float x\n");
    m_pDestPtr += snprintf(m_pDestPtr, (m_pEndDestPtr - m_pDestPtr), "property float y\n");
    m_pDestPtr += snprintf(m_pDestPtr, (m_pEndDestPtr - m_pDestPtr), "property float z\n");
    m_pDestPtr += snprintf(m_pDestPtr, (m_pEndDestPtr - m_pDestPtr), "property uchar red\n");
    m_pDestPtr += snprintf(m_pDestPtr, (m_pEndDestPtr - m_pDestPtr), "property uchar green\n");
    m_pDestPtr += snprintf(m_pDestPtr, (m_pEndDestPtr - m_pDestPtr), "property uchar blue\n");

    // Declare the Lines.
    if (m_NumLines > 0) {
        m_pDestPtr += snprintf(m_pDestPtr, (m_pEndDestPtr - m_pDestPtr), "element edge %d\n", m_NumLines);
        m_pDestPtr += snprintf(m_pDestPtr, (m_pEndDestPtr - m_pDestPtr), "property int vertex1\n");
        m_pDestPtr += snprintf(m_pDestPtr, (m_pEndDestPtr - m_pDestPtr), "property int vertex2\n");
        m_pDestPtr += snprintf(m_pDestPtr, (m_pEndDestPtr - m_pDestPtr), "property uchar red\n");
        m_pDestPtr += snprintf(m_pDestPtr, (m_pEndDestPtr - m_pDestPtr), "property uchar green\n");
        m_pDestPtr += snprintf(m_pDestPtr, (m_pEndDestPtr - m_pDestPtr), "property uchar blue\n");
    }    

    // Declare the polygons.
    if (m_NumPolygons > 0) {
        m_pDestPtr += snprintf(m_pDestPtr, (m_pEndDestPtr - m_pDestPtr), "element face %d\n", m_NumPolygons);
        m_pDestPtr += snprintf(m_pDestPtr, (m_pEndDestPtr - m_pDestPtr), "property list uchar int vertex_index\n");
        m_pDestPtr += snprintf(m_pDestPtr, (m_pEndDestPtr - m_pDestPtr), "property uchar red\n");
        m_pDestPtr += snprintf(m_pDestPtr, (m_pEndDestPtr - m_pDestPtr), "property uchar green\n");
        m_pDestPtr += snprintf(m_pDestPtr, (m_pEndDestPtr - m_pDestPtr), "property uchar blue\n");
    }    

    // Close the header. Data starts after this line.
    m_pDestPtr += snprintf(m_pDestPtr, (m_pEndDestPtr - m_pDestPtr), "end_header\n");

    // Write each vertex.
    pVertex = m_pVertexList;
    while (pVertex) {
        err = WriteToFile(WRITE_IFF_NEED_MORE_SPACE, 200);
        if (err) {
            gotoErr(err);
        }
        m_pDestPtr += snprintf(m_pDestPtr, (m_pEndDestPtr - m_pDestPtr), "%d %d %d %d %d %d\n", 
                        pVertex->m_x, pVertex->m_y, pVertex->m_z,
                        pVertex->m_Red, pVertex->m_Blue, pVertex->m_Green);

        pVertex = pVertex->m_pNext;
    } // while (pVertex)


    // Write each Line.
    pLine = m_pLineList;
    while (pLine) {
        err = WriteToFile(WRITE_IFF_NEED_MORE_SPACE, 200);
        if (err) {
            gotoErr(err);
        }

        m_pDestPtr += snprintf(
                        m_pDestPtr, 
                        (m_pEndDestPtr - m_pDestPtr), 
                        "%d %d %d %d %d\n", 
                        pLine->m_Point1, 
                        pLine->m_Point2,
                        pLine->m_Red, pLine->m_Blue, pLine->m_Green);


        pLine = pLine->m_pNextLine;
    } // while (pLine)    
    

    // Write each polygon.
    pPolygon = m_pPolygonList;
    while (pPolygon) {
        err = WriteToFile(WRITE_IFF_NEED_MORE_SPACE, 200);
        if (err) {
            gotoErr(err);
        }

        if (3 == pPolygon->m_NumPoints) {
            m_pDestPtr += snprintf(
                            m_pDestPtr, 
                            (m_pEndDestPtr - m_pDestPtr), 
                            "%d %d %d %d %d %d %d\n", 
                            pPolygon->m_NumPoints, 
                            pPolygon->m_PointList[0], 
                            pPolygon->m_PointList[1], 
                            pPolygon->m_PointList[2],
                            pPolygon->m_Red, pPolygon->m_Blue, pPolygon->m_Green);
        } else if (4 == pPolygon->m_NumPoints) {
            m_pDestPtr += snprintf(
                            m_pDestPtr, 
                            (m_pEndDestPtr - m_pDestPtr), 
                            "%d %d %d %d %d %d %d %d\n", 
                            pPolygon->m_NumPoints, 
                            pPolygon->m_PointList[0], 
                            pPolygon->m_PointList[1], 
                            pPolygon->m_PointList[2], 
                            pPolygon->m_PointList[3],
                            pPolygon->m_Red, pPolygon->m_Blue, pPolygon->m_Green);
        } else {
            m_pDestPtr += snprintf(
                            m_pDestPtr, 
                            (m_pEndDestPtr - m_pDestPtr), 
                            "%d", 
                            pPolygon->m_NumPoints);
            for (index = 0; index < pPolygon->m_NumPoints; index++) {
                m_pDestPtr += snprintf(
                                m_pDestPtr, 
                                (m_pEndDestPtr - m_pDestPtr), 
                                " %d",  
                                pPolygon->m_PointList[index]);
            }
             m_pDestPtr += snprintf(m_pDestPtr, (m_pEndDestPtr - m_pDestPtr), " %d %d %d\n", 
                                pPolygon->m_Red, pPolygon->m_Blue, pPolygon->m_Green);
        }

        pPolygon = pPolygon->m_pNextPolygon;
    } // while (pPolygon)    


    // Flush the buffer to save any remaining bytes that were not written.
    err = WriteToFile(WRITE_ALWAYS, 0);
    if (err) {
        gotoErr(err);
    }

abort:
    memFree(m_pBuffer);
    m_pBuffer = NULL;

    returnErr(err);
} // Save






/////////////////////////////////////////////////////////////////////////////
//
// [WriteToFile]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
CPLY3DModelFile::WriteToFile(int32 opCode, int32 neededSpace) {
    ErrVal err = ENoErr;
    int32 dataLength;

    if ((WRITE_IFF_NEED_MORE_SPACE == opCode)
        && ((m_pDestPtr + neededSpace) < m_pEndDestPtr)) {
        gotoErr(ENoErr);
    }

    err = m_File.Seek(0, CSimpleFile::SEEK_FILE_END);
    if (err) {
        gotoErr(err);
    }

    dataLength = (int32) (m_pDestPtr - m_pBuffer);
    err = m_File.Write(m_pBuffer, (int32) dataLength);
    if (err) {
        gotoErr(err);
    }
    err = m_File.Flush();
    if (err) {
        gotoErr(err);
    }

    m_pDestPtr = m_pBuffer;

abort:
    returnErr(err);
} // WriteToFile






