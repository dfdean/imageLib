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
// Geometry Utilities
//
/////////////////////////////////////////////////////////////////////////////

#include <math.h>

#if WASM
#include "portableBuildingBlocks.h"
#else
#include "buildingBlocks.h"
#endif

#include "imageLib.h"
#include "imageLibInternal.h"

FILE_DEBUGGING_GLOBALS(LOG_LEVEL_DEFAULT, 0);

static int g_NextFeatureID = 1;

static uint32 GetPixelLuminance(CImageFile *pSrcImage, int32 currentX, int32 currentY);



/////////////////////////////////////////////////////////////////////////////
//
// [CBioCADPoint]
//
/////////////////////////////////////////////////////////////////////////////
CBioCADPoint::CBioCADPoint() {
    m_X = 0;
    m_Y = 0;
    m_Z = 0;

    //m_TangentSlope = 0.0L;
    //m_OrthogonalSlope = 0.0L;
    
    m_pNextPoint = NULL;
} // CBioCADPoint




/////////////////////////////////////////////////////////////////////////////
//
// [GetDistanceBetweenPoints]
//
/////////////////////////////////////////////////////////////////////////////
double
GetDistanceBetweenPoints(CBioCADPoint *pPointA, CBioCADPoint *pPointB) {
    double xLength;
    double yLength;
    double zLength;
    double lineLength;

    if ((NULL == pPointA) || (NULL == pPointB)) {
        return(0L);
    }

    xLength = pPointA->m_X - pPointB->m_X;
    yLength = pPointA->m_Y - pPointB->m_Y;
    zLength = pPointA->m_Z - pPointB->m_Z;

    xLength = xLength * xLength;
    yLength = yLength * yLength;
    zLength = zLength * zLength;

    lineLength = xLength + yLength + zLength;
    lineLength = sqrt(lineLength);

    return(lineLength);
} // GetDistanceBetweenPoints






/////////////////////////////////////////////////////////////////////////////
//
// [CBioCADShape]
//
/////////////////////////////////////////////////////////////////////////////
CBioCADShape::CBioCADShape() {    
    m_FeatureType = FEATURE_TYPE_REGION;
    m_FeatureID = g_NextFeatureID;
    g_NextFeatureID = g_NextFeatureID + 1;
    m_ShapeFlags = 0;

    m_BoundingBoxLeftX = 0;
    m_BoundingBoxRightX = 0;
    m_BoundingBoxTopY = 0;
    m_BoundingBoxBottomY = 0;

    m_pPointList = NULL;
    m_NumPoints = 0;

    m_pCrossSectionList = NULL;
    m_NumCrossSections = 0;

    m_pOwnerImage = NULL;
    m_pNextShape = NULL;
} // CBioCADShape






/////////////////////////////////////////////////////////////////////////////
//
// [~CBioCADShape]
//
/////////////////////////////////////////////////////////////////////////////
CBioCADShape::~CBioCADShape() {    
    if (m_pCrossSectionList) {
        memFree(m_pCrossSectionList);
    }

    while (m_pPointList) {
        CBioCADPoint *pNextPoint = m_pPointList->m_pNextPoint;
        delete m_pPointList;
        m_pPointList = pNextPoint;
    }
} // ~CBioCADShape






/////////////////////////////////////////////////////////////////////////////
//
// [AddPoint]
//
/////////////////////////////////////////////////////////////////////////////
CBioCADPoint *
CBioCADShape::AddPoint(int32 x, int32 y, int32 z) {    
    CBioCADPoint *pNewPoint = NULL;

    pNewPoint = newex CBioCADPoint;
    if (pNewPoint) {
        pNewPoint->m_X = x;
        pNewPoint->m_Y = y;
        pNewPoint->m_Z = z;

        pNewPoint->m_pNextPoint = m_pPointList;
        m_pPointList = pNewPoint;
        m_NumPoints += 1;
    }

    return(pNewPoint);
} // AddPoint






