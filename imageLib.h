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

#ifndef _IMAGE_LIB_H_
#define _IMAGE_LIB_H_

class C2DImage;
class CStatsFile;
class CBioCADCrossSection;



////////////////////////////////////////////////////////////////////////////////
// Colors
// This uses the RGBQuad data structure, which is a 4-byte value which
// contains one byte for each Red, Green, and Blue respectively.
//
// This is NON-STANDARD, however, because my constants arrange the
// RGB values into the order they appear in memory in a BMP pixel array,
// not the conventional ordering. This saves byte-swap operations on
// an Intel processor, but makes the constants a bit more tricky.
//
// WinGDI.h defines some macros GetRValue, GetGValue and GetBValue.
// These define the byte order as:
//    Red is bits 0-7  so just mask off higher 24 bits
//    Green is bits 8-15  so shift right 16 bits and then mask off higher bits
//    Blue is bits 16-23  so shift right 8 bits and then mask off higher bits
// WinGDI.h also defines a macro PALETTERGB(r,g,b) which uses the same ordering.
// In other words, a 4-byte value (in Hex notation) would be 0x00BBGGRR
//
// Note, however, that BMP files often use 24-bits per pixel, and they store
// these as BB GG RR in increasing order in memory. 
// See http://en.wikipedia.org/wiki/BMP_file_format
// Be Careful! - That Wiki page is a bit confusing. The memory does NOT use
// the RGBX format that is illustrated in a diagram. Instead, see the
// section "Pixel array (bitmap data)" which says:
//      "A 24-bit bitmap with Width=1, would have 3 bytes of data per row (blue, green, red)"
// I have confirmed this with experiments that parsed a pre-built image that
// has a color table, so it uses the byte layout that BMP considers to be those actual colors.
// This format is
//    Blue is the byte 0, bits 0-7
//    Green is the byte 1, bits 8-15
//    Red is the byte 2, bits 16-23
//
// So, I define my own colors, which will use the byte ordering used in a BMP
// This optimizes for BMP files. Additionally, Intel chips are little-endian, 
// so a 4-byte (b3, b2, b1, b0) value is stored in memory as: b0 b1 b2 b3 where 
// b0 has the lowest memory address.
//
// As a result, I define constants as 0x00RRGGBB
// BB is blue, the least significant byte, and the first byte in the sequence of a BMP pixel.
// GG is green, bits 8-15, and the second byte in the sequence of a BMP pixel.
// RR is red, bits 16-23, and the third byte in the sequence of a BMP pixel.
#define WHITE_PIXEL             0xFFFFFF
#define BLACK_PIXEL             0x000000
#define BLUE_PIXEL              0x0000FF
#define GREEN_PIXEL             0x00FF00
#define RED_PIXEL               0xFF0000
#define BLUEGREEN_PIXEL         0x00FFFF
#define YELLOW_PIXEL            0xFFFF00
#define PURPLE_PIXEL            0xFF00FF

#define ORANGE_PIXEL            0x0077FF
#define CAMAUGREEN_PIXEL        0xFFFF00
#define COLOR1_PIXEL            0x770000
#define COLOR2_PIXEL            0x007700
#define COLOR3_PIXEL            0x000077
#define LIST_END_PIXEL          0x123456
#define LIGHT_GRAY_PIXEL        0xDDDDDD




////////////////////////////////////////////////////////////////////////////////
// 2D Files
//
// This abstraction conceals the specific format of different
// image files. This one interface should be supported by parsers for
// file types like BMP and JPG files.
////////////////////////////////////////////////////////////////////////////////
class CImageFile {
public:
    enum {
        // 0x00 is no colors, so it's black.
        // 0xff is all colors, so it's white.
        GRAYSCALE_WHITE     = 0xFF,
        GRAYSCALE_BLACK     = 0x00,

        BIOCAD_FILE_CLOSE_AFTER_SAVE    = 0x01,
        BIOCAD_FILE_MAY_CREATE_FILE     = 0x02,
    };

    virtual ErrVal ReadImageFile(const char *pFilePath) = 0;
    virtual ErrVal InitializeFromSource(
                            CImageFile *pSrcImageFile,
                            uint32 value) = 0;
    virtual ErrVal InitializeFromBitMap(
                            char *pSrcBitMap, 
                            const char *pBitmapFormat, 
                            int32 widthInPixels, 
                            int32 heightInPixels, 
                            int32 bitsPerPixel) = 0;
    virtual void Close() = 0;
    virtual void CloseOnDiskOnly() = 0;
 
    virtual ErrVal Save(int32 options) = 0;
    virtual ErrVal SaveAs(const char *pNewPathName, int32 options) = 0;

    virtual ErrVal GetImageInfo(int32 *pMaxXPos, int32 *pMaxYPos) = 0;
    virtual ErrVal GetBitMap(char **ppBitMap, int32 *pBitmapLength) = 0;

