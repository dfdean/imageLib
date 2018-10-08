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
// See the corresponding .cpp file for a description of this module.
/////////////////////////////////////////////////////////////////////////////

#ifndef _IMAGE_LIB_INTERNAL_IMPL_H_
#define _IMAGE_LIB_INTERNAL_IMPL_H_

// This is the public interface.
#include "imageLib.h"

#define GENERATED_LINE_DETECTION_FILE_SUFFIX    ".lines.bmp"


////////////////////////////////////////////////////////////////////////////////
//
// Cross Sections
//
////////////////////////////////////////////////////////////////////////////////

// A simple cross-section for a shape. This is just start and stop positions
// on a horizontal scan line. It makes it easy for hit testing.
class CBioCADCrossSection {
public:
    int32               m_Y;
    int32               m_StartX;
    int32               m_StopX;
}; // CBioCADCrossSection



////////////////////////////////////////////////////////////////////////////////
//
// EDGE DETECTION
//
////////////////////////////////////////////////////////////////////////////////

// These are the directions of the gradients.
#define PIXELS_BRIGHTER_WEST_TO_EAST     1
#define PIXELS_BRIGHTER_EAST_TO_WEST     2
#define PIXELS_BRIGHTER_NORTH_TO_SOUTH   3
#define PIXELS_BRIGHTER_SOUTH_TO_NORTH   4
#define PIXELS_BRIGHTER_NE_TO_SW         5
#define PIXELS_BRIGHTER_SW_TO_NE         6
#define PIXELS_BRIGHTER_NW_TO_SE         7
#define PIXELS_BRIGHTER_SE_TO_NW         8


///////////////////////////////////////////////////////
class CEdgeDetectionEntry {
public:
    uint8           m_IsEdge;
    uint8           m_GrayScaleValue;
    uint8           m_GradientDirection;
    int32           m_Gradient;
}; // CEdgeDetectionEntry


///////////////////////////////////////////////////////
class CEdgeDetectionTable {
public:
    CEdgeDetectionTable();
    virtual ~CEdgeDetectionTable();
    NEWEX_IMPL();

    ErrVal Initialize(
                    CImageFile *pSrcImage,
                    uint32 blackWhiteThreshold);

    bool IsEdge(int32 x, int32 y);
    uint8 GetLuminance(int32 x, int32 y);
    uint8 GetGradientDirection(int32 x, int32 y);
    int32 GetGradient(int32 x, int32 y);

    int32               m_MaxXPos;
    int32               m_MaxYPos;
    CEdgeDetectionEntry *m_pInfoTable;

private:
    uint32 GetPixelLuminance(CImageFile *pSrcImage, uint32 pixel);
}; // CEdgeDetectionTable




ErrVal AllocateEdgeDetectionTable(
                CImageFile *pImageFile, 
                CEdgeDetectionTable **ppResult);





////////////////////////////////////////////////////////////////////////////////
//
// 3D Files
//
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////
class C3DModelFile {
public:
    virtual ErrVal InitializeForNewFile(const char *pFilePath) = 0;
    virtual void Close() = 0;    
    virtual void CloseOnDiskOnly() = 0;
    virtual ErrVal Save() = 0;

    virtual ErrVal AddVertex(int32 x, int32 y, int32 z, int32 index) = 0;
    virtual ErrVal AddColoredVertex(
                        int32 x, int32 y, int32 z, 
                        int32 index, 
                        int32 red, int32 blue, int32 green) = 0;
    virtual ErrVal AddLine(int32 numPoints, int32 pointID1, int32 pointID2) = 0;
    virtual ErrVal AddColoredLine(int32 pointID1, int32 pointID2, int32 red, int32 blue, int32 green) = 0;    
    virtual ErrVal AddPolygon(
                        int32 numPoints, 
                        int32 pointID1, 
                        int32 pointID2, 
                        int32 pointID3, 
                        int32 pointID4) = 0;
    virtual ErrVal AddColoredPolygon(
                        int32 numPoints, 
                        int32 pointID1, 
                        int32 pointID2, 
                        int32 pointID3, 
                        int32 pointID4, 
                        int32 red, 
                        int32 blue, 
                        int32 green) = 0;

    virtual ErrVal StartPolygon(int32 numPoints) = 0;
    virtual ErrVal AddPointToPolygon(int32 index, int32 pointID) = 0;
}; // CPLY3DModelFile


C3DModelFile *CreateNewPLYFile(const char *pFilePath);
void Delete3DFileRuntimeState(C3DModelFile *pFile);


////////////////////////////////////////////////
// A single object in 3D space. A model is a group of these 3d objects.
class CBioCAD3DObject {
public:
    //CBioCADNode         *m_pAllShapeNodes;
    //CBioCADNode         *m_p3DSkeletonNodes;
    //CBioCADSpine         *m_pAllShapeEdges;
    //CBioCADSpine         *m_p3DSkeletonEdges;

    CBioCAD3DObject     *m_pNextObject;
}; // CBioCAD3DObject




////////////////////////////////////////////////
// This is a 3D model that is built up from a stack of images and contains one or more 3DObjects
class C3DModel {
public:
    virtual ErrVal Add2dImage(const char *pImageFileName, int32 options, CStatsFile *pStatFile) = 0;
    virtual C2DImage *GetFirstImage() = 0;
    virtual C2DImage *GetImageAtZPlane(int32 zPlane) = 0;
    virtual CBioCAD3DObject *GetFirstObject() = 0;

    virtual ErrVal DrawModel(const char *pImageFileName) = 0;
    virtual ErrVal Draw3DSkeleton(const char *pImageFileName) = 0;
}; // C3DModel


ErrVal Build3DModel(
            const char *pImageFileName,
            int32 options, 
            CStatsFile *pStatFile, 
            C3DModel **ppResult);




#endif // _IMAGE_LIB_INTERNAL_IMPL_H_