/////////////////////////////////////////////////////////////////////////////
//
// [DrawShape]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
CBioCADShape::DrawShape(int32 color, int32 options) {
    ErrVal err = ENoErr;
    CBioCADPoint *pCurrentPoint = NULL;
    int32 x;
    int32 y;
    UNUSED_PARAM(options);
    
    if (NULL == m_pSourceFile) {
        gotoErr(EFail);
    }
    
    /////////////////////////////////////////////
    if (FEATURE_TYPE_RECTANGLE == m_FeatureType) {
        // Horizontal sides
        for (x = m_BoundingBoxLeftX; x <= m_BoundingBoxRightX; x++) {
            m_pSourceFile->SetPixel(x, m_BoundingBoxTopY, color);
        }
        for (x = m_BoundingBoxLeftX; x <= m_BoundingBoxRightX; x++) {
            m_pSourceFile->SetPixel(x, m_BoundingBoxBottomY, color);
        }
        // Vertical sides
        for (y = m_BoundingBoxTopY; y <= m_BoundingBoxBottomY; y++) {
            m_pSourceFile->SetPixel(m_BoundingBoxLeftX, y, color);
        }
        for (y = m_BoundingBoxTopY; y <= m_BoundingBoxBottomY; y++) {
            m_pSourceFile->SetPixel(m_BoundingBoxRightX, y, color);
        }
    /////////////////////////////////////////////
    } else {
        // Look for every pixel that matches this pixel.
        pCurrentPoint = m_pPointList;
        while (pCurrentPoint) {
            err = m_pSourceFile->SetPixel(pCurrentPoint->m_X, pCurrentPoint->m_Y, color);
            if (err) {
                gotoErr(err);
            }        
            pCurrentPoint = pCurrentPoint->m_pNextPoint;
        } // while (pCurrentPoint)
    }

abort:
    returnErr(err);
} // DrawShape







/////////////////////////////////////////////////////////////////////////////
//
// [FindShapeBoundingBox]
//
/////////////////////////////////////////////////////////////////////////////
void
CBioCADShape::FindBoundingBox() {
    CBioCADPoint *pPoint;  

    m_BoundingBoxLeftX = 0;
    m_BoundingBoxRightX = 0;
    m_BoundingBoxTopY = 0;
    m_BoundingBoxBottomY = 0;
    pPoint = m_pPointList;

    // The first point is special because it is the base case.
    // This means we dont have to check for the initial case on every 
    // other iteration.
    if (pPoint) {
        m_BoundingBoxRightX = pPoint->m_X;
        m_BoundingBoxLeftX = pPoint->m_X;
        m_BoundingBoxBottomY = pPoint->m_Y;
        m_BoundingBoxTopY = pPoint->m_Y;
        pPoint = pPoint->m_pNextPoint;
    } // if (pPoint)

    // Now examine every other point in the shape.
    while (pPoint) {
        if (pPoint->m_X > m_BoundingBoxRightX) {
            m_BoundingBoxRightX = pPoint->m_X;
        }
        if (pPoint->m_X < m_BoundingBoxLeftX) {
            m_BoundingBoxLeftX = pPoint->m_X;
        }
        if (pPoint->m_Y > m_BoundingBoxBottomY) {
            m_BoundingBoxBottomY = pPoint->m_Y;
        }
        if (pPoint->m_Y < m_BoundingBoxTopY) {
            m_BoundingBoxTopY = pPoint->m_Y;
        }

        pPoint = pPoint->m_pNextPoint;
    } // while (pPoint)
} // FindShapeBoundingBox





/////////////////////////////////////////////////////////////////////////////
//
// [DrawBoundingBox]
//
/////////////////////////////////////////////////////////////////////////////
void
CBioCADShape::DrawBoundingBox(int32 color) {
    ErrVal err = ENoErr;
    int32 x;
    int32 y;
        
    if (NULL == m_pSourceFile) {
        gotoErr(EFail);
    }
    
    // Horizontal sides
    for (x = m_BoundingBoxLeftX; x <= m_BoundingBoxRightX; x++) {
        m_pSourceFile->SetPixel(x, m_BoundingBoxTopY, color);
    }
    for (x = m_BoundingBoxLeftX; x <= m_BoundingBoxRightX; x++) {
        m_pSourceFile->SetPixel(x, m_BoundingBoxBottomY, color);
    }

    // Vertical sides
    for (y = m_BoundingBoxTopY; y <= m_BoundingBoxBottomY; y++) {
        m_pSourceFile->SetPixel(m_BoundingBoxLeftX, y, color);
    }
    for (y = m_BoundingBoxTopY; y <= m_BoundingBoxBottomY; y++) {
        m_pSourceFile->SetPixel(m_BoundingBoxRightX, y, color);
    }

abort:
    return;
} // DrawBoundingBox







