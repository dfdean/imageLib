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

#include <math.h>

#if WASM
#include "portableBuildingBlocks.h"
#else
#include "buildingBlocks.h"
#endif

#include "perfMetrics.h"
#include "imageLib.h"
#include "imageLibInternal.h"

FILE_DEBUGGING_GLOBALS(LOG_LEVEL_DEFAULT, 0);

#define HIGHLIGHT_PIXEL_VALUE   YELLOW_PIXEL
#define BLOCKED_PIXEL_VALUE     RED_PIXEL

// LIST_END_PIXEL marks the end of the list.
static int32 g_ColoredShapeColorList[] = { BLUE_PIXEL, GREEN_PIXEL, PURPLE_PIXEL, YELLOW_PIXEL, ORANGE_PIXEL, BLUEGREEN_PIXEL, CAMAUGREEN_PIXEL, COLOR1_PIXEL, COLOR2_PIXEL, COLOR3_PIXEL, LIST_END_PIXEL };
static int32 g_GrayShapeColorList[] = { BLACK_PIXEL, LIST_END_PIXEL };


///////////////////////////////////////////////////////////////
// These should all be passed in as client parameters.
#define EDGE_DETECTION_THRESHOLD                        25 // 150 -> 110-->60-->40-->30-->25-->20
#define MAX_LUMINENCE_DIFFERENCE_FOR_NEARBY_EDGE_PIXELS 0
//#define MIN_LUMINENCE_FOR_NON_EDGE_PIXEL_ON_BOUNDARY    100 // 80
//#define MIN_LUMINENCE_FOR_BRIGHT_PIXEL                  100

#define MAX_DISTANCE_BETWEEN_DANGLING_PEERS             10.0L
#define MIN_PIXELS_IN_USEFUL_SHAPE                      30
#define MAX_SLOPE_FOR_PATH_WALKING                      5.0

static bool g_DrawStopPixels                    = false;
static bool g_EraseBorderArtifacts              = false;
static int32 g_BackGroundPixelColor             = BLACK_PIXEL;
static int32 g_ShapeInteriorColor               = GREEN_PIXEL;



////////////////////////////////////////////////
// This is the state of each pixel.
class CPixelInfo {
public:  
    int32               m_Flags;
    int32               m_X;
    int32               m_Y;

    CBioCADShape        *m_pShape;
    //<>CMarkerPixelInfo    *m_pMarker;
    //<>CBioCADSpine        *m_pEdge;

    CPixelInfo          *m_pNextPixel;
}; // CPixelInfo

// These are values for CPixelInfo::m_Flags
#define SHAPE_INTERIOR_PIXEL        0x0001
#define SHAPE_EXTERIOR_PIXEL        0x0002
#define SHAPE_BOUNDARY_PIXEL        0x0004
#define DANGLING_BORDER_PIXEL       0x0008
#define EXTRAPOLATED_PIXEL          0x0010
#define DEBUG_HIGHLIGHT_PIXEL       0x0020



////////////////////////////////////////////////
class C2DImageImpl : public C2DImage {
public:  
    C2DImageImpl();
    virtual ~C2DImageImpl();
    NEWEX_IMPL();
    ErrVal Initialize(CImageFile *pImageSource, const char *pImageFileName, int32 options);

    // C2DImage
    virtual ErrVal Save();
    virtual ErrVal SaveAs(const char *pNewFilePathName);
    virtual void Close();
    virtual void CloseOnDiskOnly();

    virtual void GetDimensions(int32 *pWidth, int32 *pHeight);
    virtual void GetBitMap(char **ppBitMap, int32 *pBitmapLength);
    virtual ErrVal GetFeatureProperty(
                        int32 featureID,
                        int32 propertyID,
                        int32 *pResult);

    virtual ErrVal AddFeature(
                    int32 featureType,
                    int32 pointAX, 
                    int32 pointAY, 
                    int32 pointBX, 
                    int32 pointBY, 
                    int32 options,
                    int32 color,
                    int32 *pFeatureIDResult);
    virtual void DrawFeatures(int32 options);

    virtual ErrVal CopyRect(
                    int32 srcLeftX, 
                    int32 srcTopY, 
                    int32 srcWidth, 
                    int32 srcHeight, 
                    int32 destLextX, 
                    int32 destTopY);
    virtual ErrVal CropImage(
                    int32 newWidth, 
                    int32 newHeight);

    virtual ErrVal CreateInspectRegion(
                    int32 positionType,
                    int32 topOffset, 
                    int32 bottomOffset, 
                    int32 leftOffset, 
                    int32 rightOffset, 
                    CBioCADShape **ppResult);

private:
    virtual CPixelInfo *GetPixelState(int32 x, int32 y);
    int32 GetPixelFlags(int32 x, int32 y);
    void SetPixelFlag(int32 x, int32 y, int32 newFlag);
    void ClearPixelFlag(int32 x, int32 y, int32 newFlag);
    
    ErrVal FindEdgePointsOnSameShape(CBioCADShape *pShape);
    int32 CheckPossibleAdjacentEdgePixel(
                    CBioCADShape *pShape,  
                    int32 x, 
                    int32 y);

    void DrawEdges(int32 options);
    void DrawLine(
                int32 pointAX, 
                int32 pointAY, 
                int32 pointBX, 
                int32 pointBY, 
                int32 lineColor);

    void ConnectDanglingEnds(CBioCADShape *pShape);
    int32 CountNeighborPixels(CBioCADShape *pShape, int32 x, int32 y);
    bool PixelsAppearOnSimilarPaths(CBioCADPoint *pPoint1, CBioCADPoint *pPoint2);
    ErrVal AddExtrapolatedBorderPoints(
                    CBioCADShape *pShape,
                    CBioCADPoint *pPoint1, 
                    CBioCADPoint *pPoint2);    
    ErrVal AddOneExtrapolatedPixel(
                    CBioCADShape *pShape, 
                    int32 x, 
                    int32 y, 
                    CPixelInfo *pPixelState);
    void DeleteShape(CBioCADShape *pShape);
    
    ErrVal BuildCrossSections();
    ErrVal RedrawProcessedImage(int32 options);


    char                *m_pImageFileName;
    CImageFile          *m_pSourceFile;
    CEdgeDetectionTable *m_pEdgeDetectionTable;

    int32               m_ImageWidth;
    int32               m_ImageHeight;

    int32               m_NumPixelsInImage;
    CPixelInfo          *m_pPixelFlagsTable;

    CBioCADShape        *m_pShapeList;
    CBioCADShape        *m_pInspectRegionList;
}; // C2DImageImpl





/////////////////////////////////////////////////////////////////////////////
//
// [Open2DImageFromFile]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal 
Open2DImageFromFile(
            const char *pImageFileName, 
            int32 options, 
            CStatsFile *pStatFile,
            C2DImage **ppResult) {
    ErrVal err = ENoErr;
    CImageFile *pImageSource = NULL;
    C2DImageImpl *p2DImage = NULL;
    UNUSED_PARAM(pStatFile);

    // Validate the parameters.
    if ((NULL == pImageFileName) || (NULL == ppResult)) {
        gotoErr(EFail);
    }
    *ppResult = NULL;

    p2DImage = newex C2DImageImpl;
    if (NULL == p2DImage) {
        gotoErr(EFail);
    }

    // Open the cell image file.
    pImageSource = OpenBMPFile(pImageFileName);
    if (NULL == pImageSource) {
        gotoErr(EFail);
    }

    err = p2DImage->Initialize(pImageSource, pImageFileName, options);
    if (err) {
        gotoErr(err);
    }

    *ppResult = p2DImage;
    p2DImage = NULL;


abort:
    delete p2DImage;
    returnErr(err);
} // Open2DImageFromFile





/////////////////////////////////////////////////////////////////////////////
//
// [Open2DImageFromBitMap]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal 
Open2DImageFromBitMap(
                char *pSrcBitMap, 
                const char *pBitmapFormat, 
                int32 widthInPixels, 
                int32 heightInPixels, 
                int32 bitsPerPixel,
                int32 options, 
                CStatsFile *pStatFile,
                C2DImage **ppResult) {
    ErrVal err = ENoErr;
    CImageFile *pImageSource = NULL;
    C2DImageImpl *p2DImage = NULL;
    UNUSED_PARAM(pStatFile);

    // Validate the parameters.
    if ((NULL == pSrcBitMap) || (NULL == ppResult)) {
        gotoErr(EFail);
    }
    *ppResult = NULL;

    p2DImage = newex C2DImageImpl;
    if (NULL == p2DImage) {
        gotoErr(EFail);
    }

    // Open the cell image file.
    pImageSource = OpenBitmapImage(
                            pSrcBitMap, 
                            pBitmapFormat, 
                            widthInPixels, 
                            heightInPixels, 
                            bitsPerPixel);
    if (NULL == pImageSource) {
        gotoErr(EFail);
    }

    err = p2DImage->Initialize(pImageSource, NULL, options);
    if (err) {
        gotoErr(err);
    }

    *ppResult = p2DImage;
    p2DImage = NULL;


abort:
    delete p2DImage;
    returnErr(err);
} // Open2DImageFromBitMap






/////////////////////////////////////////////////////////////////////////////
//
// [Open2DImageFromFile]
//
/////////////////////////////////////////////////////////////////////////////
void
DeleteImageObject(C2DImage *pGenericImage) {
    C2DImageImpl *p2DImage = (C2DImageImpl *) pGenericImage;
    if (p2DImage) {
        delete p2DImage;
    }
} // DeleteImageObject




/////////////////////////////////////////////////////////////////////////////
//
// [C2DImageImpl]
//
/////////////////////////////////////////////////////////////////////////////
C2DImageImpl::C2DImageImpl() {
    m_pImageFileName = NULL;
    m_pSourceFile = NULL;
    m_pEdgeDetectionTable = NULL;

    m_pPixelFlagsTable = NULL;

    m_pShapeList = NULL;
    m_pInspectRegionList = NULL;
      
    m_ZPlane = 0;
    m_pNextImage = NULL;
} // C2DImageImpl





