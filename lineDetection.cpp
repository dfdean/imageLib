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
// Line Detection for Image Analyzer
// Detect all lines in an image with a (slightly modified) Hough algorithm.
//
//
// Basic Algorithm (Simplified)
// ============================
// 1. Create a set of counters, one for each possible line.
//      Initialize all counters to 0.
// 2. For every pixel in the image, increment all counters 
//    that correspond to lines that *could* contain that pixel.
//    Each pixel "votes" for all possible lines that may contain that pixel.
// 3. Pick the highest counters, these correspond to the lines 
//   with the most votes.
//
// Note, a line does not have to be continuous. If some pixels are missing,
// then it will not have a perfect score, but will have a high enough score to
// qualify as a line.
// 
//
// Improved Algorithm (Still Simplified)
// =====================================
//
// Consider a simple algorithm that works like this:
//
//          For each pixel (x,y)
//              Vote for each possible line that could intersect (x,y)
//          Examine all lines, and pick those with the most votes
// 
// This algorithm is very good at finding all possible lines, but it also has
// LOTS of false positives. For example, suppose you have 20 parallel vertical
// lines. There are LOTS of possible lines that are either horizontal or some
// slight angle and will intersect all 20 of the real lines, and so have 20 pixels.
// Now, you say, why not just raise the threshold? In that case a line does not
// count unless it has many more votes. That would reduce the number of false 
// positives, but it would also not find short real lines. 
//
// As an improvement, each pixel (x,y) only votes for only a few possible lines that
// are LIKELY candidates. The line intersects (x,y) and is determined by looking 
// at the pixels immediately around (x,y). So, each (x,y) makes a guess of the 
// line that intersects it, and votes for just that one. But what if 2 lines 
// intersect a point (x,y)? In that case, the point may make a wrong/useless vote. 
// It votes, but its vote may miss one or both lines and instead vote for some strange
// like, and no other points will vote in the same particular way. Most points on a
// line will vote for just the line that actually contains them, so that line will 
// still get most votes.
//
// If the gradient function were perfect, then we would use it to define exactly one
// perpendicular angle and be done. However, it is not precise enough. There may 
// be noise pixels around, which change the gradient. Moreover, all lines are 
// pixelated, so a local gradient is different than the line's true gradient. 
// So, instead consider all possible angles around the grandient angle and vote 
// for them all.
//
// 
// Line Geometry
// =============
//
// Choosing the right way to describe a line turns out to be a bit tricky.
// First, unlike some algorithms, I try to find line segments, not
// infinitely long lines. This means I track the start and end points
// of each line segment, which is different than a typical Hough implementation.
// So, if 2 line segments are pointing towards each other, but stop short 
// before they actually intersect, then I will be able to detect that. This 
// is an important details for some of the applications I will be using 
// this module for.
//
// Tracking the endpoints, however, has some other important advantages.
// It means that I have all the information to describe a line from just these
// endpoints; after all two points define a line. This means that I
// never have to convert from the representation of a line used in
// the Hough code to a more useful representation (like y=mx + b). This
// is important, because the Hough code may use some odd representations of
// lines that are specifically oriented to line detection.
//
// This also means that the representation of a line in the Hough code is
// really just a hash function. All I need is a function that will come up with
// the same result for any 2 points on the same line. Any function that has this
// property is good, it doesn't even have to be geometrically correct. Now,
// I use the common and standard Hough algorithm representation, which is geometrically
// correct, but I could expreiment with other hash functions.
//
//
// Describing a Line in the Hough Algorithm
// ========================================
//
// First, some definitions. A line can be described with a slope called m 
// and y-intercept called b:
//                              y=mx + b.
// The x-intercept is -b/m, which is derived by setting y=0 and solving for
// x in y=mx + b. Each point (x,y) in the X-Y plane may be contained by
// many different possible lines, each with a different slope and y-intercept.
// It votes for the line most likely to contain it, and we call this the
// "Containing-Line". 
//
// Each line has a single normal vector, which is a second line that is
// perpendicular to the containing-line. We call this line the "perpendicular-line".
// There is exactly ONE "perpendicular-line" for each containing-line, and
// all points (x,y) in the containing-line that coan describe the containing-line
// with y=mx+b will also be able to find the perpendicular-line.
//
// A Key Idea: every different "Containing-Line" will correspond to exactly one 
// unique "Perpendicular-line". Each different  "Perpendicular-line" is described
// with a unique combination of two values: (1) its length, called rho, (ii) and 
// its angle with the X-axis, called theta. So, each combination of rho and theta will 
// correspond to one unique "Perpendicular-line", which in turn corresponds to
// one unique "Containing-Line".
//
// We describe lines (y=mx+b) with the rho and theta of the perpendicular line.
// This seems awkward, but this indirect representation of lines is more robust and
// can describe infinite slopes. Now, the problem comes, for each point (x,y) on
// a line, how do we describe the perpendicular-line for that containing-line.
//
// A warning: We examine every point (x,y) in the containing-line, and use it to
// compute the perpendicular line. However, the perpendicular-line will NOT necessarily
// intersect the containing-line at (x,y). It intersects at some other point. 
// There is only ONE perpendicular-line for each containing-line, so EVERY point 
// along the containing-line will describe the same perpendicular line.
//
//
// Step 1: Compute the angle theta of the perpendicular line
// =========================================================
//
// The containing-line is a line, so a perpendicular at any point (x,y) will make the
// same angle with the X-axis. Imaging "sliding" a perpendicular-line up 
// and down along the containing-line. It is always perpendicular to the containing-line, 
// and all angles stay the same, including its angle with the x-axis, perpendicularLineAngle.
// So, we can compute the angle between the perpendicular-line and the X-axis from
// ANY OTHER point (x,y) along the containing line. In other words, each (x,y) can
// make an estimate of the angle between the perpendicular-line and the x-axis, and
// in theory every point (x,y) should derive the same angle, even though the perpendicular-line
// doesn't intersect (x,y).
//
// So, how do we compute the angle of the perpendicular-line? 
// We start by getting the gradient in the X-direction and Y-direction for 
// each pixel. These gradients measure how luminence changes around a point
// (x,y) and are an estimate of where are the edges of the line that 
// contains the point (x,y).
//
// The luminence, or brightness, around (x,y) changes in two directions: in the
// X-direction and in the Y-direction. The change between rows is computed
// by rowGradient and the change between columns is computed with colGradient.
// These two changes are really basis-vectors. They can be vector-added to define
// a line that shows the overall net change in luminence. This overall net change
// shows where the luminance changes most abruptly, so it is down the gradient
// from light to dark. This is the perpendicular-line, not the containing-line.
// It points perpendicular to the boundary of the containing-line. 
// Note, the containing-line has black pixels at every point, so it never changes 
// luminence, and the gradient is 0 along the containing-line. The perpendicular-line
// is the most abrupt change from light to dark, so it is perpendicular to the 
// containing-line.
//
// The rowGradient and colGradient are perpendicular to each other, so they 
// form a right triangle with the overall-net-change, which is the 
// perpendicular-line. The perpendicular-line is the hypotaneus of these right
// triangles. Imagine a rectangle, with the sides being rowGradient and the top
// and bottom being colGradient, and the diagonals are perpendicular-line.
// This means we can use some trigonometry on these right triangles. The angle of
// between the perpendicular-line and the X-axis is the same as the angle between 
// the perpendicular-line and the X-dimension component vector, which is colGradient.
// Let's call this perpendicularLineAngle. this means that:
//
//                  perpendicularLineAngle = atan(y, x)
//              or
//                  perpendicularLineAngle = atan(rowGradient, colGradient)
//
// Other implementations of Hough Transforms call this same angle theta, but I call
// it perpendicularLineAngleInRadians. Yes, it's a long name, but I want to be as explicit and
// easy to understand as possible.
//
// Now, we compute the gradients with Gradient operators.
// "Computer Vision" [1] has an example, but it doesn't define the operators. 
// That's useless. But their example seems to use the Prewitt gradient operators
// that are illustrated in [2]. 
//
//      rowGradient =   | -1   -1   -1 |
//                      |  0    0    0 |
//                      |  1    1    1 |
//
//      colGradient =   | 1     0   -1 |
//                      | 1     0   -1 |
//                      | 1     0   -1 |
//
// Be careful, these show the weights of pixel positions, they are not matrices.
// See the comment in the edgeDetector.cpp file for a description this syntax.
// Basically, it means that the gradients are:
//
//    rowGradient = (pixelBelow + pixelBelowLeft + pixelBelowRight) 
//                  - (pixelAbove + pixelAboveLeft + pixelAboveRight)
//
//    colGradient = (pixelLeft + pixelAboveLeft + pixelBelowLeft)
//                  - (pixelRight + pixelAboveRight + pixelBelowRight) 
//
// This is slightly tricky. The rowGradient is the difference between rows,
// so it is the change in the Y direction. The colGradient is the difference
// between columns, so it is the change in the X direction.
//
// Also, the colGradient is positive for a gradient that has numerical values
// that increase from right to left. But, 0 is black, and 0xFF is white, so 
// this is the same as having color that darkens from left to right. And, a
// black pixel is a line in the luminence map, so this is a line that points
// left to right.
//
// It seems that edge-detection and line-detection can be combined into a 
// single step for optimal performance. That's true, but I am implementing them
// as mostly separate steps for now. I compute all luminence values only once,
// to avoid redundant work. However, the code is in separate stages, which makes
// it slower. My application doesn't have to be real-time, and separating the 
// steps for now makes it more clear what is going on and allows me to experiement
// with different line detectors.
//
//
//
// Step 2: Compute the length rho of the perpendicular line
// ========================================================
//
// If you seriously want to understand this, you may want to draw the lines on a
// piece of paper as you read the comment. I didn't try ASCII-art.
//
// Start with a set of cartesian coordinates and an upward-sloping "Containing-Line" 
// that passes from (x=0, y=-b) to (x=N, y=0). We will focus on the lower-right quadrant,
// which is x>0 and y<0. But, note that (x,y) may be anywhere in the top-right
// quadrant, (x>0, y>0).
//
// The Y-intercept of the "Containing-Line" is called b, and the length of the 
// "Perpendicular-Line" is called rho. The X-intercept of the "Containing-Line" 
// is (-b/m) by solving y=mx + b with (y=0). 
//
// The "Containing-Line" makes a big triangle with the X-axis and Y-axis.
// The "Perpendicular-line" splits this into two smaller right triangles,
// since it is perpendicular to the "Containing-Line".
//
// (i) The first right triangle has sides of length b (the Y-intercept of the "Containing-Line") 
// and rho (the length of the "Perpendicular-line"). The perpendicular line with length
// rho is the hypoteneus.
//
// (ii) The second right triangle has sides of length -b/m (the X-intercept of the "Containing-Line")
// and r (the length of the "Perpendicular-line").
// Again, the perpendicular line with length rho is the hypoteneus.
// 
// From part I, we know that the perpendicular line has angle theta with the X-axis. 
// We can use some simple trigonometry on the second right triangle:
//
//                  -b/m * cos(theta) = rho
//                  or m = (-b/rho) * cos(theta) after rearranging
//
// Now, do some trig on the first right triangle.
// The perpendicular line forms angle theta with the X-axis, and
// 90-theta (in degrees) or pi/2 - theta (in radians) with the Y-axis. But, these
// are right triangles, so all 3 angles add up to 180, the 2 non-right angles add 
// up to 90. This means theta is also the angle between the containing-line and the
// Y-axis. This sets us up for some more trig.
//                  rho = b*sin(theta)
//                  or b = rho/sin(theta) after rearranging.
//
// Now, substitute the expressions for rho into the expression for m.
//                  m = (-b/rho) * cos(theta)             from above
//                  m = (-rho/sin(theta)*rho) * cos(theta)
//                  m = -cos(theta)/sin(theta)          cancelling out rho terms
//
// Now, we can plug these new expressions for b and m into the original y = mx + b.
// Be careful here. This is the (x,y) of any pixel on the containing-line. These
// are NOT the x,y where the perpendicular-line intersects the contianing-line.
// They are just used as a way of deriving b and m from any point on the contianing-line.
//                  y = mx + b
//                  y = (-cos(theta)/sin(theta))*x + rho/sin(theta)
//                  y + (cos(theta)/sin(theta))*x = rho/sin(theta)
//                  y*sin(theta) + cos(theta)*x = rho
//                  or rho = cos(theta)*x + y*sin(theta)   after rearranging.
//
// This uses ANY (x,y) in the "Containing-Line" and theta of the "Perpendicular-Line"
// to derive rho of the perpendicular line.
// 
// ****
// I don't fully understand this next point, but here is a guess.
// Now, in this case, theta is negative, since it is below the X axis. The 
// "Perpendicular-Line" intersects the "Containing-Line" in the (x>0, y<0)
// quadrant. But, cos(-theta) == cos(theta) and sin(-theta) = -sin(theta).
// So, the equation can be rewritten as:
//                  rho = cos(-theta)*x + y*sin(-theta)
//                  rho = cos(theta)*x - y*sin(theta)
// One textbook describes this as just changing the pixel coordinates, so (0,0)
// is the top left corner and the pixels "grow" by moving down the Y axis. I am
// not sure if this is important, but making this change will keep my math identical
// to some standard textbooks.
// ****
//
//
// Basic Algorithm (Implementation)
// ================================
// All angles are in radians.
// 
// Now, the "Containing-lines" are non-directional. In theory, this means 
// we only have to find one half-plane of all possible lines. The other 
// half-plane is just the same lines facing the opposite direction, or 
// shifted by 180 degrees. This means we only consider angles theta ranging 
// between 0 and 180 degrees, or 0 and Pi/2 radians. In practice, however, I
// had to find all lines at all angles to make this work.
// 
//
// References
// ==========
// [1] Linda Shapiro, George Stockman
// "Computer Vision" Prentice Hall, 2001
//
// [2] Lawrence O'Gorman, Michael J. Sammon, Michael Seul
// "Practical algorithms for image analysis: description, examples, programs"
// http://books.google.com/books?id=8dXkUPv2DGYC&pg=PA85&lpg=PA85&dq=%22o'gorman%22+row+gradient&source=bl&ots=P_ooi9zElI&sig=kx7MSE5WzTFq7aLpFlcDYzVU7JE&hl=en&ei=ENqkTYq8E6uC0QGUlsD8CA&sa=X&oi=book_result&ct=result&resnum=5&sqi=2&ved=0CC4Q6AEwBA#v=onepage&q=gradient&f=false
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
#include "approximateMath.h"
#include "perfMetrics.h"