/////////////////////////////////////////////////////////////////////////////
//
// [GetPixelStats]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
CBioCADShape::GetPixelStats(
                    uint32 *pTotalLuminence, 
                    uint32 *pAverageLuminence,
                    uint32 *pMinLuminence,
                    uint32 *pMaxLuminence,
                    uint32 *pNumPixelsChecked) {
    ErrVal err = ENoErr;
    CBioCADCrossSection *pCrossSection;
    int32 currentX;
    int32 currentY;
    int32 index;
    uint32 currentPixelLuminence;
    int32 numPixels;
    uint32 totalLuminence;
    uint32 minLuminence;
    uint32 maxLuminence;

    if (NULL == m_pSourceFile) {
        gotoErr(EFail);
    }
    if (pTotalLuminence) {
        *pTotalLuminence = 0;
    }
    if (pAverageLuminence) {
        *pAverageLuminence = 0;
    }
    if (pMinLuminence) {
        *pMinLuminence = 0;
    }
    if (pMaxLuminence) {
        *pMaxLuminence = 0;
    }
    if (pNumPixelsChecked) {
        *pNumPixelsChecked = 0;
    }
    numPixels = 0;
    totalLuminence = 0;
    minLuminence = 1024 * 1024;
    maxLuminence = 0;

    /////////////////////////////////////////////
    if (FEATURE_TYPE_RECTANGLE == m_FeatureType) {
        for (currentY = m_BoundingBoxTopY; currentY <= m_BoundingBoxBottomY; currentY++) {
            for (currentX = m_BoundingBoxLeftX; currentX <= m_BoundingBoxRightX; currentX++) {
                currentPixelLuminence = GetPixelLuminance(m_pSourceFile, currentX, currentY);
                totalLuminence += currentPixelLuminence;
                if (currentPixelLuminence < minLuminence) {
                    minLuminence = currentPixelLuminence;
                }
                if (currentPixelLuminence > maxLuminence) {
                    maxLuminence = currentPixelLuminence;
                }
                numPixels += 1;
            }
        }
    /////////////////////////////////////////////
    } else if (FEATURE_TYPE_REGION == m_FeatureType) {
        for (index = 0; index < m_NumCrossSections; index++) {
            pCrossSection = &(m_pCrossSectionList[index]);
            currentY = pCrossSection->m_Y;
            for (currentX = pCrossSection->m_StartX; currentX < pCrossSection->m_StopX; currentX++) {
                currentPixelLuminence = GetPixelLuminance(m_pSourceFile, currentX, currentY);
                totalLuminence += currentPixelLuminence;
                if (currentPixelLuminence < minLuminence) {
                    minLuminence = currentPixelLuminence;
                }
                if (currentPixelLuminence > maxLuminence) {
                    maxLuminence = currentPixelLuminence;
                }
                numPixels += 1;
            }
        }
    }

    if (pNumPixelsChecked) {
        *pNumPixelsChecked = numPixels;
    }
    if (pTotalLuminence) {
        *pTotalLuminence = totalLuminence;
    }
    if (pAverageLuminence) {
        *pAverageLuminence = (float) (((float)totalLuminence) / ((float)numPixels));
    }
    if (pMinLuminence) {
        *pMinLuminence = minLuminence;
    }
    if (pMaxLuminence) {
        *pMaxLuminence = maxLuminence;
    }

abort:
    returnErr(err);
} // GetPixelStats