/////////////////////////////////////////////////////////////////////////////
//
// [~C2DImageImpl]
//
/////////////////////////////////////////////////////////////////////////////
C2DImageImpl::~C2DImageImpl() {
    memFree(m_pImageFileName);
    memFree(m_pPixelFlagsTable);

    while (m_pShapeList) {
        CBioCADShape *pTargetShape = m_pShapeList;
        m_pShapeList = pTargetShape->m_pNextShape;
        delete pTargetShape;
    } // if (m_pShapeList)

    while (m_pInspectRegionList) {
        CBioCADShape *pTargetShape = m_pInspectRegionList;
        m_pInspectRegionList = pTargetShape->m_pNextShape;
        delete pTargetShape;
    } // if (m_pInspectRegionList)

    if (m_pEdgeDetectionTable) {
        delete m_pEdgeDetectionTable;
    }

    if (m_pSourceFile) {
        m_pSourceFile->Save(0);
        DeleteImageObject(m_pSourceFile);
    }
} // ~C2DImageImpl





/////////////////////////////////////////////////////////////////////////////
//
// [Initialize]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal 
C2DImageImpl::Initialize(
            CImageFile *pImageSource, 
            const char *pImageFileName, 
            int32 options) {
    ErrVal err = ENoErr;
    char *pEdgeImageFileName = NULL;
    int32 edgeDetectionThreshold = EDGE_DETECTION_THRESHOLD;
    int32 x;
    int32 y;
    CPixelInfo *pPixelInfo;
    CBioCADPoint *pPoint;
    bool isEdge;
    CBioCADShape *pShape = NULL;


    // Validate the parameters.
    if (NULL == pImageSource) {
        gotoErr(EFail);
    }
    m_pSourceFile = pImageSource;


    // Save a copy of the file name so we can reopen it and change it later.
    if (pImageFileName) {
        m_pImageFileName = strdupex(pImageFileName);
        if (NULL == m_pImageFileName) {
            gotoErr(EFail);
        }
    }

    err = m_pSourceFile->GetImageInfo(&m_ImageWidth, &m_ImageHeight);
    if (err) {
        gotoErr(err);
    }

    /////////////////////////////////////////
    // Some images may have an artifact of light along the edges.
    // Draw a row of black pixels along the edges to block this out.
    if (g_EraseBorderArtifacts) {
        int32 x;
        for (x = 0; x < m_ImageWidth; x++) {
            m_pSourceFile->SetPixel(x, 0, BLACK_PIXEL);
            m_pSourceFile->SetPixel(x, m_ImageHeight - 1, BLACK_PIXEL);
        }
    }

    m_NumPixelsInImage = m_ImageWidth * m_ImageHeight;
    m_pPixelFlagsTable = (CPixelInfo *) memCalloc(sizeof(CPixelInfo) * m_NumPixelsInImage);
    if (NULL == m_pPixelFlagsTable) {
        gotoErr(EFail);
    }

    // Create a luminence map from the original image. This basically makes an image
    // where every pixel is an overall brightness value, rather than specific color.
    // This is used by both edge detection and line detection.
    err = AllocateEdgeDetectionTable(m_pSourceFile, &m_pEdgeDetectionTable);
    if (err) {
        gotoErr(err);
    }
    
    // Create a temporary data structure for holding the edge detection data.
    // If the flag is set, then we generate a non-NULL file name and actually 
    // save it to disk.
    if ((pImageFileName) && (options & CELL_GEOMETRY_SAVE_EDGE_DETECTION_TO_FILE)) {
        pEdgeImageFileName = strCatEx(pImageFileName, GENERATED_LINE_DETECTION_FILE_SUFFIX);
        if (NULL == pEdgeImageFileName) {
            gotoErr(EFail);
        }
    }

    // Examine the original image and find the edges. This will save the edges
    // to the temporary image object we just created.
    err = m_pEdgeDetectionTable->Initialize(m_pSourceFile, edgeDetectionThreshold);
    if (err) {
        gotoErr(err);
    }

    if (options & CELL_GEOMETRY_SAVE_EDGE_DETECTION_TO_FILE) {
        //<><><>
    }


    ///////////////////////////////////////
    // Look at every pixel and create shape objects for each separate edge.
    for (x = 0; x < m_ImageWidth; x++) {
        for (y = 0; y < m_ImageHeight; y++) {
            pPixelInfo = GetPixelState(x, y);
            if (NULL == pPixelInfo) {
                continue;
            }

            // Set the X and Y coordinate in every pixel entry.
            // This lets me know where a pixel came from when I am passed just the pixelInfo.
            pPixelInfo->m_X = x;
            pPixelInfo->m_Y = y;
            //pPixelInfo->m_pMarker = NULL;
            //pPixelInfo->m_pEdge = NULL;

            // Ignore any pixel that is already part of another shape.
            // This loop walks through the pixels in raster scan. But, we may 
            // follow a shape to pixels out of order, so we may have already seen a 
            // pixel by the time this loop gets to it.
            if (pPixelInfo->m_Flags & SHAPE_INTERIOR_PIXEL) {
                continue;
            }

            isEdge = m_pEdgeDetectionTable->IsEdge(x, y);
            // Edges are black in the EdgeDetection bitmap, even though it may the color 
            // of the background in some original images. So even if the original images have white
            // lines on a black background (or any other color combination), the edgeDetection is 
            // always black lines on a white background.
            if (!isEdge) {
                continue;
            }

            // We found an edge pixel that is not in a shape. So, start a new shape for
            // this pixel and all (directly or indirectly) adjacent pixels.
            pShape = newex CBioCADShape;
            if (NULL == pShape) {
                gotoErr(EFail);
            }
            pShape->m_pSourceFile = m_pSourceFile;
            pShape->m_FeatureType = CBioCADShape::FEATURE_TYPE_REGION;
            pShape->m_ShapeFlags = pShape->m_ShapeFlags | CBioCADShape::SOFTWARE_DISCOVERED;

            SetPixelFlag(x, y, SHAPE_INTERIOR_PIXEL);
            pPoint = pShape->AddPoint(x, y, m_ZPlane);
            if (NULL == pPoint) {
                gotoErr(EFail);
            }

            // Spread out from these initial points, and find all connected
            // points on a shared edge or points with the same color. This 
            // should find all points in the current shape
            FindEdgePointsOnSameShape(pShape);
            if (pShape->m_NumPoints < MIN_PIXELS_IN_USEFUL_SHAPE) {
                delete pShape;
            } else {
                pShape->m_pOwnerImage = this;
                pShape->m_pNextShape = m_pShapeList;
                m_pShapeList = pShape;
            }

            pShape = NULL;
        } // for (y = 0; y < pMap->m_MaxYPos; y++)
    } // for (x = 0; x < maxXPos; x++)



    ///////////////////////////////////////
    // Perform several local fixups to the image. The scan may be imperfect, and the
    // edge-detection may also be imperfect, and both may cause gaps in the shape border.
    pShape = m_pShapeList;
    while (pShape) {
        // Find the INITIAL bounding box. This is used for the other shape
        // operations, and it helps limit the number of pixels we search through 
        // for each shape.
        pShape->FindBoundingBox();

/////////////////////////////////////////////////////////////////////////////////
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>
#if 0
        // Patch small gaps in the border. This may make some points that were 
        // previously on the outside border now on the interior of the shape.
        PatchSmallGapsInBorder(pShape);
                
        // Removing the bottlenecks added new extrapolated points.
        !!!!!!!!!!!!!!!!!!!!!!!!1 Connecting dangling endpoints is a search problem.
        ConnectDanglingEnds(pShape);
        
        // Update the bounding box. Do this after we have fixed up the shape border.
        pShape->FindBoundingBox();
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>
#endif

        pShape = pShape->m_pNextShape;
    } // while (pShape)

    
    ///////////////////////////////////////
    // Discard any shape that is not big enough to be interesting, or
    // that have been deleted for other reasons.
    pShape = m_pShapeList;
    m_pShapeList = NULL;
    while (pShape) {
        CBioCADShape *pNextShape = pShape->m_pNextShape;

        if ((pShape->m_NumPoints < MIN_PIXELS_IN_USEFUL_SHAPE)
                || (pShape->m_ShapeFlags & CBioCADShape::SHAPE_FLAG_DELETE)) {
            DeleteShape(pShape);
        } else {
            pShape->m_pNextShape = m_pShapeList;
            m_pShapeList = pShape;
        }

        // Process the next shape on the next iteration.
        pShape = pNextShape;
    } // while (pShape)



    ///////////////////////////////////////
    // Build a list of Cross-sections for each shape. This is basically a run-length encoding
    // of the horizontal scan lines that pass through each shape. This is a faily small data
    // structure that is used a lot in later steps of the shape analysis.
    BuildCrossSections();

    /////////////////////////////////////////////////////////////////////
    // Optionally re-draw the image with just the shapes.
    // m_pSourceFile now has the updated image.
    RedrawProcessedImage(options);

abort:
    if (pEdgeImageFileName) {
        memFree(pEdgeImageFileName);
    }

    return(err);
} // Initialize