FILE_DEBUGGING_GLOBALS(LOG_LEVEL_DEFAULT, 0);

static const double PI = 3.1415927;
static const double PIOver2 = 3.1415927 / 2;

#define DEBUG_LINE_DETECTOR 1

CStatsGroupAPI *g_LineDetectionPerf = NULL;
CPerfValueAPI *g_ReadBitmapTime = NULL;
CPerfValueAPI *g_MergeLinesTime = NULL;



//////////////////////////////////////////////////
class CPossibleLine {
public:
    int32           m_NumVotes;

    CBioCADPoint    m_PointA;
    CBioCADPoint    m_PointB;
    
    bool            m_fRecorded;
}; // CPossibleLine



//////////////////////////////////////////////////
class CLineDetectorState {
public:
    CImageFile          *m_pFullImage;
    CImageFile          *m_pEdgesImage;
    int32               m_MinXPos;
    int32               m_MaxXPos;
    int32               m_MinYPos;
    int32               m_MaxYPos;
    int32               m_Width;
    int32               m_Height;
    uint32              m_BlackPixel;

    // Properties of a useful line.
    // Includes metrics for deciding whether a line is unique and interesting.
    int32               m_MinVotesForRealLine;
    double              m_MinPixelDensityForRealLine;
    double              m_MinPointResolution;
    double              m_AngleResolutionInRadians;
    int32               m_MaxGapBetweenDashesInLine;
    int32               m_MinUsefulLineLength;