    virtual ErrVal GetPixel(int32 xPos, int32 yPos, uint32 *pResult) = 0;
    virtual ErrVal SetPixel(int32 xPos, int32 yPos, uint32 value) = 0;

    virtual void ParsePixel(uint32 value, uint32 *pBlue, uint32 *pGreen, uint32 *pRed) = 0;
    virtual uint32 ConvertGrayScaleToPixel(uint32 grayScaleValue) = 0;

    virtual bool RowOperationsAreFast() = 0;
    virtual ErrVal CopyPixelRow(int32 srcX, int32 srcY, int32 destX, int32 destY, int32 numPixels) = 0;
    virtual ErrVal CropImage(int32 newWidth, int32 newHeight) = 0;
}; // CImageFile

CImageFile *OpenBMPFile(const char *pFilePath);
CImageFile *OpenBitmapImage(
                char *pSrcBitMap, 
                const char *pBitmapFormat, 
                int32 widthInPixels, 
                int32 heightInPixels, 
                int32 bitsPerPixel);
CImageFile *MakeNewBMPImage(const char *pNewFilePath);
void DeleteImageObject(CImageFile *pParserInterface);





////////////////////////////////////////////////////////////////////////////////
//
//                           GEOMETRY OBJECTS
//
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////
// This is a point in a 2D or 3D space. 
class CBioCADPoint {
public:    
    CBioCADPoint();
    NEWEX_IMPL();

    int32           m_X;
    int32           m_Y;
    int32           m_Z;

    //double          m_TangentSlope;
    //double          m_OrthogonalSlope;

    CBioCADPoint    *m_pNextPoint;
}; // CBioCADPoint

double GetDistanceBetweenPoints(CBioCADPoint *pPointA, CBioCADPoint *pPointB);



////////////////////////////////////////////////
// This is a line in 2D or 3D space
class CBioCADLine {
public:
    enum {
        LINE_TEMP_PRUNED    = 0x08,
    };

    CBioCADLine();
    NEWEX_IMPL();

    double GetLength();
    ErrVal DrawLineToImage(CImageFile *pDestImage, int32 color, int32 options);

    int32               m_LineFlags;

    // You can define a line as either two endpoints or else
    // as a slope and Y-intercept. This data structure allows
    // both, since both are used in different parts of the code.
    // For example, the line detection code uses slope and Y-intercept
    // while some shape detection code uses two endpoints.
    //////////////////
    CBioCADPoint        m_PointA;
    CBioCADPoint        m_PointB;
    //////////////////
    double              m_Slope;
    double              m_YIntercept;
    double              m_AngleWithHorizontal;
    //////////////////
    // This is the actual pixels contained in a line.
    // This is only used by the line detection code.
    int32               m_NumPixels;
    CBioCADPoint        *m_PixelList;
    
    double              m_Length;

    CBioCADLine         *m_pNextLine;
}; // CBioCADLine





////////////////////////////////////////////////
class CBioCADLineSet {
public:
    NEWEX_IMPL();

    enum {
        FILTER_BY_MIN_LENGTH            = 1,
        FILTER_BY_MIN_PIXEL_DENSITY     = 2,
    };

    CBioCADLineSet();
    virtual ~CBioCADLineSet();
    void DiscardLines();

    virtual ErrVal SetLineList(CBioCADLine *pLine);
    virtual CBioCADLine *GetLineList();
    virtual void FilterLines(int32 criteria, int32 value);
    
    int32           m_NumLines;
    int32           m_MaxLines;
    CBioCADLine     **m_pLineList;
    bool            m_fAllocatedLines;

    CBioCADLine     *m_pRemovedLines;

    CBioCADLineSet  *m_pNextGraph;
}; // CBioCADLineSet







////////////////////////////////////////////////
class CBioCADShape {
public:    
    CBioCADShape();
    ~CBioCADShape();
    NEWEX_IMPL()
    
    CBioCADPoint *AddPoint(int32 x, int32 y, int32 z);
    void FindBoundingBox();
    
    ErrVal DrawShape(int32 color, int32 options);
    void DrawBoundingBox(int32 color);

    ErrVal GetPixelStats(
                    uint32 *pTotalLuminence, 
                    uint32 *pAverageLuminence,
                    uint32 *pMinLuminence,
                    uint32 *pMaxLuminence,
                    uint32 *pNumPixelsChecked);

    ErrVal CountPixelsInLuminenceRange(
                    uint32 minLuminence,
                    uint32 maxLuminence,
                    uint32 *pNumPixels,
                    float *pFractionOfRegion,
                    uint32 *pNumPixelsChecked);

    int32 GetAreaInPixels();

    float ComputeOverlap(
                    int32 topOffset, 
                    int32 bottomOffset, 
                    int32 leftOffset, 
                    int32 rightOffset); 
    
    enum {
        // Feature Type
        FEATURE_TYPE_REGION         = 1,
        FEATURE_TYPE_RECTANGLE      = 2,