/////////////////////////////////////////////////////////////////////////////
//
// [CountPixelsInLuminenceRange]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
CBioCADShape::CountPixelsInLuminenceRange(
                    uint32 minLuminence,
                    uint32 maxLuminence,
                    uint32 *pNumPixels,
                    float *pFractionOfRegion,
                    uint32 *pNumPixelsChecked) {
    ErrVal err = ENoErr;
    CBioCADCrossSection *pCrossSection;
    int32 currentX;
    int32 currentY;
    int32 index;
    uint32 currentPixelLuminence;
    int32 numPixels;
    int32 numPixelsChecked;

    if (NULL == m_pSourceFile) {
        gotoErr(EFail);
    }
    if (pNumPixels) {
        *pNumPixels = 0;
    }
    if (pFractionOfRegion) {
        *pFractionOfRegion = 0;
    }
    if (pNumPixelsChecked) {
        *pNumPixelsChecked = 0;
    }
    numPixels = 0;
    numPixelsChecked = 0;

    /////////////////////////////////////////////
    if (FEATURE_TYPE_RECTANGLE == m_FeatureType) {
        for (currentY = m_BoundingBoxTopY; currentY <= m_BoundingBoxBottomY; currentY++) {
            for (currentX = m_BoundingBoxLeftX; currentX <= m_BoundingBoxRightX; currentX++) {
                currentPixelLuminence = GetPixelLuminance(m_pSourceFile, currentX, currentY);
                if ((currentPixelLuminence >= minLuminence) && (currentPixelLuminence <= maxLuminence)) {
                    numPixels += 1;
                }
                numPixelsChecked += 1;
            }
        }
    /////////////////////////////////////////////
    } else if (FEATURE_TYPE_REGION == m_FeatureType) {
        for (index = 0; index < m_NumCrossSections; index++) {
            pCrossSection = &(m_pCrossSectionList[index]);
            currentY = pCrossSection->m_Y;
            for (currentX = pCrossSection->m_StartX; currentX < pCrossSection->m_StopX; currentX++) {
                currentPixelLuminence = GetPixelLuminance(m_pSourceFile, currentX, currentY);
                if ((currentPixelLuminence >= minLuminence) && (currentPixelLuminence <= maxLuminence)) {
                    numPixels += 1;
                }
                numPixelsChecked += 1;
            }
        }
    }

    if (pNumPixels) {
        *pNumPixels = numPixels;
    }
    if (pFractionOfRegion) {
        float totalPixelArea = (float) numPixelsChecked;
        *pFractionOfRegion = (float) (((float) numPixels) / totalPixelArea);
    }
    if (pNumPixelsChecked) {
        *pNumPixelsChecked = numPixelsChecked;
    }

abort:
    returnErr(err);
} // CountPixelsInLuminenceRange







/////////////////////////////////////////////////////////////////////////////
//
// [ComputeOverlap]
//
/////////////////////////////////////////////////////////////////////////////
float
CBioCADShape::ComputeOverlap(
                    int32 topOffset, 
                    int32 bottomOffset, 
                    int32 leftOffset, 
                    int32 rightOffset) {
    ErrVal err = ENoErr;
    CBioCADCrossSection *pCrossSection;
    int32 currentX;
    int32 currentY;
    int32 index;
    int32 totalNumPixels = 0;
    int32 numPixelsInOverlap = 0;
    int32 rowWidth;
    float ratio = 0.0;

    if (NULL == m_pSourceFile) {
        gotoErr(EFail);
    }
    totalNumPixels = 0;
    numPixelsInOverlap = 0;

    /////////////////////////////////////////////
    if (FEATURE_TYPE_RECTANGLE == m_FeatureType) {
        for (currentY = m_BoundingBoxTopY; currentY <= m_BoundingBoxBottomY; currentY++) {
            if ((currentY >= topOffset) && (currentY <= bottomOffset)) {
                for (currentX = m_BoundingBoxLeftX; currentX <= m_BoundingBoxRightX; currentX++) {
                    if ((currentX >= leftOffset) && (currentX <= rightOffset)) {
                        numPixelsInOverlap += 1;
                    }
                }
            } // if ((currentY >= topOffset) && (currentY <= bottomOffset)

            rowWidth = (m_BoundingBoxRightX - m_BoundingBoxLeftX) + 1;
            totalNumPixels += rowWidth;
        }
    /////////////////////////////////////////////
    } else if (FEATURE_TYPE_REGION == m_FeatureType) {
        for (index = 0; index < m_NumCrossSections; index++) {
            pCrossSection = &(m_pCrossSectionList[index]);
            currentY = pCrossSection->m_Y;
            if ((currentY >= topOffset) && (currentY <= bottomOffset)) {
                for (currentX = pCrossSection->m_StartX; currentX <= pCrossSection->m_StopX; currentX++) {
                    if ((currentX >= leftOffset) && (currentX <= rightOffset)) {
                        numPixelsInOverlap += 1;
                    }
                }
            } // if ((currentY >= topOffset) && (currentY <= bottomOffset)

            rowWidth = (pCrossSection->m_StopX - pCrossSection->m_StartX) + 1;
            totalNumPixels += rowWidth;
        }
    }


    if (totalNumPixels > 0) {
        ratio = (float) (((float) numPixelsInOverlap) / totalNumPixels);
    }

abort:
    return(ratio);
} // ComputeOverlap