    // Voting statistics
    int32               m_NumPossibleLines;
    int32               m_NumLinesWithMinVotes;
    int32               m_NumDuplicateLines;
 
    // The lines we actually find
    CBioCADLine         *m_pLineList;
    int32               m_NumLines;

    // The range of possible values for theta and rho
    // These are for theta
    double              m_MinPerpendicularLineAngle;
    double              m_MaxPerpendicularLineAngle;
    double              m_PerpendicularLineAngleModulo;
    double              m_AngleIncrement;
    int32               m_NumPossibleAngleValues;
    // These are for rho
    double              m_MinPerpendicularLength;
    double              m_MaxPerpendicularLength;
    int32               m_NumPossibleLengthValues;
    // Used for all angles.
    double              m_AngleRangeAroundGradientInRadians;

    CPossibleLine       *m_pVoteArray;
    int32               m_NumEntriesInVoteArray;
}; // CLineDetectorState



static ErrVal RecordOneLine(
                    CPossibleLine *pPossibleLine,
                    CLineDetectorState *pDetectorState);

static bool OverlapNewLineWithExistingLines(
                    CPossibleLine *pPossibleLine,
                    double slope,
                    double yIntercept,
                    CLineDetectorState *pDetectorState);

static ErrVal DrawLines(CImageFile *pDestImage, CBioCADLine *pLineList);






/////////////////////////////////////////////////////////////////////////////
// Votes are stored in a 2D-array that is indexed by angle and theta.
/////////////////////////////////////////////////////////////////////////////
static inline CPossibleLine *
GetPossibleLine(CLineDetectorState *pDetectorState, double theta, double rho) {
    CPossibleLine *pResult = NULL;
    double thetaOffset;
    double rhoOffset;
    double numThetaIncrements;
    int32 intThetaOffset;
    int32 intRhoOffset;
    int32 index;

    // These are numbers that range from -n, -n+1, ...-1, 0, 1, .....n-1, n
    // Convert them into positive offsets.
    thetaOffset = theta - pDetectorState->m_MinPerpendicularLineAngle;
    rhoOffset = rho - pDetectorState->m_MinPerpendicularLength;

    // The angle has limited precision. So convert it into a multiple of precision units.
    numThetaIncrements = (thetaOffset / pDetectorState->m_AngleIncrement);

    intThetaOffset = RoundDoubleToInt(numThetaIncrements);
    intRhoOffset = RoundDoubleToInt(rhoOffset);

    index = (intRhoOffset * pDetectorState->m_NumPossibleAngleValues) + intThetaOffset;
    ASSERT(index < pDetectorState->m_NumEntriesInVoteArray);
    pResult = &((pDetectorState->m_pVoteArray)[index]);

    return(pResult);
} // GetPossibleLine









