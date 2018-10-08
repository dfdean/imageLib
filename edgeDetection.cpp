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
// Edge Detection for Image Analyzer
// =================================
//
// Read an image, and generate a new image that shows just the border
// lines in the image. This works by finding points in the image where 
// the "brightness" changes sharply.
//
// The "brightness" of each point in the image is measured by its grayscale
// value. So, the first step is to convert each RGB pixel into a grayscale. 
// We do this by first getting the intensity values for red, blue, and green
// at a pixel, and then using this equation:
//
//          grayscale luminance = (0.30 * red) + (0.59 * green) + (0.11 * blue)
//
// Note, together these add up to 1.0, so we are just weighting red, green, and blue
// and then summing them. Intuitively, "blue" is a darker color, so it contributes 
// less to the overall luminance than a brighter color like green.
//
// Now, we create a new pixel array with the luminance of each pixel instead of
// the original pixel. Now we are ready to detect edges.
//
// The "Sobel" operator compute how much the image is CHANGING its brightness at each 
// point. In other words, we replace a pixel with a value for the change in brightness 
// at that position. To compute this change, we first compute how much the image is changing
// in the left-to-right direction, or X axis, and separately compute how much it is changing
// in the up-to-down direction, or Y axis. We could look at a lot of pixels to do this,
// but for simplicity, we just look at adjacent pixels.
//
// For example, suppose there is a grid of 9 pixels like this:
//         10    15    20
//         30    35    40
//         50    55    60
// Remember, these are all grayscale values.
//
// We are replacing the pixel 35 with a new value that is the change in the image
// at that position. Start by computing how much the image changes in
// the X and Y directions. In the simplest form, we could just compute:
//    change-in-X-Direction = pixelToRight - pixelToLeft
//    change-in-Y-Direction = pixelBelow - pixelAbove
//
// In our example array of pixels, this would be:
//    change-in-X-Direction = 40 - 30 = 10
//    change-in-Y-Direction = 55 - 15 = 40
//    
// OK, but this is a bit TOO simplistic. We can be smarter and also include the
// pixels in the diagonals in our calculation. For example, we could say:
//
//    change-in-X-Direction = (pixelToRight + pixelAboveRight + pixelBelowRight)
//                              - (pixelToLeft + pixelAboveLeft + pixelBelowLeft)
//
//    change-in-Y-Direction = (pixelBelow + pixelBelowLeft + pixelBelowRight)
//                              - (pixelAbove + pixelAboveLeft + pixelAboveRight)
//
// This is better, but not great. It considers change along the diagonals to be as important
// as change directly up-and-down or left-to-right. We want to include the diagonals, but 
// weigh their contribution less. So, we just scale up the parts we want to be more important.
//
//    change-in-X-Direction = (2*pixelToRight + pixelAboveRight + pixelBelowRight) 
//                             - (2*pixelToLeft + pixelAboveLeft + pixelBelowLeft)
//
//    change-in-Y-Direction = (2*pixelBelow + pixelBelowLeft + pixelBelowRight) 
//                             - (2*pixelAbove + pixelAboveLeft + pixelAboveRight)
//
// Now the biggest contributors in the Y direction are directly above and below.
// The upper and lower diagonals contribute, but less so. Similarly, the biggest 
// contributors on the X direction are immediately to the left and right. Again, 
// the diagonals contribute, but less so.
//
// And, this is the "Sobel" operator. For simplicity, it is written like this:
//
//      Gx = | -1    0    1 |
//           | -2    0    2 |
//           | -1    0    1 |
//
//      Gy = | -1   -2   -1 |
//           | 0     0    0 |
//           | 1     2    1 |
//
// Note, these are NOT matrices, but rather the coefficients for how we add the 
// grayscales of adjacent pixels.
//
// Now, we have the change in the X and Y direction, we combine them to make a total
// change. In effect, we combine the X and Y vectors. The total change is just adding
// these components, or Pythagorean. 
//
//              G = Sqrt(Gx**2 + Gy**2)
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