/////////////////////////////////////////////////////////////////////////////
//
// [FindEdgePointsOnSameShape]
//
// This will find all points on the border, but it will also find a lot of
// points inside the shape as well. Find everythng for now, and the redundant
// points will be filtered out later.
/////////////////////////////////////////////////////////////////////////////
ErrVal
C2DImageImpl::FindEdgePointsOnSameShape(CBioCADShape *pShape) {
    ErrVal err = ENoErr;
    CBioCADPoint *pCurrentPoint = NULL;
    CBioCADPoint *pProcessedPoints = NULL;
    int32 numNeighborsFound;

    if (NULL == pShape) {
        gotoErr(EFail);
    }    

    // Look for every neighboring pixel to find any other pixels that also
    // form the shape boundary. These may not have had enough contrast to show up in the edge
    // map, but they are similar and adjacent to the pixel that did.
    //
    // This will iterate on the point list so
    // a pixel found on one iterations may cause us to look for more neighbors of this new pixel
    // on future iterations.
    pProcessedPoints = NULL;
    while (pShape->m_pPointList) {
        // Take the current point off the pending list
        pCurrentPoint = pShape->m_pPointList;
        pShape->m_pPointList = pCurrentPoint->m_pNextPoint;

        // Put it on the completed list.
        pCurrentPoint->m_pNextPoint = pProcessedPoints;
        pProcessedPoints = pCurrentPoint;
    
        // Look for neighbors of this point.
        numNeighborsFound = 0;
        numNeighborsFound += CheckPossibleAdjacentEdgePixel(pShape, pCurrentPoint->m_X - 1, pCurrentPoint->m_Y - 1);
        numNeighborsFound += CheckPossibleAdjacentEdgePixel(pShape, pCurrentPoint->m_X - 1, pCurrentPoint->m_Y);
        numNeighborsFound += CheckPossibleAdjacentEdgePixel(pShape, pCurrentPoint->m_X - 1, pCurrentPoint->m_Y + 1);

        numNeighborsFound += CheckPossibleAdjacentEdgePixel(pShape, pCurrentPoint->m_X, pCurrentPoint->m_Y - 1);
        numNeighborsFound += CheckPossibleAdjacentEdgePixel(pShape, pCurrentPoint->m_X, pCurrentPoint->m_Y + 1);

        numNeighborsFound += CheckPossibleAdjacentEdgePixel(pShape, pCurrentPoint->m_X + 1, pCurrentPoint->m_Y - 1);
        numNeighborsFound += CheckPossibleAdjacentEdgePixel(pShape, pCurrentPoint->m_X + 1, pCurrentPoint->m_Y);
        numNeighborsFound += CheckPossibleAdjacentEdgePixel(pShape, pCurrentPoint->m_X + 1, pCurrentPoint->m_Y + 1);

        if (numNeighborsFound <= 1) {
            CPixelInfo *pPixelState;
            pPixelState = GetPixelState(pCurrentPoint->m_X, pCurrentPoint->m_Y);
            if (pPixelState) {
                pPixelState->m_Flags |= DANGLING_BORDER_PIXEL;
            }
        }
    } // while (pShape->m_pPointList)

    // Put all processed points back on the past.
    pShape->m_pPointList = pProcessedPoints;

abort:
    returnErr(err);
} // FindEdgePointsOnSameShape







/////////////////////////////////////////////////////////////////////////////
//
// [CheckPossibleAdjacentEdgePixel]
//
/////////////////////////////////////////////////////////////////////////////
int32
C2DImageImpl::CheckPossibleAdjacentEdgePixel(
                            CBioCADShape *pShape, 
                            int32 x, 
                            int32 y) {
    ErrVal err = ENoErr;
    bool fPixelIsPossibleBorder = true;
    CBioCADPoint *pPoint;
    int32 pixelFlags;
    int32 numNeighborsFound = 0;
    bool isEdge;

    if (NULL == pShape) {
        gotoErr(EFail);
    }
    
    // Do not freak out if the coordinates are not value. The caller may be a bit sloppy
    // to simplify his loops.
    if ((x < 0) || (y < 0) || (x >= m_ImageWidth) || (y >= m_ImageHeight)) {
        gotoErr(ENoErr);
    }

    // Ignore any pixel that is already part of a shape.
    pixelFlags = GetPixelFlags(x, y);
    if (pixelFlags & SHAPE_INTERIOR_PIXEL) {
        numNeighborsFound = 1;
        goto abort;
    }

    // Get the pixel we are considering.
    isEdge = m_pEdgeDetectionTable->IsEdge(x, y);
    fPixelIsPossibleBorder = false;

    // If it is an edge, and even slightly bright, then include it.
    // Be careful! Edges are black, even though in the original image, an "edge" may be bright (like white). 
    if (isEdge) {
        fPixelIsPossibleBorder = true;
    }

    // If a pixel is a border pixel, then add it to the shape.
    if (fPixelIsPossibleBorder) {
        SetPixelFlag(x, y, SHAPE_INTERIOR_PIXEL);
        pPoint = pShape->AddPoint(x, y, m_ZPlane);
        if (NULL == pPoint) {
            gotoErr(EFail);
        }
        numNeighborsFound = 1;
    } else if (g_DrawStopPixels) {
        err = m_pSourceFile->SetPixel(x, y, BLOCKED_PIXEL_VALUE);
        if (err) {
            gotoErr(err);
        }
    }
    
abort:
    return(numNeighborsFound);
} // CheckPossibleAdjacentEdgePixel






/////////////////////////////////////////////////////////////////////////////
//
// [Save]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
C2DImageImpl::Save() {
    ErrVal err = ENoErr;

    if (m_pSourceFile) {
        m_pSourceFile->Save(0);
    }

    returnErr(err);
} // Save





/////////////////////////////////////////////////////////////////////////////
//
// [SaveAs]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
C2DImageImpl::SaveAs(const char *pNewFilePathName) {
    ErrVal err = ENoErr;

    if (NULL == pNewFilePathName) {
        gotoErr(EFail);
    }

    if (m_pSourceFile) {
        m_pSourceFile->SaveAs(pNewFilePathName, 0);
    }

    memFree(m_pImageFileName);
    m_pImageFileName = strdupex(pNewFilePathName);
    if (NULL == m_pImageFileName) {
        gotoErr(EFail);
    }

abort:
    returnErr(err);
} // SaveAs






/////////////////////////////////////////////////////////////////////////////
//
// [ConnectDanglingEnds]
//
//        !!!!!!!!!!!!!!!!!!!!!!!!1 Connecting dangling endpoints is a search problem.
/////////////////////////////////////////////////////////////////////////////
void
C2DImageImpl::ConnectDanglingEnds(CBioCADShape *pShape) {
    CBioCADPoint *pOriginalPointList;
    CBioCADPoint *pCurrentPoint;
    CBioCADPoint *pPeerPoint;
    CBioCADPoint *pNextPoint;
    CPixelInfo *pPixelState;
    CPixelInfo *pPeerPixelState;
    int32 numNeighbors;
    double distanceToPoint;
    double closestDistance;
    CBioCADPoint *pClosestDanglingPoint;

    if (NULL == pShape) {
        return;
    }

    // Find all dangling endpoints.
    // This will also remove isolated points that are not part of a connected shape.
    pCurrentPoint = pShape->m_pPointList;
    while (pCurrentPoint) {
        //pCurrentPoint->m_PointFlags &= ~CBioCADPoint::DELETE_POINT;

        pPixelState = GetPixelState(pCurrentPoint->m_X, pCurrentPoint->m_Y);
        if (NULL == pPixelState) {
            //pCurrentPoint->m_PointFlags |= CBioCADPoint::DELETE_POINT;
            pCurrentPoint = pCurrentPoint->m_pNextPoint;
            continue;
        }

        numNeighbors = CountNeighborPixels(pShape, pCurrentPoint->m_X, pCurrentPoint->m_Y);
        if (0 == numNeighbors) {
            //pCurrentPoint->m_PointFlags |= CBioCADPoint::DELETE_POINT;
            pCurrentPoint = pCurrentPoint->m_pNextPoint;
            continue;
        }

        if (1 == numNeighbors) {
            pPixelState->m_Flags |= DANGLING_BORDER_PIXEL;
            //pPixelState->m_Flags |= DEBUG_HIGHLIGHT_PIXEL;
        }

        pCurrentPoint = pCurrentPoint->m_pNextPoint;
    } // while (pCurrentPoint)


    // For each dangling endpoint, find the closest other dangling endpoint.
    // If there is one, then we can connect them.
    pCurrentPoint = pShape->m_pPointList;
    while (pCurrentPoint) {
        pNextPoint = pCurrentPoint->m_pNextPoint;

        pPixelState = GetPixelState(pCurrentPoint->m_X, pCurrentPoint->m_Y);
        if ((pPixelState) && (pPixelState->m_Flags & DANGLING_BORDER_PIXEL)) {
            pClosestDanglingPoint = NULL;

            // For each dangling endpoint, look for the closest other dangling endpoint.
            pPeerPoint = pShape->m_pPointList;
            while (pPeerPoint) {
                if (pPeerPoint != pCurrentPoint) {
                    pPeerPixelState = GetPixelState(pPeerPoint->m_X, pPeerPoint->m_Y);
                    if ((pPeerPixelState) && (pPeerPixelState->m_Flags & DANGLING_BORDER_PIXEL)) {
                        distanceToPoint = GetDistanceBetweenPoints(pCurrentPoint, pPeerPoint);
                        if ((distanceToPoint < MAX_DISTANCE_BETWEEN_DANGLING_PEERS)
                                && (PixelsAppearOnSimilarPaths(pCurrentPoint, pPeerPoint))) {
                            if ((NULL == pClosestDanglingPoint) || (distanceToPoint < closestDistance)) {
                                pClosestDanglingPoint = pPeerPoint;
                                closestDistance = distanceToPoint;
                            }
                        } // if (distanceToPoint < MAX_DISTANCE_BETWEEN_DANGLING_PEERS)
                    }
                } // if (pPeerPoint != pCurrentPoint)

                pPeerPoint = pPeerPoint->m_pNextPoint;
            } // while (pPeerPoint)
            
            // If this dangling point has a close other dangling point, then connect them.
            if (pClosestDanglingPoint) {
                (void) AddExtrapolatedBorderPoints(pShape, pCurrentPoint, pClosestDanglingPoint);
            } // if (pClosestDanglingPoint)
            else {
                //pCurrentPoint->m_PointFlags |= CBioCADPoint::DELETE_POINT;
            }
        } // if ((pPixelState) && (pPixelState->m_Flags & DANGLING_BORDER_PIXEL))

        pCurrentPoint = pCurrentPoint->m_pNextPoint;
    } // while (pCurrentPoint)


    

    //////////////////////////////////////////////
    // Clean up the list.
    // Save all points to a side list until they are validated.
    // Points will have to have some neighbors to be part of the final list of
    // points for this shape.
    pOriginalPointList = pShape->m_pPointList;
    pShape->m_pPointList = NULL;
    pShape->m_NumPoints = 0;

    pCurrentPoint = pOriginalPointList;
    while (pCurrentPoint) {
        pNextPoint = pCurrentPoint->m_pNextPoint;

        if (0) { // (pCurrentPoint->m_PointFlags & CBioCADPoint::DELETE_POINT) {
            pPixelState = GetPixelState(pCurrentPoint->m_X, pCurrentPoint->m_Y);
            if (pPixelState) {
                pPixelState->m_Flags &= ~SHAPE_INTERIOR_PIXEL;
                pPixelState->m_Flags &= ~SHAPE_BOUNDARY_PIXEL;
                pPixelState->m_Flags &= ~DANGLING_BORDER_PIXEL;
                pPixelState->m_Flags &= ~EXTRAPOLATED_PIXEL;
                pPixelState->m_Flags &= ~DEBUG_HIGHLIGHT_PIXEL;

                pPixelState->m_Flags |= SHAPE_EXTERIOR_PIXEL;
                pPixelState->m_pShape = NULL;
            }
            delete pCurrentPoint;
        } else {
            // Put the point back on the final list.
            pCurrentPoint->m_pNextPoint = pShape->m_pPointList;
            pShape->m_pPointList = pCurrentPoint;
            pShape->m_NumPoints += 1;
        }

        pCurrentPoint = pNextPoint;
    } // while (pCurrentPoint)
} // ConnectDanglingEnds