/////////////////////////////////////////////////////////////////////////////
//
// [DetectLines]
//
// Detect all lines in an image with a Hough algorithm.
// This is the main top-level procedure.
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
DetectLines(
        int32 options,
        CImageFile *pFullImage, 
        CEdgeDetectionTable *pLuminanceMap,
        CImageFile *pEdgesImage,
        int32 boundingBoxLeftX,
        int32 boundingBoxRightX,
        int32 boundingBoxTopY,
        int32 boundingBoxBottomY,
        CImageFile *pRebuiltLineImage,
        CBioCADLineSet *pLineList) {
    ErrVal err = ENoErr;
    double theta;
    double rho;
    double startTheta;
    double endTheta;
    int32 lineArraySize;
    CPossibleLine *pPossibleLine;
    int32 x;
    int32 y;
    uint32 pixelValue;
    CLineDetectorState detectorState;
    
    ProfilerDeclareGroup(g_LineDetectionPerf, "LineDetection");
    ProfilerDeclareTimer(g_LineDetectionPerf, "ReadBitmap", g_ReadBitmapTime);
    ProfilerDeclareTimer(g_LineDetectionPerf, "MergeLinesTime", g_MergeLinesTime);


    detectorState.m_pLineList = NULL;
    detectorState.m_pVoteArray = NULL;
    detectorState.m_NumEntriesInVoteArray = 0;
    
    if ((NULL == pFullImage) || (NULL == pEdgesImage)) {
        gotoErr(EFail);
    }
    
    // Statistics.
    detectorState.m_NumPossibleLines = 0;
    detectorState.m_NumLinesWithMinVotes = 0;
    detectorState.m_NumDuplicateLines = 0;    
    detectorState.m_NumLines = 0;
    detectorState.m_NumDuplicateLines = 0;
 
    // m_AngleRangeAroundGradientInRadians is the range of possible containing lines
    // when considering which possible lines can contain a point. We don't consider all
    // possible lines, only those lines that are sort of similar the the local gradiant
    // of that line.
    // Be careful here, this parameter has a lot of effect on the quality of the result.
    // A larger parameter means a bigger sweep of possible lines. That means more time
    // and space computing the possible lines. But, it also significantly improves
    // the number of lines we detect. On one test image, I found that (PIOver2 / 16)
    // found almost NO lines, while (PIOver2 / 8) was pretty good, and (PIOver2 / 4)
    // seemed to find almost all lines (and the missed lines were parallel to other found lines).
    // So, I am leaving this at (PIOver2 / 4).
    detectorState.m_AngleRangeAroundGradientInRadians = PIOver2 / 4;

    detectorState.m_MinPerpendicularLineAngle = -(PIOver2);
    detectorState.m_MaxPerpendicularLineAngle = PIOver2;
    detectorState.m_PerpendicularLineAngleModulo 
        = detectorState.m_MaxPerpendicularLineAngle - detectorState.m_MinPerpendicularLineAngle;    

    // m_AngleIncrement is the angle we increase the containing line when considering 
    // which possible lines can contain a point.
    // This is in radians. 1 degree == 0.017 radians, so 0.01 radians = 0.57 degrees.
    // So, consider all possible lines at 0.5 degree intervals, which is pretty good.
    // In practice, changing this to 0.001 radians, or 0.005 degrees raised the time and
    // space requirements by 10x, but only increased the lines by 2x, and most of those 
    // were not useful lines. So, this parameter does not seem to have to be extremely 
    // sensitive to get good results.
    detectorState.m_AngleIncrement = 0.01;

    detectorState.m_NumPossibleAngleValues = (uint32) (double) ((detectorState.m_MaxPerpendicularLineAngle 
                                                                    - detectorState.m_MinPerpendicularLineAngle)
                                                                / detectorState.m_AngleIncrement);

    // These values are just a wild guess. Basically, I am trying to ignore the tiny lines, 
    // or lines that intersect random unrelated points. This defines the sensitivity of
    // the algorithm.
    if (options & CELL_GEOMETRY_LINE_DETECTION_STYLE_SQUISHY_BLOBS) {
        detectorState.m_MinVotesForRealLine = 10; // <>Last-Working-Value = 50; Tried 10(great but slow), 20(great but slow)
        // At least 1 pixel for every N spaces.
        detectorState.m_MinPixelDensityForRealLine = 1 / 5; //<>Last-Working-Value = 1 / 5;
        detectorState.m_MinPointResolution = 10;
        detectorState.m_AngleResolutionInRadians = 0.4;
        detectorState.m_MaxGapBetweenDashesInLine = 10;
        detectorState.m_MinUsefulLineLength = 5;   //<> Last-Working-Value = 20
    } else {
        detectorState.m_MinVotesForRealLine = 90; // <>Last-Working-Value = 50; Tried 10(great but slow), 20(great but slow)
        // At least 1 pixel for every N spaces.
        detectorState.m_MinPixelDensityForRealLine = 1 / 5; //<>Last-Working-Value = 1 / 5;
        detectorState.m_MinPointResolution = 10;
        detectorState.m_AngleResolutionInRadians = 0.4;
        detectorState.m_MaxGapBetweenDashesInLine = 10;
        detectorState.m_MinUsefulLineLength = 50;   //<> Last-Working-Value = 20
    }
    

    // Record information about the image in the state.
    detectorState.m_pFullImage = pFullImage;
    detectorState.m_pEdgesImage = pEdgesImage;
    detectorState.m_BlackPixel = pEdgesImage->ConvertGrayScaleToPixel(CImageFile::GRAYSCALE_BLACK);

    detectorState.m_MinXPos = boundingBoxLeftX;
    if (detectorState.m_MinXPos < 0) {
        detectorState.m_MinXPos = 0;
    }
    detectorState.m_MinYPos = boundingBoxTopY;
    if (detectorState.m_MinYPos < 0) {
        detectorState.m_MinYPos = 0;
    }
    detectorState.m_MaxXPos = boundingBoxRightX;
    detectorState.m_MaxYPos = boundingBoxBottomY;
    if ((detectorState.m_MaxXPos < 0) || (detectorState.m_MaxYPos < 0)) {
        err = pEdgesImage->GetImageInfo(&(detectorState.m_MaxXPos), &(detectorState.m_MaxYPos));
        if (err) {
            gotoErr(err);
        }
    }
    detectorState.m_Width = detectorState.m_MaxXPos - detectorState.m_MinXPos;
    detectorState.m_Height = detectorState.m_MaxYPos - detectorState.m_MinYPos;
    

    // Compute the maximum size of a line. This is the length of the 
    // main diagonal across the rectangular image.
    detectorState.m_MaxPerpendicularLength
        = (uint32) (sqrt((double) (detectorState.m_Width * detectorState.m_Width) 
                         + (double) (detectorState.m_Height * detectorState.m_Height)));
    detectorState.m_MinPerpendicularLength = -(detectorState.m_MaxPerpendicularLength);
    detectorState.m_NumPossibleLengthValues = (int32) (2 * detectorState.m_MaxPerpendicularLength);


    detectorState.m_NumEntriesInVoteArray = (detectorState.m_NumPossibleLengthValues + 1) 
                                            * (detectorState.m_NumPossibleAngleValues + 1);
    lineArraySize = detectorState.m_NumEntriesInVoteArray * sizeof(CPossibleLine);
    // Make sure it is initalized to all 0's.
    detectorState.m_pVoteArray = (CPossibleLine *) memCalloc(lineArraySize);
    if (NULL == detectorState.m_pVoteArray) {
        gotoErr(EFail);
    }

    ProfilerStartTimer(g_ReadBitmapTime);

    // Examine every pixel in the image to find all lines.
    // NOTE: I index the voting arrays with a 0-based index, and that ranges x=0...Width, y=0...height.
    // But, the image itself is indexed with x=minX....maxX, and y=minY....maxY.
    for (x = detectorState.m_MinXPos; x < detectorState.m_MaxXPos; x++) {
        for (y = detectorState.m_MinYPos; y < detectorState.m_MaxYPos; y++) {
            bool fPixelMayBePartOfLine = false;

            err = pEdgesImage->GetPixel(x, y, &pixelValue);
            if (err)  {
                gotoErr(err);
            }
            fPixelMayBePartOfLine = (pixelValue == detectorState.m_BlackPixel);

            // If this is a black pixel, then use it to vote for every line that
            // can pass through this pixel.
            if (fPixelMayBePartOfLine) {
                double perpendicularLineLength;
                double perpendicularLineAngleInRadians;
                double cosAngle;
                double sinAngle;
                // Leave all these as signed. The grayscale values ate unsigned 0-255 
                // values, but we want to convert this into changes in luminance, 
                // which can be positive or negative.
                int32 pixelAbove;
                int32 pixelBelow;
                int32 pixelLeft;
                int32 pixelRight;
                int32 pixelAboveLeft;
                int32 pixelAboveRight;
                int32 pixelBelowLeft;
                int32 pixelBelowRight;
                int32 rowGradient;
                int32 colGradient;


                // Get the luminance of all surrounding pixels
                pixelAbove = pLuminanceMap->GetLuminance(x, y-1);
                pixelBelow = pLuminanceMap->GetLuminance(x, y+1); 
                pixelLeft = pLuminanceMap->GetLuminance(x-1, y);
                pixelRight = pLuminanceMap->GetLuminance(x+1, y);
                pixelAboveLeft = pLuminanceMap->GetLuminance(x-1, y-1);
                pixelAboveRight = pLuminanceMap->GetLuminance(x+1, y-1);
                pixelBelowLeft = pLuminanceMap->GetLuminance(x-1, y+1);
                pixelBelowRight = pLuminanceMap->GetLuminance(x+1, y+1);

                // Use the convolution matrices to get the change in the 
                // X and Y dimensions. These are basis vectors for the net change
                // in luminence at this point. The net change in luminence is
                // the same as the direction the dark-pixels are in. Those dark
                // pixels may be part of a line we are looking for.
                rowGradient = ((2*pixelBelow) + pixelBelowLeft + pixelBelowRight) 
                            - ((2*pixelAbove) + pixelAboveLeft + pixelAboveRight);

                colGradient = ((2*pixelLeft) + pixelAboveLeft + pixelBelowLeft)
                            - ((2*pixelRight) + pixelAboveRight + pixelBelowRight);


                // Now, use the lengths of the 2 basis vectors to get the angle of the line.
                // Remember, the rowGradient is the difference between rows,
                // so it is the change in the Y direction. The colGradient is the difference
                // between columns, so it is the change in the X direction.
                perpendicularLineAngleInRadians = atan2((double) rowGradient, (double) colGradient);

                // Lines are non-directional. This means 2 lines with angles theta and theta+Pi are 
                // the same. One is just the other rotated by pi radians (180 degrees) so it is just
                // reversed direction.
                if (perpendicularLineAngleInRadians < detectorState.m_MinPerpendicularLineAngle) {
                    perpendicularLineAngleInRadians = perpendicularLineAngleInRadians 
                                    + detectorState.m_PerpendicularLineAngleModulo;
                }

                if (perpendicularLineAngleInRadians >= detectorState.m_MaxPerpendicularLineAngle) {
                    perpendicularLineAngleInRadians = perpendicularLineAngleInRadians 
                                    - detectorState.m_PerpendicularLineAngleModulo;
                }

                // Round the angle. This is important because we want pixels that are on the
                // same pixelated line to derive the same abstract line. So, don't get too precise,
                // or very nearly colinear points will think they are totally different.
                perpendicularLineAngleInRadians = LimitDoubleToFixedPrecision(
                                                            perpendicularLineAngleInRadians, 
                                                            detectorState.m_AngleIncrement);


                // If the gradient function were perfect, then we would use it as the perpendicular
                // angle and be done. However, the image may not be perfect. There may be noise pixels
                // around, which change the gradient. Moreover, all lines are pixelated, so a local
                // gradient is different than the line's true gradient. So, instead consider all
                // possible angles around the grandient angle and vote for them all.
                startTheta = perpendicularLineAngleInRadians - detectorState.m_AngleRangeAroundGradientInRadians;
                if (startTheta < detectorState.m_MinPerpendicularLineAngle) {
                    startTheta = detectorState.m_MinPerpendicularLineAngle;
                }
                if (startTheta >= detectorState.m_MaxPerpendicularLineAngle) {
                    startTheta = detectorState.m_MaxPerpendicularLineAngle;
                }
                endTheta = perpendicularLineAngleInRadians + detectorState.m_AngleRangeAroundGradientInRadians;
                if (endTheta < detectorState.m_MinPerpendicularLineAngle) {
                    endTheta = detectorState.m_MinPerpendicularLineAngle;
                }
                if (endTheta >= detectorState.m_MaxPerpendicularLineAngle) {
                    endTheta = detectorState.m_MaxPerpendicularLineAngle;
                }
                // Sweep through all possible angles and vote for every line in that range.
                for (theta = startTheta; theta < endTheta; theta += detectorState.m_AngleIncrement) {
                    // Compute the length of the perpendicular.
                    cosAngle = cos(theta);
                    sinAngle = sin(theta);
                    perpendicularLineLength = (((double) x) * cosAngle) - (((double) y) * sinAngle);
                    if (perpendicularLineLength < detectorState.m_MinPerpendicularLength) {
                        perpendicularLineLength = detectorState.m_MinPerpendicularLength;
                    }
                    if (perpendicularLineLength > detectorState.m_MaxPerpendicularLength) {
                        perpendicularLineLength = detectorState.m_MaxPerpendicularLength;
                    }

                    pPossibleLine = GetPossibleLine(
                                        &detectorState,
                                        theta, 
                                        perpendicularLineLength);

                    // Record the endpoints for each line. Remember, this uses the real (x,y) which
                    // ranges from x=minX....maxX, and y=minY....maxY.
                    if (0 == pPossibleLine->m_NumVotes) {
                        pPossibleLine->m_PointA.m_X = x;
                        pPossibleLine->m_PointA.m_Y = y;
                        pPossibleLine->m_PointA.m_Z = 0;

                        pPossibleLine->m_PointB.m_X = x;
                        pPossibleLine->m_PointB.m_Y = y;
                        pPossibleLine->m_PointB.m_Z = 0;
                    } else {
                        if ((x < pPossibleLine->m_PointA.m_X)
                            || ((x == pPossibleLine->m_PointA.m_X) && (y < pPossibleLine->m_PointA.m_Y))) {
                            pPossibleLine->m_PointA.m_X = x;
                            pPossibleLine->m_PointA.m_Y = y;
                            pPossibleLine->m_PointA.m_Z = 0;
                        }
                        if ((x > pPossibleLine->m_PointB.m_X)
                            || ((x == pPossibleLine->m_PointB.m_X) && (y > pPossibleLine->m_PointB.m_Y))) {
                            pPossibleLine->m_PointB.m_X = x;
                            pPossibleLine->m_PointB.m_Y = y;
                            pPossibleLine->m_PointB.m_Z = 0;
                        }
                    }
                    pPossibleLine->m_NumVotes += 1;
                } // for (rho = startTheta; rho < endTheta; rho += detectorState.m_AngleIncrement)
            } // if (pixelValue == detectorState.m_BlackPixel)
        } // for (y = 0; y < detectorState.m_MaxYPos; y++)
    } // for (x = 0; x < detectorState.m_MaxXPos; x++)

    ProfilerStopTimer(g_ReadBitmapTime);
    ProfilerStartTimer(g_MergeLinesTime);

    // Put all possible lines with a minimum number of votes on a linked list.
    detectorState.m_NumPossibleLines = 0;
    detectorState.m_NumLinesWithMinVotes = 0;
    for (theta = detectorState.m_MinPerpendicularLineAngle; 
        theta < detectorState.m_MaxPerpendicularLineAngle; 
        theta += detectorState.m_AngleIncrement) {
        for (rho = detectorState.m_MinPerpendicularLength; 
            rho < detectorState.m_MaxPerpendicularLength; 
            rho++) {
            detectorState.m_NumPossibleLines += 1;
            pPossibleLine = GetPossibleLine(&detectorState, theta, rho);
            // Be careful here. theta and rho may round and so the
            // same entry can be returned for different values. This can
            // cause unnecessary duplicated lines.
            if ((pPossibleLine->m_NumVotes >= detectorState.m_MinVotesForRealLine)
                && !(pPossibleLine->m_fRecorded)) {
                pPossibleLine->m_fRecorded = true;
                detectorState.m_NumLinesWithMinVotes += 1;

                err = RecordOneLine(pPossibleLine, &detectorState);
                if (err) {
                    gotoErr(err);
                }
            } // if (pPossibleLine->m_NumVotes >= detectorState.m_MinVotesForRealLine)
        } // for (rho = 0; rho < detectorState.m_MaxPerpendicularLength; rho++)
    } // for (theta = 0; theta < detectorState.m_MaxPerpendicularLineAngle; theta += detectorState.m_AngleIncrement)


    ProfilerStopTimer(g_MergeLinesTime);

    // The vote array is huge, so delete it before we do anything else.
    memFree(detectorState.m_pVoteArray);
    detectorState.m_pVoteArray = NULL;
    // Ahhh, see? Main memory feels much better now.


#if DEBUG_LINE_DETECTOR     
    printf("\nLine Detection:\n");
    printf("NumPossibleLines = %d\n", detectorState.m_NumPossibleLines);
    printf("NumLinesWithMinVotes = %d\n", detectorState.m_NumLinesWithMinVotes);
    printf("NumDuplicateLines = %d\n", detectorState.m_NumDuplicateLines);
    printf("NumLines = %d\n", detectorState.m_NumLines);
    printf("\n\n");
#endif // DEBUG_LINE_DETECTOR


    // pLineList is optional.
    if (pLineList) {
        ((CBioCADLineSet *) pLineList)->SetLineList(detectorState.m_pLineList);
        detectorState.m_pLineList = NULL;

        pLineList->FilterLines(CBioCADLineSet::FILTER_BY_MIN_LENGTH, detectorState.m_MinUsefulLineLength);
        // <> Don't do this yet. I don't yet combine the pixel lists when I combine 2 lines 
        // in OverlapNewLineWithExistingLines.
        //<>pLineList->FilterLines(CBioCADLineSet::FILTER_BY_MIN_PIXEL_DENSITY, detectorState.m_MinPixelDensityForRealLine);
    }

    // Printing the image to an output file is also optional. It is mainly used for debugging.
    if (pRebuiltLineImage) {
        CBioCADLine *pRawLines;

        pRawLines = ((CBioCADLineSet *) pLineList)->GetLineList();

        err = pRebuiltLineImage->InitializeFromSource(pEdgesImage, 0xFFFFFFFF);
        if (err) {
            gotoErr(err);
        }

        err = DrawLines(pRebuiltLineImage, pRawLines);
        if (err) {
            gotoErr(err);
        }
    } // if (pRebuiltLineImage)


abort:
    memFree(detectorState.m_pVoteArray);
    detectorState.m_pVoteArray = NULL;

    returnErr(err);
} // DetectLines