        // m_ShapeFlags
        SHAPE_FLAG_DELETE           = 0x0001,
        SOFTWARE_DISCOVERED         = 0x0002,
    };
    CImageFile          *m_pSourceFile;
    int32               m_FeatureType;
    int32               m_FeatureID;
    int32               m_ShapeFlags;

    int32               m_BoundingBoxLeftX;
    int32               m_BoundingBoxRightX;
    int32               m_BoundingBoxTopY;
    int32               m_BoundingBoxBottomY;

    // These are the boundary points.
    CBioCADPoint        *m_pPointList;
    int32               m_NumPoints;

    // A Cross Section is one line across a shape, along a horizontal dimension 
    // It may not correspond to the orientation of the shape; for example a 
    // shape may be tilted at 45 degrees then the cross sections will slice the 
    // shape at an angle. They are useful, however, for hit-testing: to see of 
    // a point is inside a shape.
    CBioCADCrossSection *m_pCrossSectionList;
    int32               m_NumCrossSections;

    C2DImage            *m_pOwnerImage;
    CBioCADShape        *m_pNextShape;
}; // CBioCADShape







////////////////////////////////////////////////////////////////////////////////
//
//                           2-DIMENSIONAL IMAGES
//
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////
// This is a 2D image that may contain one or more shapes.
// Each C2DImage is a runtime objest that is the processed form
// of a simple image file.
class C2DImage {
public:
    virtual ErrVal Save() = 0;
    virtual ErrVal SaveAs(const char *pNewFilePathName) = 0;
    virtual void Close() = 0;
    virtual void CloseOnDiskOnly() = 0;

    virtual void GetDimensions(int32 *pWidth, int32 *pHeight) = 0;
    virtual void GetBitMap(char **ppBitMap, int32 *pBitmapLength) = 0;
    virtual ErrVal GetFeatureProperty(
                        int32 featureID,
                        int32 propertyID,
                        int32 *pResult) = 0;

    virtual ErrVal AddFeature(
                    int32 featureType,
                    int32 pointAX, 
                    int32 pointAY, 
                    int32 pointBX, 
                    int32 pointBY, 
                    int32 options,
                    int32 color,
                    int32 *pFeatureIDResult) = 0;

    virtual void DrawFeatures(int32 options) = 0;

    virtual ErrVal CopyRect(
                    int32 srcTopLeftX, 
                    int32 srcTopLeftY, 
                    int32 srcWidth, 
                    int32 srcHeight, 
                    int32 destTopLextX, 
                    int32 destTopLeftY) = 0;

    virtual ErrVal CropImage(
                    int32 newWidth, 
                    int32 newHeight) = 0;

    virtual ErrVal CreateInspectRegion(
                    int32 positionType,
                    int32 topOffset, 
                    int32 bottomOffset, 
                    int32 leftOffset, 
                    int32 rightOffset, 
                    CBioCADShape **ppResult) = 0;

    int32           m_ZPlane;
    C2DImage        *m_pNextImage;
}; // C2DImage


ErrVal Open2DImageFromFile(
                const char *pImageFileName,
                int32 options, 
                CStatsFile *pStatFile,
                C2DImage **ppResult);

ErrVal Open2DImageFromBitMap(
                char *pSrcBitMap, 
                const char *pBitmapFormat, 
                int32 widthInPixels, 
                int32 heightInPixels, 
                int32 bitsPerPixel,
                int32 options, 
                CStatsFile *pStatFile,
                C2DImage **ppResult);

void DeleteImageObject(C2DImage *pGenericImage);

// Options for shape and line detection
enum {
    CELL_GEOMETRY_SAVE_EDGE_DETECTION_TO_FILE           = 0x0001,
    CELL_GEOMETRY_SAVE_LINELIST_TO_FILE                 = 0x0002,
    CELL_GEOMETRY_DRAW_SHAPES_IN_COLOR                  = 0x0004,
    CELL_GEOMETRY_LINE_DETECTION_STYLE_SQUISHY_BLOBS    = 0x0008,
    CELL_GEOMETRY_DRAW_SHAPE_INTERIORS                  = 0x0010,
    CELL_GEOMETRY_DRAW_DIAMETERS                        = 0x0020,
    CELL_GEOMETRY_DRAW_DIAMETER_MIDPOINTS               = 0x0040,
    CELL_GEOMETRY_DRAW_INTERIOR_AS_GRAY                 = 0x0080,
    CELL_GEOMETRY_REDRAW_WITH_JUST_SHAPE_OUTLINES       = 0x0100,
    CELL_GEOMETRY_DRAW_SHAPE_SCANLINES                  = 0x0200,
};



// Options for creating inspection regions
enum {
    INSPECTION_REGION_RELATIVE_T0_IMAGE_MIDDLE  = 1,
    INSPECTION_REGION_RELATIVE_T0_IMAGE_EDGES   = 2,
    INSPECTION_REGION_ABSOLUTE_COORDS           = 3,
    INSPECTION_REGION_FROM_EDGE_DETECTION       = 4,
};




#endif // _IMAGE_LIB_H_