/////////////////////////////////////////////////////////////////////////////
//
// [GetPixelFlags]
//
/////////////////////////////////////////////////////////////////////////////
int32
C2DImageImpl::GetPixelFlags(int32 x, int32 y) { 
    CPixelInfo *pPixelInfo;

    if ((NULL == m_pPixelFlagsTable) 
            || (x < 0) 
            || (y < 0) 
            || (x >= m_ImageWidth) 
            || (y >= m_ImageHeight)) { 
        return(0); 
    } 

    pPixelInfo = &(m_pPixelFlagsTable[(y * m_ImageWidth) + x]);
    return(pPixelInfo->m_Flags); 
}  // GetPixelFlags  






/////////////////////////////////////////////////////////////////////////////
//
// [SetPixelFlag]
//
/////////////////////////////////////////////////////////////////////////////
void
C2DImageImpl::SetPixelFlag(int32 x, int32 y, int32 newFlag) { 
    CPixelInfo *pPixelInfo;

    if ((NULL == m_pPixelFlagsTable)
            || (x < 0)
            || (y < 0)
            || (x >= m_ImageWidth) 
            || (y >= m_ImageHeight)) { 
        return; 
    } 

    pPixelInfo = &(m_pPixelFlagsTable[(y * m_ImageWidth) + x]);
    pPixelInfo->m_Flags |= newFlag;
} // SetPixelFlag





/////////////////////////////////////////////////////////////////////////////
//
// [GetPixelState]
//
/////////////////////////////////////////////////////////////////////////////
CPixelInfo *
C2DImageImpl::GetPixelState(int32 x, int32 y) { 
    CPixelInfo *pPixelInfo;

    if ((NULL == m_pPixelFlagsTable) 
            || (x < 0) 
            || (y < 0) 
            || (x >= m_ImageWidth) 
            || (y >= m_ImageHeight)) { 
        return(NULL); 
    } 

    pPixelInfo = &(m_pPixelFlagsTable[(y * m_ImageWidth) + x]);
    return(pPixelInfo);
} // GetPixelState






/////////////////////////////////////////////////////////////////////////////
//
// [ClearPixelFlag]
//
/////////////////////////////////////////////////////////////////////////////
void 
C2DImageImpl::ClearPixelFlag(int32 x, int32 y, int32 newFlag) { 
    CPixelInfo *pPixelInfo;

    if ((NULL == m_pPixelFlagsTable)
            || (x < 0) 
            || (y < 0)
            || (x >= m_ImageWidth) 
            || (y >= m_ImageHeight)) { 
        return; 
    } 

    pPixelInfo = &(m_pPixelFlagsTable[(y * m_ImageWidth) + x]);
    pPixelInfo->m_Flags &= ~newFlag;
} // ClearPixelFlag




/////////////////////////////////////////////////////////////////////////////
//
// [Close]
//
/////////////////////////////////////////////////////////////////////////////
void
C2DImageImpl::Close() {
    memFree(m_pPixelFlagsTable);
    m_pPixelFlagsTable = NULL;

    memFree(m_pImageFileName);
    m_pImageFileName = NULL;

    while (m_pShapeList) {
        CBioCADShape *pTargetShape = m_pShapeList;
        m_pShapeList = pTargetShape->m_pNextShape;
        delete pTargetShape;
    } // if (m_pShapeList)

    if (m_pEdgeDetectionTable) {
        delete m_pEdgeDetectionTable;
        m_pEdgeDetectionTable = NULL;
    }

    if (m_pSourceFile) {
        m_pSourceFile->Save(0);
        m_pSourceFile->CloseOnDiskOnly();
        DeleteImageObject(m_pSourceFile);
        m_pSourceFile = NULL;
    }
} // Close





/////////////////////////////////////////////////////////////////////////////
//
// [CloseOnDiskOnly]
//
/////////////////////////////////////////////////////////////////////////////
void
C2DImageImpl::CloseOnDiskOnly() {
    if (m_pSourceFile) {
        m_pSourceFile->CloseOnDiskOnly();
    }

    if (m_pEdgeDetectionTable) {
        delete m_pEdgeDetectionTable;
        m_pEdgeDetectionTable = NULL;
    }
} // CloseOnDiskOnly






/////////////////////////////////////////////////////////////////////////////
//
// [DeleteShape]
//
/////////////////////////////////////////////////////////////////////////////
void
C2DImageImpl::DeleteShape(CBioCADShape *pShape) {
    CBioCADPoint *pCurrentPoint = NULL;
    CBioCADPoint *pNextPoint = NULL;

    if (NULL == pShape) {
        return;
    }

    pCurrentPoint = pShape->m_pPointList;
    pShape->m_pPointList = NULL;
    while (pCurrentPoint) {        
        ClearPixelFlag(pCurrentPoint->m_X, pCurrentPoint->m_Y, SHAPE_INTERIOR_PIXEL);
        ClearPixelFlag(pCurrentPoint->m_X, pCurrentPoint->m_Y, SHAPE_BOUNDARY_PIXEL);

        pNextPoint = pCurrentPoint->m_pNextPoint;
        delete pCurrentPoint;
        pCurrentPoint = pNextPoint;
    } // while (pCurrentPoint)

    delete pShape;
} // DeleteShape




/////////////////////////////////////////////////////////////////////////////
//
// [GetDimensions]
//
/////////////////////////////////////////////////////////////////////////////
void
C2DImageImpl::GetDimensions(int32 *pWidth, int32 *pHeight) {
    if (pWidth) {
        *pWidth = m_ImageWidth;
    }
    if (pHeight) {
        *pHeight = m_ImageHeight;
    }
} // GetDimensions





/////////////////////////////////////////////////////////////////////////////
//
// [GetBitMap]
//
/////////////////////////////////////////////////////////////////////////////
void
C2DImageImpl::GetBitMap(char **ppBitMap, int32 *pBitmapLength) {
    if (m_pSourceFile) {
        (void) m_pSourceFile->GetBitMap(ppBitMap, pBitmapLength);
    }
} // GetBitMap






/////////////////////////////////////////////////////////////////////////////
//
// [GetFeatureProperty]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
C2DImageImpl::GetFeatureProperty(
                        int32 featureID,
                        int32 propertyID,
                        int32 *pResult) {
    ErrVal err = ENoErr;
    CBioCADShape *pShape = NULL;
    UNUSED_PARAM(propertyID);
    
    if (NULL == pResult) {
        gotoErr(EFail);
    }

    // Look for the shape.
    pShape = m_pShapeList;
    while (pShape) {
        if (featureID == pShape->m_FeatureID) {
            break;
        }
        pShape = pShape->m_pNextShape;
    } // while (pShape)

    // If we cannot find the shape, then quit.
    if (NULL == pShape) {
        gotoErr(EFail);
    }

    switch (propertyID) {
    default:
        gotoErr(EFail);
        break;
    } // switch (propertyID)

abort:
    returnErr(err);
} // GetFeatureProperty







/////////////////////////////////////////////////////////////////////////////
//
// [AddFeature]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
C2DImageImpl::AddFeature(
                    int32 featureType,
                    int32 pointAX, 
                    int32 pointAY, 
                    int32 pointBX, 
                    int32 pointBY, 
                    int32 options,
                    int32 color,
                    int32 *pFeatureIDResult) {
    ErrVal err = ENoErr;
    CBioCADShape *pShape = NULL;
    UNUSED_PARAM(pointAX);
    UNUSED_PARAM(pointAY);
    UNUSED_PARAM(pointBX);
    UNUSED_PARAM(pointBY);
    UNUSED_PARAM(options);
    UNUSED_PARAM(color);

    if (pFeatureIDResult) {
        *pFeatureIDResult = -1;
    }

    pShape = newex CBioCADShape;
    if (NULL == pShape) {
        gotoErr(EFail);
    }
    pShape->m_pSourceFile = m_pSourceFile;
    pShape->m_FeatureType = featureType;

    pShape->m_pOwnerImage = this;
    pShape->m_pNextShape = m_pShapeList;
    m_pShapeList = pShape;

    if (pFeatureIDResult) {
        *pFeatureIDResult = pShape->m_FeatureID;
    }
    pShape = NULL;

abort:
    delete pShape;
    returnErr(err);
} // AddFeature