/////////////////////////////////////////////////////////////////////////////
//
// [RecordOneLine]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
RecordOneLine(CPossibleLine *pPossibleLine, CLineDetectorState *pDetectorState) {
    ErrVal err = ENoErr;
    CBioCADLine *pLine;
    CBioCADPoint *pPixel;
    int32 deltaY = 0;
    int32 deltaX = 0;
    bool fUsefulLine = false;
    double slope;
    double yIntercept;
    int32 x;
    int32 y;
    double theoreticalY;
    uint32 pixelValue;
    double density;
    bool fPixelMayBePartOfLine = false;


    if ((NULL == pPossibleLine) || (NULL == pDetectorState))
    {
        gotoErr(EFail);
    }
    fUsefulLine = true;

    deltaX = pPossibleLine->m_PointB.m_X - pPossibleLine->m_PointA.m_X;
    deltaY = pPossibleLine->m_PointB.m_Y - pPossibleLine->m_PointA.m_Y;

    // Treat vertical lines as "almost" vertical so the slope can be calculated
    if (0 == deltaX)
    {
        deltaX = 1;
    }

    slope = ((double) deltaY) / ((double) deltaX);
    // y = mx + b so, after some algebra  b = y - mx
    yIntercept = ((double) pPossibleLine->m_PointA.m_Y) - (slope * ((double) pPossibleLine->m_PointA.m_X));


    fUsefulLine = OverlapNewLineWithExistingLines(pPossibleLine, slope, yIntercept, pDetectorState);
    if (!fUsefulLine)
    {
        pDetectorState->m_NumDuplicateLines += 1;
        goto abort;
    }

    pLine = newex CBioCADLine;
    if (NULL == pLine)
    {
        gotoErr(EFail);
    }

    pLine->m_PointA = pPossibleLine->m_PointA;
    pLine->m_PointB = pPossibleLine->m_PointB;
    pLine->m_Slope = slope;
    pLine->m_YIntercept = yIntercept;

    // Each line forms an angle with this horizontal line, and those angles
    // are a function of the slopes of each line. Use atan(x,y) to compute
    // the angle between each line and the horizontal line.
    //   slope = deltaY / deltaX. 
    //   If deltaX = 1, then deltaY = slope.
    pLine->m_AngleWithHorizontal = atan2((double) 1.0, (double) slope);        


    // Possibly extend the line.
    for (x = pLine->m_PointA.m_X; x <= pLine->m_PointB.m_X; x++)
    {
        theoreticalY = (((double) x) * pLine->m_Slope) + yIntercept;
            
        // First truncate the floating point number. This finds the
        // pixel with y rounded down, which is just below the point.
        y = (int32) (theoreticalY);        

        fPixelMayBePartOfLine = false;
        err = pDetectorState->m_pEdgesImage->GetPixel(x, y, &pixelValue);
        if ((!err) && (pixelValue == pDetectorState->m_BlackPixel)) {
            fPixelMayBePartOfLine = true;
        }
        if (fPixelMayBePartOfLine)
        {
            pPixel = newex CBioCADPoint;
            if (NULL == pPixel)
            {
                gotoErr(EFail);
            }
            pPixel->m_X = x;
            pPixel->m_Y = y;

            pPixel->m_pNextPoint = pLine->m_PixelList;
            pLine->m_PixelList = pPixel;

            //<><>
            pDetectorState->m_pFullImage->SetPixel(x, y, 0x0000FF);
            //<><>
        }

        // Now add to the truncated number. This finds the pixel just above the point
        y += 1;
        fPixelMayBePartOfLine = false;
        err = pDetectorState->m_pEdgesImage->GetPixel(x, y, &pixelValue);
        if ((!err) && (pixelValue == pDetectorState->m_BlackPixel)) {
            fPixelMayBePartOfLine = true;
        }
        if (fPixelMayBePartOfLine)
        {
            pPixel = newex CBioCADPoint;
            if (NULL == pPixel)
            {
                gotoErr(EFail);
            }
            pPixel->m_X = x;
            pPixel->m_Y = y;

            pPixel->m_pNextPoint = pLine->m_PixelList;
            pLine->m_PixelList = pPixel;

            //<><>
            pDetectorState->m_pFullImage->SetPixel(x, y, 0x0000FF);
            //<><>
        }
        


        // Look at the pixel just below.
        y = y - 1;
        fPixelMayBePartOfLine = false;
        err = pDetectorState->m_pEdgesImage->GetPixel(x, y, &pixelValue);
        if ((!err) && (pixelValue == pDetectorState->m_BlackPixel)) {
            fPixelMayBePartOfLine = true;
        }
        if (fPixelMayBePartOfLine)
        {
            pPixel = newex CBioCADPoint;
            if (NULL == pPixel)
            {
                gotoErr(EFail);
            }
            pPixel->m_X = x;
            pPixel->m_Y = y;

            pPixel->m_pNextPoint = pLine->m_PixelList;
            pLine->m_PixelList = pPixel;

            //<><>
            pDetectorState->m_pFullImage->SetPixel(x, y, 0x0000FF);
            //<><>
        }
    } // for (x = pLine->m_PointA.m_X; x < pLine->m_PointB.m_X; x++)

    // Check the pixel density
    density = pLine->m_NumPixels / pLine->GetLength();
    if (density < pDetectorState->m_MinPixelDensityForRealLine)
    {
        delete pLine;
        goto abort;
    }

    // Add it to the list of useful lines.
    pLine->m_pNextLine = pDetectorState->m_pLineList;
    pDetectorState->m_pLineList = pLine;
    pDetectorState->m_NumLines += 1;

abort:
    returnErr(err);
} // RecordOneLine