#define MAX_GRADIENT_FOR_STRAIGHT_LINE      10






/////////////////////////////////////////////////////////////////////////////
//
// [AllocateEdgeDetectionTable]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
AllocateEdgeDetectionTable(CImageFile *pImageFile, CEdgeDetectionTable **ppResult) {
    ErrVal err = ENoErr;
    CEdgeDetectionTable *pMap = NULL;
    int32 totalNumPixels;

    if ((NULL == pImageFile) || (NULL == ppResult)) {
        gotoErr(EFail);
    }
    *ppResult = NULL;


    pMap = newex CEdgeDetectionTable;
    if (NULL == pMap) {
        gotoErr(EFail);
    }

    err = pImageFile->GetImageInfo(&(pMap->m_MaxXPos), &(pMap->m_MaxYPos));
    if (err) {
        gotoErr(err);
    }

    totalNumPixels = pMap->m_MaxXPos * pMap->m_MaxYPos;
    pMap->m_pInfoTable = (CEdgeDetectionEntry *) memAlloc(sizeof(CEdgeDetectionEntry) * totalNumPixels);
    if (NULL == pMap->m_pInfoTable) {
        gotoErr(EFail);
    }

    *ppResult = pMap;
    pMap = NULL;

abort:
    delete pMap;
    returnErr(err);
} // AllocateEdgeDetectionTable







/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
CEdgeDetectionTable::CEdgeDetectionTable() {
    m_MaxXPos = 0;
    m_MaxYPos = 0;
    m_pInfoTable = NULL;
} // CEdgeDetectionTable




/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
CEdgeDetectionTable::~CEdgeDetectionTable() {
    memFree(m_pInfoTable);
} // ~CEdgeDetectionTable




/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
uint8 
CEdgeDetectionTable::GetLuminance(int32 x, int32 y) {
    CEdgeDetectionEntry *pEntry;

    if (x < 0) {
        x = 0;
    }
    if (y < 0) {
        y = 0;
    }
    if (x > m_MaxXPos) {
        x = m_MaxXPos;
    }
    if (y > m_MaxYPos) {
        y = m_MaxYPos;
    }

    pEntry = &(m_pInfoTable[(y * m_MaxXPos) + x]);
    return(pEntry->m_GrayScaleValue);
} // GetLuminance





/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
bool 
CEdgeDetectionTable::IsEdge(int32 x, int32 y) {
    CEdgeDetectionEntry *pEntry;

    if (x < 0) {
        x = 0;
    }
    if (y < 0) {
        y = 0;
    }
    if (x > m_MaxXPos) {
        x = m_MaxXPos;
    }
    if (y > m_MaxYPos) {
        y = m_MaxYPos;
    }

    pEntry = &(m_pInfoTable[(y * m_MaxXPos) + x]);
    if (pEntry->m_IsEdge > 0) {
        return(true);
    } else {
        return(false);
    }
} // IsEdge







/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
uint8 
CEdgeDetectionTable::GetGradientDirection(int32 x, int32 y) {
    CEdgeDetectionEntry *pEntry;

    if (x < 0) {
        x = 0;
    }
    if (y < 0) {
        y = 0;
    }
    if (x > m_MaxXPos) {
        x = m_MaxXPos;
    }
    if (y > m_MaxYPos) {
        y = m_MaxYPos;
    }

    pEntry = &(m_pInfoTable[(y * m_MaxXPos) + x]);
    return(pEntry->m_GradientDirection);
} // GetGradientDirection






/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
int32 
CEdgeDetectionTable::GetGradient(int32 x, int32 y) {
    CEdgeDetectionEntry *pEntry;

    if (x < 0) {
        x = 0;
    }
    if (y < 0) {
        y = 0;
    }
    if (x > m_MaxXPos) {
        x = m_MaxXPos;
    }
    if (y > m_MaxYPos) {
        y = m_MaxYPos;
    }

    pEntry = &(m_pInfoTable[(y * m_MaxXPos) + x]);
    return(pEntry->m_Gradient);
} // GetGradient