/////////////////////////////////////////////////////////////////////////////
//
// [CountNeighborPixels]
//
/////////////////////////////////////////////////////////////////////////////
int32
C2DImageImpl::CountNeighborPixels(CBioCADShape *pShape, int32 x, int32 y) {
    CPixelInfo *pNWPixelState;
    CPixelInfo *pNPixelState;
    CPixelInfo *pNEPixelState;
    CPixelInfo *pWPixelState;
    CPixelInfo *pEPixelState;
    CPixelInfo *pSWPixelState;
    CPixelInfo *pSPixelState;
    CPixelInfo *pSEPixelState;
    int32 numNeighbors = 0;

    pNWPixelState = GetPixelState(x - 1, y - 1);
    if ((pNWPixelState) && (pNWPixelState->m_Flags & SHAPE_BOUNDARY_PIXEL) && (pShape == pNWPixelState->m_pShape)) {
        numNeighbors += 1;
    }
    pNPixelState = GetPixelState(x, y - 1);
    if ((pNPixelState) && (pNPixelState->m_Flags & SHAPE_BOUNDARY_PIXEL) && (pShape == pNPixelState->m_pShape)) {
        numNeighbors += 1;        
    }   
    pNEPixelState = GetPixelState(x + 1, y - 1);
    if ((pNEPixelState) && (pNEPixelState->m_Flags & SHAPE_BOUNDARY_PIXEL) && (pShape == pNEPixelState->m_pShape)) {
        numNeighbors += 1;        
    }

    pWPixelState = GetPixelState(x - 1, y);
    if ((pWPixelState) && (pWPixelState->m_Flags & SHAPE_BOUNDARY_PIXEL) && (pShape == pWPixelState->m_pShape)) {
        numNeighbors += 1;        
    }
    pEPixelState = GetPixelState(x + 1, y);
    if ((pEPixelState) && (pEPixelState->m_Flags & SHAPE_BOUNDARY_PIXEL) && (pShape == pEPixelState->m_pShape)) {
        numNeighbors += 1;        
    }    

    pSWPixelState = GetPixelState(x - 1, y + 1); 
    if ((pSWPixelState) && (pSWPixelState->m_Flags & SHAPE_BOUNDARY_PIXEL) && (pShape == pSWPixelState->m_pShape)) {
        numNeighbors += 1;
    }
    pSPixelState = GetPixelState(x, y + 1);
    if ((pSPixelState) && (pSPixelState->m_Flags & SHAPE_BOUNDARY_PIXEL) && (pShape == pSPixelState->m_pShape)) {
        numNeighbors += 1;
    }
    pSEPixelState = GetPixelState(x + 1, y + 1);
    if ((pSEPixelState) && (pSEPixelState->m_Flags & SHAPE_BOUNDARY_PIXEL) && (pShape == pSEPixelState->m_pShape)) {
        numNeighbors += 1;
    }

    return(numNeighbors);
} // CountNeighborPixels




 

/////////////////////////////////////////////////////////////////////////////
//
// [PixelsAppearOnSimilarPaths]
//
// <><> BUG.
// Really, this should trace the original image between the 2 border points 
// and try to see if the pixels between the points are still bright, just 
// not sharp enough to be a boundary.
/////////////////////////////////////////////////////////////////////////////
bool
C2DImageImpl::PixelsAppearOnSimilarPaths(CBioCADPoint *pPoint1, CBioCADPoint *pPoint2) {
    double slopeD;
    double floatY;
    double floatX;
    int32 x;
    int32 y;
    int32 startX;
    int32 startY;
    int32 endX;
    int32 endY;
    CPixelInfo *pPixelState;

    if ((NULL == pPoint1) || (NULL == pPoint2)) {
        return(false);
    }

    slopeD = (((double) pPoint1->m_Y) - ((double) pPoint2->m_Y)) / (((double) pPoint1->m_X) - ((double) pPoint2->m_X));


    ////////////////////////////////////////////
    // Relatively horizontal line
    // We walk up along the X coordinates and find the Y value for each X.
    // This ensures there is a pixel at every X, and the Y position may be a bit
    // jagged and may skip or duplicate.
    if ((slopeD > -(MAX_SLOPE_FOR_PATH_WALKING)) && (slopeD < MAX_SLOPE_FOR_PATH_WALKING)) {
        if (pPoint1->m_X < pPoint2->m_X) {
            startX = pPoint1->m_X;
            startY = pPoint1->m_Y;
            endX = pPoint2->m_X;
            endY = pPoint2->m_Y;
        } else {
            startX = pPoint2->m_X;
            startY = pPoint2->m_Y;
            endX = pPoint1->m_X;
            endY = pPoint1->m_Y;
        }
        // Recompute the slope so it increases from start to end.
        slopeD = (((double) endY) - ((double) startY)) / (((double) endX) - ((double) startX));

        floatY = (double) (startY);
        for (x = startX; x <= endX; x++) {
            y = (int32) floatY;     
        
            pPixelState = GetPixelState(x, y);
            if ((pPixelState) 
                && !(pPixelState->m_Flags & SHAPE_EXTERIOR_PIXEL)
                && !(pPixelState->m_Flags & SHAPE_BOUNDARY_PIXEL)) {
                return(false);
            }

            floatY += slopeD;
            if (floatY < 0) {
                floatY = 0;
            }
            if (floatY >= m_ImageHeight) {
                floatY = m_ImageHeight - 1;
            }
        } // for (x = pPoint1->m_X; x <= pPoint2->m_X; x++)
    }  
    ////////////////////////////////////////////
    // Relatively vertical line.
    /// We walk up each Y coordinate and find the X value for that.
    // This ensures there is a pixel at every Y, and the X position may be a bit
    // jagged and may skip or duplicate.
    else {
        if (pPoint1->m_Y < pPoint2->m_Y) {
            startX = pPoint1->m_X;
            startY = pPoint1->m_Y;
            endX = pPoint2->m_X;
            endY = pPoint2->m_Y;
        } else { 
            startX = pPoint2->m_X;
            startY = pPoint2->m_Y;
            endX = pPoint1->m_X;
            endY = pPoint1->m_Y;
        }
        // Recompute the slope so it increases from start to end.
        slopeD = (((double) endX) - ((double) startX)) / (((double) endY) - ((double) startY));

        floatX = (double) (startX);
        for (y = startY; y <= endY; y++) {
            x = (int32) floatX;   

            pPixelState = GetPixelState(x, y);
            if ((pPixelState) 
                   && !(pPixelState->m_Flags & SHAPE_EXTERIOR_PIXEL)
                   && !(pPixelState->m_Flags & SHAPE_BOUNDARY_PIXEL)) {
                return(false);
            }

            floatX += slopeD;
            if (floatX < 0) {
                floatX = 0;
            }
            if (floatX >= m_ImageWidth) {
                floatX = m_ImageWidth - 1;
            }
        } // for (y = startY; y <= endY; y++)
    } // Vertical Line


    return(true);
} // PixelsAppearOnSimilarPaths






/////////////////////////////////////////////////////////////////////////////
//
// [AddExtrapolatedBorderPoints]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
C2DImageImpl::AddExtrapolatedBorderPoints(
                    CBioCADShape *pShape,
                    CBioCADPoint *pPoint1, 
                    CBioCADPoint *pPoint2) {
    ErrVal err = ENoErr;
    double slopeD;
    double floatY;
    double floatX;
    int32 x;
    int32 y;
    int32 startX;
    int32 startY;
    int32 endX;
    int32 endY;
    CPixelInfo *pPixelState;


    if ((NULL == pShape) || (NULL == pPoint1) || (NULL == pPoint2)) {
        returnErr(EFail);
    }

    slopeD = (((double) pPoint1->m_Y) - ((double) pPoint2->m_Y)) / (((double) pPoint1->m_X) - ((double) pPoint2->m_X));


    ////////////////////////////////////////////
    // Relatively horizontal line
    // We walk up along the X coordinates and find the Y value for each X.
    // This ensures there is a pixel at every X, and the Y position may be a bit
    // jagged and may skip or duplicate.
    if ((slopeD > -(MAX_SLOPE_FOR_PATH_WALKING)) && (slopeD < MAX_SLOPE_FOR_PATH_WALKING)) {
        if (pPoint1->m_X < pPoint2->m_X) {
            startX = pPoint1->m_X;
            startY = pPoint1->m_Y;
            endX = pPoint2->m_X;
            endY = pPoint2->m_Y;
        } else {
            startX = pPoint2->m_X;
            startY = pPoint2->m_Y;
            endX = pPoint1->m_X;
            endY = pPoint1->m_Y;
        }
        // Recompute the slope so it increases from start to end.
        slopeD = (((double) endY) - ((double) startY)) / (((double) endX) - ((double) startX));

        floatY = (double) (startY);
        for (x = startX; x <= endX; x++) {
            y = (int32) floatY;     
        
            pPixelState = GetPixelState(x, y);
            if (pPixelState) {
                if (pPixelState->m_Flags & SHAPE_BOUNDARY_PIXEL) {
                    pPixelState->m_Flags &= ~DANGLING_BORDER_PIXEL;
                } else {
                    err = AddOneExtrapolatedPixel(pShape, x, y, pPixelState);
                    if (err) {
                        gotoErr(err);
                    }
                }
            }

            floatY += slopeD;
            if (floatY < 0) {
                floatY = 0;
            }
            if (floatY >= m_ImageHeight) {
                floatY = m_ImageHeight - 1;
            }
        } // for (x = pPoint1->m_X; x <= pPoint2->m_X; x++)
    }  
    ////////////////////////////////////////////
    // Relatively vertical line.
    /// We walk up each Y coordinate and find the X value for that.
    // This ensures there is a pixel at every Y, and the X position may be a bit
    // jagged and may skip or duplicate.
    else {
        if (pPoint1->m_Y < pPoint2->m_Y) {
            startX = pPoint1->m_X;
            startY = pPoint1->m_Y;
            endX = pPoint2->m_X;
            endY = pPoint2->m_Y;
        } else { 
            startX = pPoint2->m_X;
            startY = pPoint2->m_Y;
            endX = pPoint1->m_X;
            endY = pPoint1->m_Y;
        }
        // Recompute the slope so it increases from start to end.
        slopeD = (((double) endX) - ((double) startX)) / (((double) endY) - ((double) startY));

        floatX = (double) (startX);
        for (y = startY; y <= endY; y++) {
            x = (int32) floatX;   

            pPixelState = GetPixelState(x, y);
            if (pPixelState) {
                if (pPixelState->m_Flags & SHAPE_BOUNDARY_PIXEL) {
                    pPixelState->m_Flags &= ~DANGLING_BORDER_PIXEL;
                } else {
                    err = AddOneExtrapolatedPixel(pShape, x, y, pPixelState);
                    if (err) {
                        gotoErr(err);
                    }
                }
            }

            floatX += slopeD;
            if (floatX < 0) {
                floatX = 0;
            }
            if (floatX >= m_ImageWidth) {
                floatX = m_ImageWidth - 1;
            }
        } // for (y = startY; y <= endY; y++)
    } // Vertical Line


abort:
    returnErr(err);
} // AddExtrapolatedBorderPoints