/////////////////////////////////////////////////////////////////////////////
//
// [OverlapNewLineWithExistingLines]
//
/////////////////////////////////////////////////////////////////////////////
bool
OverlapNewLineWithExistingLines(
                    CPossibleLine *pPossibleLine,
                    double slope,
                    double yIntercept,
                    CLineDetectorState *pDetectorState)
{
    CBioCADLine *pExistingLine = NULL;
    int32 deltaY = 0;
    int32 deltaX = 0;
    double startPointDifference;
    //double endPointDifference;
    bool fOverlappingLines = false;
    bool fUsefulLine = false;


    fUsefulLine = true;


    // Look if this line overlaps an alternate line.
    pExistingLine = pDetectorState->m_pLineList;
    while (pExistingLine)
    {        
        fOverlappingLines = false;

        // Check if the lines are roughly the same slope.
        if (DoubleValuesAreClose(
                        slope, 
                        pExistingLine->m_Slope, 
                        pDetectorState->m_AngleResolutionInRadians))
        {
            // Check if they are roughly the same intercept. If so, then they may overlap.
            if (DoubleValuesAreClose(
                        yIntercept, 
                        pExistingLine->m_YIntercept, 
                        pDetectorState->m_MinPointResolution))
            {
                // If the endpoints really do overlap, then the lines overlap.
                if ((pExistingLine->m_PointA.m_X >= pPossibleLine->m_PointA.m_X) 
                    && (pExistingLine->m_PointA.m_X <= pPossibleLine->m_PointB.m_X))
                {
                    fOverlappingLines = true;
                }
                else if ((pExistingLine->m_PointB.m_X >= pPossibleLine->m_PointA.m_X) 
                    && (pExistingLine->m_PointB.m_X <= pPossibleLine->m_PointB.m_X))
                {
                    fOverlappingLines = true;
                }

                // If the two lines are actually just two adjacent dashes in a larger 
                // dashed line, then combine them. This may be an artifact in the original image
                // that breaks up a line.
                if ((IntValuesAreClose(
                            pExistingLine->m_PointA.m_X,
                            pPossibleLine->m_PointB.m_X, 
                            pDetectorState->m_MaxGapBetweenDashesInLine))
                    || (IntValuesAreClose(
                            pExistingLine->m_PointB.m_X, 
                            pPossibleLine->m_PointA.m_X, 
                            pDetectorState->m_MaxGapBetweenDashesInLine)))
                {
                    fOverlappingLines = true;
                }

                if (!fOverlappingLines) {
                    startPointDifference = GetDistanceBetweenPoints(
                                                    &(pExistingLine->m_PointA), 
                                                    &(pPossibleLine->m_PointA));
                    //endPointDifference = GetDistanceBetweenPoints(&(pExistingLine->m_PointB), &(pPossibleLine->m_PointB));                    
                    if ((startPointDifference <= pDetectorState->m_MinPointResolution)
                        && (startPointDifference <= pDetectorState->m_MinPointResolution)) {
                        fOverlappingLines = true;
                    }
                }


                if (fOverlappingLines) {

                    fUsefulLine = false;
                    // Extend the endpoints of the first line to include the second line.
                    if (pPossibleLine->m_PointA.m_X < pExistingLine->m_PointA.m_X) {
                        pExistingLine->m_PointA = pPossibleLine->m_PointA;
                    }
                    if (pPossibleLine->m_PointB.m_X > pExistingLine->m_PointB.m_X) {
                        pExistingLine->m_PointB = pPossibleLine->m_PointB;
                    }

                    // Now we have extended the line, we may want to update its slope and intercept
                    deltaX = pExistingLine->m_PointB.m_X - pExistingLine->m_PointA.m_X;
                    if (0 == deltaX) {
                        deltaX = 1;
                    }
                    deltaY = pExistingLine->m_PointB.m_Y - pExistingLine->m_PointA.m_Y;
                    pExistingLine->m_Slope = ((double) deltaY) / ((double) deltaX);
                    // y = mx + b so, after some algebra  b = y - mx
                    pExistingLine->m_YIntercept = ((double) pExistingLine->m_PointA.m_Y) - (slope * ((double) pExistingLine->m_PointA.m_X));
                }
            } // Same Y-Intercept
        } // Same slope

        pExistingLine = pExistingLine->m_pNextLine;
    } // while (pExistingLine)
    

    return(fUsefulLine);
} // OverlapNewLineWithExistingLines







/////////////////////////////////////////////////////////////////////////////
//
// [DrawLines]
//
/////////////////////////////////////////////////////////////////////////////
ErrVal
DrawLines(CImageFile *pDestImage, CBioCADLine *pLine) {
    ErrVal err = ENoErr;

    if (NULL == pDestImage) {
        gotoErr(EFail);
    }

    while (pLine) {
        err = pLine->DrawLineToImage(pDestImage, CImageFile::GRAYSCALE_BLACK, 0);
        if (err) {
            gotoErr(err);
        }

        pLine = pLine->m_pNextLine;
    } // while (pLine)

abort:
    returnErr(err);
} // DrawLines