/////////////////////////////////////////////////////////////////////////////
//
// [GetAreaInPixels]
//
/////////////////////////////////////////////////////////////////////////////
int32
CBioCADShape::GetAreaInPixels() {
    CBioCADCrossSection *pCrossSection;
    int32 result = 0;
    int32 index;
    int32 deltaX;

    /////////////////////////////////////////////
    if (FEATURE_TYPE_RECTANGLE == m_FeatureType) {
        // This includes the right and bottom pixel, not stopping before the 
        // bottom and right pixel. As a result, we have to add 1.
        int32 width = (m_BoundingBoxRightX - m_BoundingBoxLeftX) + 1;
        int32 height = (m_BoundingBoxBottomY - m_BoundingBoxTopY) + 1;
        result = (width * height);

    /////////////////////////////////////////////
    } else if (FEATURE_TYPE_REGION == m_FeatureType) {
        for (index = 0; index < m_NumCrossSections; index++) {
            pCrossSection = &(m_pCrossSectionList[index]);
            if (pCrossSection->m_StopX >= pCrossSection->m_StartX) {
                // This includes the stop pixel, not stopping before the 
                // stop pixel. As a result, we have to add 1.
                deltaX = (pCrossSection->m_StopX - pCrossSection->m_StartX) + 1;
                result += deltaX;
            }
        }
    } else {
        result = 0;
    }

    return(result);
} // GetAreaInPixels








/////////////////////////////////////////////////////////////////////////////
//
// [CBioCADLine]
//
/////////////////////////////////////////////////////////////////////////////
CBioCADLine::CBioCADLine() {
    m_LineFlags = 0;

    m_Length = 0.0L;

    m_Slope = 0.0L;
    m_YIntercept = 0.0L;
    m_AngleWithHorizontal = 0.0L;

    m_NumPixels = 0;
    m_PixelList = NULL;

    m_pNextLine = NULL;
} // CBioCADLine






/////////////////////////////////////////////////////////////////////////////
//
// [GetLength]
//
/////////////////////////////////////////////////////////////////////////////
double
CBioCADLine::GetLength() {
    if (0.0L == m_Length) {
        m_Length = GetDistanceBetweenPoints(&m_PointA, &m_PointB);
    }
    return(m_Length);
} // GetLength





/////////////////////////////////////////////////////////////////////////////
//
// [DrawLineToImage]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal 
CBioCADLine::DrawLineToImage(CImageFile *pDestImage, int32 color, int32 options) {
    ErrVal err = ENoErr;
    CBioCADPoint *pPixel;
    int32 maxXPos;
    int32 maxYPos;
    uint32 blackPixel;
    UNUSED_PARAM(options);
    
    if (NULL == pDestImage) {
        gotoErr(EFail);
    }
    
    err = pDestImage->GetImageInfo(&maxXPos, &maxYPos);
    if (err) {
        gotoErr(err);
    }
    blackPixel = pDestImage->ConvertGrayScaleToPixel(color);


    pPixel = m_PixelList;
    while (pPixel) {
        err = pDestImage->SetPixel(pPixel->m_X, pPixel->m_Y, blackPixel);
        if (err) {
            gotoErr(err);
        }

        pPixel = pPixel->m_pNextPoint;
    } // while (pPixel)
    
abort:
    returnErr(err);
} // DrawLineToImage



    