/////////////////////////////////////////////////////////////////////////////
//
// [AddOneExtrapolatedPixel]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
C2DImageImpl::AddOneExtrapolatedPixel(CBioCADShape *pShape, int32 x, int32 y, CPixelInfo *pPixelState) {
    ErrVal err = ENoErr;
    CBioCADPoint *pPoint;

    if ((NULL == pShape) || (NULL == pPixelState)) {
        gotoErr(EFail);
    }

    pPoint = pShape->AddPoint(x, y, m_ZPlane);
    if (NULL == pPoint) {
        gotoErr(EFail);
    }
    
    pPixelState->m_Flags &= ~SHAPE_EXTERIOR_PIXEL;
    pPixelState->m_Flags |= SHAPE_BOUNDARY_PIXEL;
    pPixelState->m_Flags |= EXTRAPOLATED_PIXEL;
    //<>pPixelState->m_Flags |= DEBUG_HIGHLIGHT_PIXEL;
    pPixelState->m_pShape = pShape;

abort:
    returnErr(err);
} // AddOneExtrapolatedPixel

 





/////////////////////////////////////////////////////////////////////////////
//
// [BuildCrossSections]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
C2DImageImpl::BuildCrossSections() {
    ErrVal err = ENoErr;
    CBioCADShape *pShape = NULL;
    int32 index;
    CBioCADCrossSection *pCrossSection;
    CBioCADPoint *pCurrentPoint;    
    int32 uninitializedStartX = 10000000;
    int32 uninitializedStopX = 0;

    ///////////////////////////////////////
    // Build a list of Cross-sections for each shape. This is basically a run-length encoding
    // of the horizontal scan lines that pass through each shape. This is a faily small data
    // structure that is used a lot in later steps of the shape analysis.
    pShape = m_pShapeList;
    while (pShape) {
        // Allocate the cross-sections.
        pShape->m_NumCrossSections = (pShape->m_BoundingBoxBottomY - pShape->m_BoundingBoxTopY) + 1;
        pShape->m_pCrossSectionList = (CBioCADCrossSection *) (memAlloc(sizeof(CBioCADCrossSection) * pShape->m_NumCrossSections));
        if (NULL == pShape->m_pCrossSectionList) {
            gotoErr(EFail);
        }

        // Initialize the cross-sections.
        for (index = 0; index < pShape->m_NumCrossSections; index++) {
            pCrossSection = &(pShape->m_pCrossSectionList[index]);
            pCrossSection->m_Y = pShape->m_BoundingBoxTopY + index;
            // These initial values are a hige min and a small max, so it should be 
            // overwritten by any valid value.
            pCrossSection->m_StartX = uninitializedStartX;
            pCrossSection->m_StopX = uninitializedStopX;
        }

        // Use all known points (both boundary points and interior points) to 
        // Expand the cross-sections to the border of the shape. 
        pCurrentPoint = pShape->m_pPointList;
        while (pCurrentPoint) {
            index = (pCurrentPoint->m_Y - pShape->m_BoundingBoxTopY);
            pCrossSection = &(pShape->m_pCrossSectionList[index]);
            if (pCrossSection->m_Y != pCurrentPoint->m_Y) {
                pCurrentPoint->m_Y = pCurrentPoint->m_Y;
            }
            if (pCurrentPoint->m_X < pCrossSection->m_StartX) {
                pCrossSection->m_StartX = pCurrentPoint->m_X;
            }
            if (pCurrentPoint->m_X > pCrossSection->m_StopX) {
                pCrossSection->m_StopX = pCurrentPoint->m_X;
            }

            pCurrentPoint = pCurrentPoint->m_pNextPoint;
        } // while (pCurrentPoint)
        

        // Make sure each cross section has a reasonable start and stop point.
        // Some cross sections may have only 1 or no points at all.
        for (index = 0; index < pShape->m_NumCrossSections; index++) {
            pCrossSection = &(pShape->m_pCrossSectionList[index]);

            // If this is a single point, then it is ok if it is the top or bottom 
            // point in the shape.
            if ((pCrossSection->m_StartX == pCrossSection->m_StopX) 
                    && (index > 0)
                    && (index < (pShape->m_NumCrossSections - 1))) {
                // Otherwise, we do not know which point to trust, so replace 
                // both of them.
                pCrossSection->m_StartX = uninitializedStartX;
                pCrossSection->m_StopX = uninitializedStopX;
            }

            // Make sure there is a good starting point.
            if (pCrossSection->m_StartX == uninitializedStartX) {
                int32 upperIndex;
                int32 lowerIndex;
                CBioCADCrossSection *pNeighborCrossSection;

                // One good guess is to use the boundingBox.
                pCrossSection->m_StartX = pShape->m_BoundingBoxLeftX;

                // Another good guess is to use the previous line. We work down
                // through the cross sections, so we must have had either a known
                // boundary or a good guess for the previous cross section.
                upperIndex = index - 1;
                if (upperIndex >= 0) {
                    pNeighborCrossSection = &(pShape->m_pCrossSectionList[upperIndex]);
                    pCrossSection->m_StartX = pNeighborCrossSection->m_StartX;
                // Otherwise, this is the first line, so look below it.
                } else if (pShape->m_NumCrossSections > 2) {
                    lowerIndex = index + 1;
                    while (lowerIndex < pShape->m_NumCrossSections) {
                        pNeighborCrossSection = &(pShape->m_pCrossSectionList[lowerIndex]);
                        if ((pNeighborCrossSection->m_StartX != uninitializedStartX) 
                                && (pNeighborCrossSection->m_StartX != pCrossSection->m_StopX)) {
                            pCrossSection->m_StartX = pNeighborCrossSection->m_StartX;
                            break;
                        }
                        lowerIndex += 1;
                    } // while (lowerIndex < pShape->m_NumCrossSections)
                } // else if (pShape->m_NumCrossSections > 2)
            } // if (pCrossSection->m_StartX == uninitializedStartX)

            // Make sure there is a good stopping point.
            if (pCrossSection->m_StopX == uninitializedStopX) {
                int32 upperIndex;
                int32 lowerIndex;
                CBioCADCrossSection *pNeighborCrossSection;

                // One good guess is to use the boundingBox.
                pCrossSection->m_StopX = pShape->m_BoundingBoxRightX;

                // Another good guess is to use the previous line. We work down
                // through the cross sections, so we must have had either a known
                // boundary or a good guess for the previous cross section.
                upperIndex = index - 1;
                if (upperIndex >= 0) {
                    pNeighborCrossSection = &(pShape->m_pCrossSectionList[upperIndex]);
                    pCrossSection->m_StopX = pNeighborCrossSection->m_StopX;
                // Otherwise, this is the first line, so look below it.
                } else if (pShape->m_NumCrossSections > 2) {
                    lowerIndex = index + 1;
                    while (lowerIndex < pShape->m_NumCrossSections) {
                        pNeighborCrossSection = &(pShape->m_pCrossSectionList[lowerIndex]);
                        if ((pNeighborCrossSection->m_StopX != uninitializedStopX)
                                && (pNeighborCrossSection->m_StartX != pCrossSection->m_StopX)) {
                            pCrossSection->m_StopX = pNeighborCrossSection->m_StopX;
                            break;
                        }
                        lowerIndex += 1;
                    } // while (lowerIndex < pShape->m_NumCrossSections)
                } // else if (pShape->m_NumCrossSections > 2)
            } // if (pCrossSection->m_StopX == uninitializedStopX)
        }

//<><><><><><><><><>
#if 0
        for (index = 0; index < pShape->m_NumCrossSections; index++) {
            pCrossSection = &(pShape->m_pCrossSectionList[index]);

            // Record the shape in EVERY pixel in the cross sections.
            // This is useful when we look at marker pixels.
            for (int32 x = pCrossSection->m_StartX; x <= pCrossSection->m_StopX; x++) {
                CPixelInfo *pPixelInfo = GetPixelState(x, pCrossSection->m_Y);
                if (pPixelInfo) {
                    pPixelInfo->m_pShape = pShape;
                }
            }
        }
//<><><><><><><><><>
#endif


        pShape = pShape->m_pNextShape;
    } // while (pShape)
    
abort:
    returnErr(err);
} // BuildCrossSections






/////////////////////////////////////////////////////////////////////////////
//
// [RedrawProcessedImage]
//
// m_pSourceFile now has the updated image.
// This removes all noise and shape interiors.
/////////////////////////////////////////////////////////////////////////////
ErrVal
C2DImageImpl::RedrawProcessedImage(int32 options) {
    ErrVal err = ENoErr;
    int32 x;
    int32 y;

    // Optionally, erase the image, so we only draw pixels we consider to be part of shapes,
    // and not random background luminence or image noise.
    if (options & CELL_GEOMETRY_REDRAW_WITH_JUST_SHAPE_OUTLINES) {
        for (x = 0; x < m_ImageWidth; x++) {
            for (y = 0; y < m_ImageHeight; y++) {
                err = m_pSourceFile->SetPixel(x, y, g_BackGroundPixelColor);
                if (err) {
                    gotoErr(err);
                }
            }
        }
    } // if (options & CELL_GEOMETRY_REDRAW_WITH_JUST_SHAPE_OUTLINES)

abort:
    returnErr(err);
} // RedrawProcessedImage