/////////////////////////////////////////////////////////////////////////////
//
// [Initialize]
//
// The main procedure for edge-detection.
/////////////////////////////////////////////////////////////////////////////
ErrVal
CEdgeDetectionTable::Initialize(
                            CImageFile *pSrcImage, 
                            uint32 blackWhiteThreshold) {
    ErrVal err = ENoErr;
    CEdgeDetectionEntry *pEntry;
    uint32 luminance = 0;
    uint32 pixelValue;
    int32 x;
    int32 y;   

    if (NULL == pSrcImage) {
        gotoErr(EFail);
    }

    // Build up a grayscale map of the image.
    // Do this so we only have to compute the grayscale of each pixel once.
    for (x = 0; x < m_MaxXPos; x++) {
        for (y = 0; y < m_MaxYPos; y++) {
            err = pSrcImage->GetPixel(x, y, &pixelValue);
            if (err) {
                gotoErr(err);
            }

            luminance = GetPixelLuminance(pSrcImage, pixelValue);

            pEntry = &((m_pInfoTable)[(y * m_MaxXPos) + x]);
            pEntry->m_GrayScaleValue = (uint8) (luminance);
            pEntry->m_IsEdge = 0;
        } // for (y = 0; y < pMap->m_MaxYPos; y++)
    } // for (x = 0; x < m_MaxXPos; x++)

    // Build up a grayscale map of the image.
    // Do this so we only have to compute the grayscale of each pixel once.
    for (x = 0; x < m_MaxXPos; x++) {
        for (y = 0; y < m_MaxYPos; y++) {
            // Leave all these as signed. The grayscale values are unsigned 0-255 
            // values, but we want to convert this into changes in luminance, 
            // which can be positive or negative.
            uint8 pixelAbove;
            uint8 pixelBelow;
            uint8 pixelLeft;
            uint8 pixelRight;
            uint8 pixelAboveLeft;
            uint8 pixelAboveRight;
            uint8 pixelBelowLeft;
            uint8 pixelBelowRight;
            int32 xChange;
            int32 yChange;
            int32 absXChange;
            int32 absYChange;
            int32 rawLuminanceChange = 0;

            pEntry = &(m_pInfoTable[(y * m_MaxXPos) + x]);

            // Get the luminance of all surrounding pixels
            // Get the luminance of all surrounding pixels
            pixelAbove = GetLuminance(x, y-1);
            pixelBelow = GetLuminance(x, y+1); 
            pixelLeft = GetLuminance(x-1, y);
            pixelRight = GetLuminance(x+1, y);
            pixelAboveLeft = GetLuminance(x-1, y-1);
            pixelAboveRight = GetLuminance(x+1, y-1);
            pixelBelowLeft = GetLuminance(x-1, y+1);
            pixelBelowRight = GetLuminance(x+1, y+1);

            // Use the comvolution matrices to get the change in the X and Y dimensions.
            xChange = ((2 * pixelRight) + pixelAboveRight + pixelBelowRight) 
                - ((2 * pixelLeft) + pixelAboveLeft + pixelBelowLeft);
            yChange = ((2 * pixelAbove) + pixelAboveLeft + pixelAboveRight) 
                - ((2 * pixelBelow) + pixelBelowLeft + pixelBelowRight);
            absXChange = xChange;
            if (absXChange < 0) {
                absXChange = -absXChange;
            }
            absYChange = yChange;
            if (absYChange < 0) {
                absYChange = -absYChange;
            }

            // Calculate the change in luminosity. 
            // There are several ways to do this.
            // 1. This adds the basis vectors, or just computes the Pythagorean distance.
            float xSquared = (float) (xChange * xChange);
            float ySquared = (float) (yChange * yChange);
            rawLuminanceChange = (int32) (float) sqrt(xSquared + ySquared);
            // 2. ManhattanDistance: newGrayScale = abs(xChange) + abs(yChange);


            // The distance can be a value bigger than a max luminence.
            // Remember, we are subtracting two different sums of luminences, which are
            // are uint8's. So, adjust the distance if it's greater than 
            // 255 or less than zero, which is out of color range.
            // Really, this is the sqrt of the sum of squares, so it should never be < 0.
            if (rawLuminanceChange > 255) {
                rawLuminanceChange = 255;
            }
            if (rawLuminanceChange < 0) {
                rawLuminanceChange = 0;
            }

             // I want this for detecting lines, and I really only want black and white. 
            // So, I use a threshold for a black color. If it's slightly gray (below the threshold), 
            // then I ignore it and color it white. Obviously, the specific threshold value is
            // important, and may be tuned for different images.
            if (blackWhiteThreshold > 0) {
                if (((uint32) rawLuminanceChange) >= blackWhiteThreshold) {
                    pEntry->m_IsEdge = 1;
                    pEntry->m_Gradient = rawLuminanceChange;

                    // If this changes mostly in a horizontal direction.
                    if (absYChange <= MAX_GRADIENT_FOR_STRAIGHT_LINE) {
                        if (xChange >= 0) { 
                            pEntry->m_GradientDirection = PIXELS_BRIGHTER_WEST_TO_EAST;
                        } else { // if (xChange < 0) 
                            pEntry->m_GradientDirection = PIXELS_BRIGHTER_EAST_TO_WEST;
                        } 
                    // If this changes mostly in a horizontal direction.
                    } else if (absXChange <= MAX_GRADIENT_FOR_STRAIGHT_LINE) {
                        if (yChange >= 0) {
                            pEntry->m_GradientDirection = PIXELS_BRIGHTER_SOUTH_TO_NORTH;
                        } else { // if (yChange < 0)
                            pEntry->m_GradientDirection = PIXELS_BRIGHTER_NORTH_TO_SOUTH;
                        }
                    // If this changes in both x and y and also grows toward the right
                    // then it is headed either NE ot SE
                    } else if (xChange >= 0) {
                        if (yChange >= 0) {
                            pEntry->m_GradientDirection = PIXELS_BRIGHTER_SW_TO_NE;
                        } else { // if (yChange < 0)
                            pEntry->m_GradientDirection = PIXELS_BRIGHTER_NW_TO_SE;
                        }
                    // If this changes in both x and y and also grows toward the left
                    // then it is headed either NW ot SW
                    } else { // if (xChange < 0) {
                        if (yChange >= 0) {
                            pEntry->m_GradientDirection = PIXELS_BRIGHTER_SE_TO_NW;
                        } else { // if (yChange < 0)
                            pEntry->m_GradientDirection = PIXELS_BRIGHTER_NE_TO_SW;
                        }
                    }
                } else {
                    pEntry->m_IsEdge = 0;
                }
            }
        } // for (y = 0; y < m_MaxYPos; y++)
    } // for (x = 0; x < m_MaxXPos; x++)

abort:
    returnErr(err);
} // Initialize






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
CEdgeDetectionTable::GetPixelLuminance(CImageFile *pSrcImage, uint32 pixelValue) {
    uint32 red = 0;
    uint32 green = 0;
    uint32 blue = 0;
    float redFloat;
    float greenFloat;
    float blueFloat;
    float totalFloat;
    uint32 luminance = 0;

    if (pSrcImage) {
        pSrcImage->ParsePixel(pixelValue, &blue, &green, &red);
    }

    redFloat = (float) red;
    greenFloat = (float) green;
    blueFloat = (float) blue;

    redFloat = redFloat * (float) 0.30;
    greenFloat = greenFloat * (float) 0.59;
    blueFloat = blueFloat * (float) 0.11;

    totalFloat = redFloat + greenFloat + blueFloat;
    luminance = (uint32) totalFloat;

    return(luminance);
} // GetPixelLuminance