/////////////////////////////////////////////////////////////////////////////
//
// [CBioCADLineSet]
//
/////////////////////////////////////////////////////////////////////////////
CBioCADLineSet::CBioCADLineSet() {
    m_pLineList = NULL;
    m_NumLines = 0;
    m_MaxLines = 0;
    m_fAllocatedLines = false;

    m_pRemovedLines = NULL;

    m_pNextGraph = NULL;
} // CBioCADLineSet




/////////////////////////////////////////////////////////////////////////////
//
// [~CBioCADLineSet]
//
/////////////////////////////////////////////////////////////////////////////
CBioCADLineSet::~CBioCADLineSet() {
    DiscardLines();
} // ~CBioCADLineSet





/////////////////////////////////////////////////////////////////////////////
//
// [DiscardLines]
//
/////////////////////////////////////////////////////////////////////////////
void
CBioCADLineSet::DiscardLines() {
    CBioCADLine *pNextLine;
    CBioCADLine *pLine;
    int32 index;

    if (m_pLineList) {
        if (m_fAllocatedLines) {
            for (index = 0; index < m_NumLines; index++) {
                pLine = m_pLineList[index];
                delete pLine;
            }
        }

        memFree(m_pLineList);
        m_pLineList = NULL;
    }
    m_NumLines = 0;
    m_MaxLines = 0;
    m_fAllocatedLines = false;  


    if (m_pRemovedLines) {
        while (m_pRemovedLines) {
            pNextLine = m_pRemovedLines->m_pNextLine;
            delete m_pRemovedLines;
            m_pRemovedLines = pNextLine;
        }

        m_pRemovedLines = NULL;
    }
} // DiscardLines





/////////////////////////////////////////////////////////////////////////////
//
// [SetLineList]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
CBioCADLineSet::SetLineList(CBioCADLine *pLineList) {
    ErrVal err = ENoErr;
    CBioCADLine *pLine;
    int32 numNewLines;


    DiscardLines();

    numNewLines = 0;
    pLine = pLineList;
    while (pLine) {
        numNewLines += 1;
        pLine = pLine->m_pNextLine;
    }
    
    m_MaxLines = numNewLines + 10;
    m_pLineList = (CBioCADLine **) (memAlloc(sizeof(CBioCADLine *) * m_MaxLines));
    if (NULL == m_pLineList) {
        gotoErr(EFail);
    }

    m_NumLines = 0;
    pLine = pLineList;
    while (pLine) {
        m_pLineList[m_NumLines] = pLine;
        pLine = pLine->m_pNextLine;
        m_NumLines += 1;
    }

    // We own these lines now, so we have to delete them.
    m_fAllocatedLines = true; 

abort:
    returnErr(err);
} // SetLineList





    
/////////////////////////////////////////////////////////////////////////////
//
// [GetLineList]
//
/////////////////////////////////////////////////////////////////////////////
CBioCADLine *
CBioCADLineSet::GetLineList() {
    CBioCADLine *pFirstLine = NULL;
    CBioCADLine *pLastLine = NULL;
    CBioCADLine *pLine;
    int32 index;

    for (index = 0; index < m_NumLines; index++) {
        pLine = m_pLineList[index];

        pLine->m_pNextLine = NULL;
        if (pLastLine) {
            pLastLine->m_pNextLine = pLine;
        }
        pLastLine = pLine;
        if (NULL == pFirstLine) {
            pFirstLine = pLine;
        }
    }

    return(pFirstLine);
} // GetLineList