/////////////////////////////////////////////////////////////////////////////
//
// [DrawFeatures]
//
// m_pSourceFile now has the updated image.
/////////////////////////////////////////////////////////////////////////////
void
C2DImageImpl::DrawFeatures(int32 options) {
    ErrVal err = ENoErr;
    int32 x;
    int32 y;
    CBioCADShape *pShape = NULL;
    int32 colorIndex;
    int32 *pShapeColorList;

    pShapeColorList = g_ColoredShapeColorList;
    g_BackGroundPixelColor = BLACK_PIXEL;
    g_ShapeInteriorColor = GREEN_PIXEL;

    if (options & CELL_GEOMETRY_DRAW_INTERIOR_AS_GRAY) {
        g_BackGroundPixelColor = WHITE_PIXEL;
        g_ShapeInteriorColor = LIGHT_GRAY_PIXEL;
        pShapeColorList = g_GrayShapeColorList;
        options |= CELL_GEOMETRY_DRAW_SHAPE_INTERIORS;
    }


    // Optionally, erase the image, so we only draw pixels we consider to be part of shapes,
    // and not random background luminence or image noise.
    if (options & CELL_GEOMETRY_REDRAW_WITH_JUST_SHAPE_OUTLINES) {
        for (x = 0; x < m_ImageWidth; x++) {
            for (y = 0; y < m_ImageHeight; y++) {
                err = m_pSourceFile->SetPixel(x, y, g_BackGroundPixelColor);
                if (err) {
                    gotoErr(err);
                }
            }
        }
    } // if (options & CELL_GEOMETRY_REDRAW_WITH_JUST_SHAPE_OUTLINES)


    // Now, reconstruct each shape from the detected points.
    pShape = m_pShapeList;
    colorIndex = 0;
    while (pShape) {
        int32 currentColor;
        currentColor = pShapeColorList[colorIndex];
        if (LIST_END_PIXEL == currentColor) {
            colorIndex = 0;
            currentColor = pShapeColorList[colorIndex];
        }
        colorIndex += 1;

        //<>  currentColor  RED_PIXEL
        pShape->DrawShape(currentColor, 0);
        pShape->DrawBoundingBox(currentColor);

        pShape = pShape->m_pNextShape;
    } // while (pShape)

    
    // Draw any special pixels.
    for (x = 0; x < m_ImageWidth; x++) {
        for (y = 0; y < m_ImageHeight; y++) {
            int32 pixelFlags = GetPixelFlags(x, y);

            if (pixelFlags & DEBUG_HIGHLIGHT_PIXEL) {
                (void) m_pSourceFile->SetPixel(x, y, RED_PIXEL);
            }
            if ((options & CELL_GEOMETRY_DRAW_SHAPE_INTERIORS) 
                    && !(SHAPE_EXTERIOR_PIXEL & pixelFlags)
                    && !(SHAPE_BOUNDARY_PIXEL & pixelFlags)) {
                (void) m_pSourceFile->SetPixel(x, y, g_ShapeInteriorColor);
            }
        }
    }


//////////////////////////////////////////
//<><><><><><><><><><><><><><><><><><><><>
#if 0
    // Draw the scan lines of each shape
    if (options & CELL_GEOMETRY_DRAW_SHAPE_SCANLINES) {
        int32 index;
        CBioCADCrossSection *pCrossSection;

        pShape = m_pShapeList;
        while (pShape) {
            for (index = 0; index < pShape->m_NumCrossSections; index++) {
                pCrossSection = &(pShape->m_pCrossSectionList[index]);
                for (x = pCrossSection->m_StartX; x <= pCrossSection->m_StopX; x++) {
                    m_pSourceFile->SetPixel(x, pCrossSection->m_Y, RED_PIXEL);
                }
            }
            pShape = pShape->m_pNextShape;
        } // while (pShape)
    } // if (options & CELL_GEOMETRY_DRAW_SHAPE_SCANLINES)
#endif
//<><><><><><><><><><><><><><><><><><><><>
//////////////////////////////////////////

abort:
    return;
} // DrawFeatures






/////////////////////////////////////////////////////////////////////////////
//
// [CopyRect]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
C2DImageImpl::CopyRect(
                    int32 srcLeftX, 
                    int32 srcTopY, 
                    int32 srcWidth, 
                    int32 srcHeight, 
                    int32 destLextX, 
                    int32 destTopY) {
    ErrVal err = ENoErr;
    int32 srcBottomY = srcTopY + srcHeight;
    int32 srcRightX = srcLeftX + srcWidth;
    int32 destBottomY = destTopY + srcHeight;
    int32 destRightX = destLextX + srcWidth;
    int32 currentDestRow;
    int32 currentSrcRow;
    int32 currentSrcColumn;
    int32 currentDestColumn;
    uint32 pixelValue;


    // Give up if the parameters are senseless.
    if ((srcTopY < 0) || (srcLeftX < 0) || (srcWidth < 0) || (srcHeight < 0) 
            || (destTopY < 0) || (destLextX < 0)
            || (srcTopY >= m_ImageHeight) || (srcLeftX >= m_ImageWidth)
            || (destTopY >= m_ImageHeight) || (destLextX >= m_ImageWidth)) {
        gotoErr(EFail);
    }

    // We copy differently depending on the position of the src and dest.
    // The src and dest may overlap, so we have to be careful to not clobber an
    // overlapping region before it has been copied.
    //////////////////////////////////////////////
    // Copy up. 
    // Start at the highest row and work our way down. By the time we
    // start clobbering the src, we have already previously copied those src rows.
    // Remember, (0,0) is the top-left corner, so Y increases as we move down.
    if (destTopY < srcTopY) {
        currentDestRow = destTopY;
        currentSrcRow = srcTopY;
        while (currentDestRow <= destBottomY) {
            // Clip copying to the image height.
            if ((currentDestRow >= m_ImageHeight) && (currentSrcRow >= m_ImageHeight)) {
                break;
            }
            err = m_pSourceFile->CopyPixelRow(srcLeftX, currentSrcRow, destLextX, currentDestRow, srcWidth);
            if (err) {
                gotoErr(err);
            }            

            currentDestRow = currentDestRow + 1;
            currentSrcRow = currentSrcRow + 1;
        } // while (currentRow <= destBottomY)

    //////////////////////////////////////////////
    // Copy down
    // Start at the lowest row and work our way up. By the time we
    // start clobbering the src, we have already previously copied those src rows.
    // Remember, (0,0) is the top-left corner, so Y increases as we move down.
    } else if (m_ImageHeight > srcBottomY) {
        currentDestRow = destBottomY;
        currentSrcRow = srcBottomY;
        while (currentDestRow >= destTopY) {
            // Clip copying to the image height.
            if ((currentDestRow < m_ImageHeight) && (currentSrcRow < m_ImageHeight)) {
                err = m_pSourceFile->CopyPixelRow(srcLeftX, currentSrcRow, destLextX, currentDestRow, srcWidth);
                if (err) {
                    gotoErr(err);
                }        
            }

            currentDestRow = currentDestRow - 1;
            currentSrcRow = currentSrcRow - 1;
        } // while (currentDestRow >= destTopY)


    //////////////////////////////////////////////
    // Copy left
    // This copies each column, starting at the left and moving to the right.
    } else if (destLextX < srcLeftX) {
        currentSrcColumn = srcLeftX;
        currentDestColumn = destLextX;
        while (currentDestColumn < destRightX) {
            // Now copy 1 pixel from each row in the current column.
            currentDestRow = destTopY;
            currentSrcRow = srcTopY;
            while (currentDestRow < destBottomY) {
                err = m_pSourceFile->GetPixel(currentSrcColumn, currentSrcRow, &pixelValue);
                if (err) {
                    gotoErr(err);
                }
                err = m_pSourceFile->SetPixel(currentDestColumn, currentDestRow, pixelValue);
                if (err) {
                    gotoErr(err);
                }

                currentDestRow += 1;
                currentSrcRow += 1;
            } // while (currentDestRow < destBottomY)

            // On to the next column.
            currentSrcColumn += 1;
            currentDestColumn += 1;
        } // while (currentDestColumn < destRightX)


    //////////////////////////////////////////////
    // Copy right
    // This copies each column starting at the right and moving left.
    } else if (destRightX > srcRightX) {
        currentSrcColumn = srcRightX;
        currentDestColumn = destRightX;
        while (currentDestColumn >= destLextX) {
            // Now copy 1 pixel from each row in the current column.
            currentDestRow = destTopY;
            currentSrcRow = srcTopY;
            while (currentDestRow < destBottomY) {
                err = m_pSourceFile->GetPixel(currentSrcColumn, currentSrcRow, &pixelValue);
                if (err) {
                    gotoErr(err);
                }
                err = m_pSourceFile->SetPixel(currentDestColumn, currentDestRow, pixelValue);
                if (err) {
                    gotoErr(err);
                }

                currentDestRow += 1;
                currentSrcRow += 1;
            } // while (currentDestRow < destBottomY)

            // On to the next column.
            currentSrcColumn = currentSrcColumn - 1;
            currentDestColumn = currentDestColumn - 1;
        } // while (currentDestColumn >= destRightX)


    //////////////////////////////////////////////
    // Otherwise, there is complete overlap so the src and dest are the same.
    // In this case, there is nothing to do.
    } else {
        gotoErr(ENoErr);
    }

abort:
    returnErr(err);
} // CopyRect





/////////////////////////////////////////////////////////////////////////////
//
// [CropImage]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
C2DImageImpl::CropImage(int32 newWidth, int32 newHeight) {
    ErrVal err = ENoErr;

    // Give up if the parameters are senseless.
    if ((newWidth < 0) || (newHeight < 0)
            || (newHeight > m_ImageHeight) || (newWidth > m_ImageWidth)) {
        gotoErr(EFail);
    }

    err = m_pSourceFile->CropImage(newWidth, newHeight);
    if (err) {
        gotoErr(err);
    }

    m_ImageWidth = newWidth;
    m_ImageHeight = newHeight;

abort:
    returnErr(err);
} // CropImage





/////////////////////////////////////////////////////////////////////////////
//
// [DrawEdges]
//
// m_pSourceFile now has the updated image.
/////////////////////////////////////////////////////////////////////////////
void
C2DImageImpl::DrawEdges(int32 options) {
    ErrVal err = ENoErr;
    CImageFile *pEdgeDetectionImage = NULL;
    char *pEdgeImageFileName = NULL;
    uint32 blackGrayScalePixel = 0; 
    uint32 whiteGrayScalePixel = 0; 
    int32 x;
    int32 y;

    pEdgeImageFileName = strCatEx(m_pImageFileName, GENERATED_LINE_DETECTION_FILE_SUFFIX);
    if (NULL == pEdgeImageFileName) {
       gotoErr(EFail);
    }

    pEdgeDetectionImage = MakeNewBMPImage(pEdgeImageFileName);
    if (NULL == pEdgeDetectionImage) {
        gotoErr(EFail);
    }

    err = pEdgeDetectionImage->InitializeFromSource(m_pSourceFile, 0xFFFFFFFF);
    if (err) {
        gotoErr(err);
    }

    blackGrayScalePixel = m_pSourceFile->ConvertGrayScaleToPixel(CImageFile::GRAYSCALE_BLACK); 
    whiteGrayScalePixel = m_pSourceFile->ConvertGrayScaleToPixel(CImageFile::GRAYSCALE_WHITE);


    // Erase the image, so we only draw pixels we consider to be part of shapes,
    // and not random background luminence or image noise.
    if (options & CELL_GEOMETRY_REDRAW_WITH_JUST_SHAPE_OUTLINES) {
        for (x = 0; x < m_ImageWidth; x++) {
            for (y = 0; y < m_ImageHeight; y++) {
                err = m_pSourceFile->SetPixel(x, y, WHITE_PIXEL);
                if (err) {
                    gotoErr(err);
                }
            }
        }
    } // if (options & CELL_GEOMETRY_REDRAW_WITH_JUST_SHAPE_OUTLINES)

    // Draw any special pixels.
    for (x = 0; x < m_ImageWidth; x++) {
        for (y = 0; y < m_ImageHeight; y++) {
            int32 pixelFlags = GetPixelFlags(x, y);

            if (pixelFlags & DEBUG_HIGHLIGHT_PIXEL) {
                err = pEdgeDetectionImage->SetPixel(x, y, blackGrayScalePixel);
            } else {
                err = pEdgeDetectionImage->SetPixel(x, y, whiteGrayScalePixel);
            }
            if (err) {
                gotoErr(err);
            }
        }
    }

    pEdgeDetectionImage->Save(0);
    pEdgeDetectionImage->CloseOnDiskOnly();

abort:
    if (pEdgeImageFileName) {
        memFree(pEdgeImageFileName);
    }
    if (pEdgeDetectionImage) {
        DeleteImageObject(pEdgeDetectionImage);
    }
} // DrawEdges