/////////////////////////////////////////////////////////////////////////////
//
// [FilterLines]
//
// This method is only here to remove lines that should not have been generated
// initially. This may be covering up sloppiness in the original image. For
// example, a single line may be broken up and appear as a dashed line.
/////////////////////////////////////////////////////////////////////////////
void
CBioCADLineSet::FilterLines(int32 criteria, int32 value) {
    int32 srcIndex;
    int32 destIndex;
    int32 numPruned;
    CBioCADLine *pLine = NULL;

    if ((0 == m_NumLines) || (NULL == m_pLineList)) {
        goto abort;
    }

    // Just to be safe, clear the LINE_TEMP_PRUNED flag.
    for (srcIndex = 0; srcIndex < m_NumLines; srcIndex++) {
        pLine = m_pLineList[srcIndex];
        pLine->m_LineFlags &= ~CBioCADLine::LINE_TEMP_PRUNED;
    } // for (srcIndex = 0; srcIndex < m_NumLines; srcIndex++)


    numPruned = 0;
    for (srcIndex = 0; srcIndex < m_NumLines; srcIndex++) {
        pLine = m_pLineList[srcIndex];
        if (!(pLine->m_LineFlags & CBioCADLine::LINE_TEMP_PRUNED)) {
            //////////////////////////////////////////////
            if (FILTER_BY_MIN_LENGTH == criteria) {
                if (pLine->GetLength() < value) {
                    numPruned += 1;
                    pLine->m_LineFlags |= CBioCADLine::LINE_TEMP_PRUNED;
                }
            }
            //////////////////////////////////////////////
            if (FILTER_BY_MIN_PIXEL_DENSITY == criteria) {
                double density = pLine->m_NumPixels / pLine->GetLength();
                if (density < value) {
                    numPruned += 1;
                    pLine->m_LineFlags |= CBioCADLine::LINE_TEMP_PRUNED;
                }
            }
        } // if (!(pLine->m_LineFlags & CBioCADLine::LINE_TEMP_PRUNED))
    } // for (srcIndex = 0; srcIndex < m_NumLines; srcIndex++)
    

    // Now, discard all pruned lines.
    // srcIndex is always >= destIndex. 
    destIndex = 0;
    for (srcIndex = 0; srcIndex < m_NumLines; srcIndex++) {
        pLine = m_pLineList[srcIndex];
        if (pLine->m_LineFlags & CBioCADLine::LINE_TEMP_PRUNED) {
            if (m_fAllocatedLines) {
                delete pLine;
            }  
        } else {
            m_pLineList[destIndex] = pLine;
            destIndex += 1;          
        }
    }
    m_NumLines = destIndex;

abort:
    return;
} // FilterLines








/////////////////////////////////////////////////////////////////////////////
//
// [GetPixelLuminance]
//
// Get the intensity values for red, blue, and green. 
//
//          grayscale luminance = (0.30 * red) + (0.59 * green) + (0.11 * blue)
//
// Note, together these add up to 1.0, so we are just weighting red, green, and blue
// and then summing them. 
//
/////////////////////////////////////////////////////////////////////////////
uint32
GetPixelLuminance(CImageFile *pImageFile, int32 currentX, int32 currentY) {
    ErrVal err = ENoErr;
    uint32 pixelValue;
    uint32 red = 0;
    uint32 green = 0;
    uint32 blue = 0;
    float redFloat;
    float greenFloat;
    float blueFloat;
    float totalFloat;
    uint32 luminance = 0;

    if (NULL == pImageFile) {
        gotoErr(EFail);
    }

    err = pImageFile->GetPixel(currentX, currentY, &pixelValue);
    if (err) {
        gotoErr(err);
    }
    pImageFile->ParsePixel(pixelValue, &blue, &green, &red);

    if (1) {
        luminance = red + green + blue;
    } else {
        redFloat = (float) red;
        greenFloat = (float) green;
        blueFloat = (float) blue;

        redFloat = redFloat * (float) 0.30;
        greenFloat = greenFloat * (float) 0.59;
        blueFloat = blueFloat * (float) 0.11;

        totalFloat = redFloat + greenFloat + blueFloat;
        luminance = (uint32) totalFloat;
    }

abort:
    return(luminance);
} // GetPixelLuminance