/////////////////////////////////////////////////////////////////////////////
//
// [DrawLine]
//
/////////////////////////////////////////////////////////////////////////////
void
C2DImageImpl::DrawLine(
                int32 pointAX, 
                int32 pointAY, 
                int32 pointBX, 
                int32 pointBY, 
                int32 lineColor) {
    ErrVal err = ENoErr;
    double slopeD;
    double floatY;
    double floatX;
    int32 x;
    int32 y;
    int32 startX;
    int32 startY;
    int32 endX;
    int32 endY;

    slopeD = (((double) pointAY) - ((double) pointBY)) / (((double) pointAX) - ((double) pointBX));

    ////////////////////////////////////////////
    // Relatively horizontal line
    // We walk up along the X coordinates and find the Y value for each X.
    // This ensures there is a pixel at every X, and the Y position may be a bit
    // jagged and may skip or duplicate.
    if ((slopeD > -(MAX_SLOPE_FOR_PATH_WALKING)) && (slopeD < MAX_SLOPE_FOR_PATH_WALKING)) {
        if (pointAX < pointBX) {
            startX = pointAX;
            startY = pointAY;
            endX = pointBX;
            endY = pointBY;
        } else {
            startX = pointBX;
            startY = pointBY;
            endX = pointAX;
            endY = pointAY;
        }
        // Recompute the slope so it increases from start to end.
        slopeD = (((double) endY) - ((double) startY)) / (((double) endX) - ((double) startX));

        floatY = (double) (startY);
        for (x = startX; x <= endX; x++) {
            y = (int32) floatY;     
        
            err = m_pSourceFile->SetPixel(x, y, lineColor);
            if (err) {
                gotoErr(err);
            }

            floatY += slopeD;
            if (floatY < 0) {
                floatY = 0;
            }
            if (floatY >= m_ImageHeight) {
                floatY = m_ImageHeight - 1;
            }
        } // for (x = pointAX; x <= pointBX; x++)

        // Make sure the start and end points are set. With rounding error, we cannot
        // guarantee that we will hit both.
        m_pSourceFile->SetPixel(startX, startY, lineColor);
        m_pSourceFile->SetPixel(endX, endY, lineColor);
    }  
    ////////////////////////////////////////////
    // Relatively vertical line.
    /// We walk up each Y coordinate and find the X value for that.
    // This ensures there is a pixel at every Y, and the X position may be a bit
    // jagged and may skip or duplicate.
    else {
        if (pointAY < pointBY) {
            startX = pointAX;
            startY = pointAY;
            endX = pointBX;
            endY = pointBY;
        } else { 
            startX = pointBX;
            startY = pointBY;
            endX = pointAX;
            endY = pointAY;
        }           
        // Recompute the slope so it increases from start to end.
        slopeD = (((double) endX) - ((double) startX)) / (((double) endY) - ((double) startY));

        floatX = (double) (startX);
        for (y = startY; y <= endY; y++) {
            x = (int32) floatX;   

            err = m_pSourceFile->SetPixel(x, y, lineColor);
            if (err) {
                gotoErr(err);
            }

            floatX += slopeD;
            if (floatX < 0) {
                floatX = 0;
            }
            if (floatX >= m_ImageWidth) {
                floatX = m_ImageWidth - 1;
            }
        } // for (y = startY; y <= endY; y++)

        // Make sure the start and end points are set. With rounding error, we cannot
        // guarantee that we will hit both.
        m_pSourceFile->SetPixel(startX, startY, lineColor);
        m_pSourceFile->SetPixel(endX, endY, lineColor);
    } // Vertical Line

abort:
    return;
} // DrawLine






/////////////////////////////////////////////////////////////////////////////
//
// [CreateInspectRegion]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
C2DImageImpl::CreateInspectRegion(
                    int32 positionType,
                    int32 topOffset, 
                    int32 bottomOffset, 
                    int32 leftOffset, 
                    int32 rightOffset, 
                    CBioCADShape **ppResult) {
    ErrVal err = ENoErr;
    CBioCADShape *pShape = NULL;
    CBioCADShape *pBestShape = NULL;
    int32 currentShapeSize;
    int32 bestShapeSize;
    float shapeOverlap;
    int32 middleX;
    int32 middleY;

    if (NULL == ppResult) {
        gotoErr(EFail);
    }
    *ppResult = NULL;
    if ((topOffset < 0) || (topOffset >= m_ImageHeight) 
            || (bottomOffset < 0) || (bottomOffset >= m_ImageHeight)
            || (leftOffset < 0) || (rightOffset >= m_ImageWidth)
            || (leftOffset > rightOffset) || (topOffset > bottomOffset)) {
        gotoErr(EFail);
    }


    pShape = newex CBioCADShape;
    if (NULL == pShape) {
        gotoErr(EFail);
    }
    pShape->m_pSourceFile = m_pSourceFile;
    pShape->m_FeatureType = CBioCADShape::FEATURE_TYPE_RECTANGLE;
    pShape->m_ShapeFlags = pShape->m_ShapeFlags | CBioCADShape::SOFTWARE_DISCOVERED;

    /////////////////////////////////////////////////
    if (INSPECTION_REGION_RELATIVE_T0_IMAGE_MIDDLE == positionType) {
        middleX = m_ImageWidth / 2;
        middleY = m_ImageHeight / 2;

        pShape->m_BoundingBoxLeftX = middleX - leftOffset;
        pShape->m_BoundingBoxRightX = middleX + rightOffset;
        pShape->m_BoundingBoxTopY = middleY - topOffset;
        pShape->m_BoundingBoxBottomY = middleY + bottomOffset;
    /////////////////////////////////////////////////
    } else if (INSPECTION_REGION_RELATIVE_T0_IMAGE_EDGES == positionType) {
        pShape->m_BoundingBoxLeftX = leftOffset;
        pShape->m_BoundingBoxRightX = m_ImageWidth - rightOffset;
        pShape->m_BoundingBoxTopY = topOffset;
        pShape->m_BoundingBoxBottomY = m_ImageHeight - bottomOffset;
    /////////////////////////////////////////////////
    } else if (INSPECTION_REGION_ABSOLUTE_COORDS == positionType) {
        pShape->m_BoundingBoxLeftX = leftOffset;
        pShape->m_BoundingBoxRightX = rightOffset;
        pShape->m_BoundingBoxTopY = topOffset;
        pShape->m_BoundingBoxBottomY = bottomOffset;
    /////////////////////////////////////////////////
    } else if (INSPECTION_REGION_FROM_EDGE_DETECTION == positionType) {
        pBestShape = NULL;
        pShape = m_pShapeList;
        while (pShape) {
            shapeOverlap = pShape->ComputeOverlap(topOffset, bottomOffset, leftOffset, rightOffset);
            if (shapeOverlap >= 0.6) {
                currentShapeSize = pShape->GetAreaInPixels();
                if ((NULL == pBestShape) || (currentShapeSize > bestShapeSize)) {
                    pBestShape = pShape;
                    bestShapeSize = currentShapeSize;
                }
            }
            pShape = pShape->m_pNextShape;
        } // while (pShape)
        pShape = pBestShape;
    /////////////////////////////////////////////////
    } else {
        gotoErr(EFail);
    }

    pShape->m_pOwnerImage = this;
    pShape->m_pNextShape = m_pInspectRegionList;
    m_pInspectRegionList = pShape;

    *ppResult = pShape;
    pShape = NULL;

abort:
    delete pShape;
    returnErr(err);
} // CreateInspectRegion



#if 0
    uint8 luminence;
    int8 luminenceDifference;
    uint8 luminence;
    uint32 pixelValue;
    uint32 red;
    uint32 green;
    uint32 blue;

        // Get the brightness of this pixel.
        luminence = m_pEdgeDetectionTable->GetLuminance(pCurrentPoint->m_X, pCurrentPoint->m_Y);
        err = m_pSourceFile->GetPixel(pCurrentPoint->m_X, pCurrentPoint->m_Y, &pixelValue);
        if (err) {
            gotoErr(err);
        }
        m_pSourceFile->ParsePixel(pixelValue, &blue, &green, &red);


    } else if (0) {
        // If it is not an edge, but it is very bright, then also include it.
        // Compare the difference in luminence.
        // Two dark pixels on a white background are the same, but so are 2 white pixels 
        // on a black background. So, we want pixels of similar luminence as the edge.
        luminence = m_pEdgeDetectionTable->GetLuminance(x, y);
        luminenceDifference = firstPixelLuminence - luminence;
        if (luminenceDifference < 0) {
            luminenceDifference = -luminenceDifference;
        }
        if (luminenceDifference <= MAX_LUMINENCE_DIFFERENCE_FOR_NEARBY_EDGE_PIXELS) {
            fPixelIsPossibleBorder = true;
        }


/////////////////////////////////////////////////////////////////////////////
//
// [PatchSmallGapsInBorder]
//
/////////////////////////////////////////////////////////////////////////////
void
C2DImageImpl::PatchSmallGapsInBorder(CBioCADShape *pShape) {
    ErrVal err = ENoErr;

    if (NULL == pShape) {
        gotoErr(EFail);
    }

abort:
    return;
} // PatchSmallGapsInBorder



#endif

