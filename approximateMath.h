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

#ifndef _APPROXIMATE_MATH_H_
#define _APPROXIMATE_MATH_H_




/////////////////////////////////////////////////////////////////////////////
//
// [IntValuesAreClose]
//
/////////////////////////////////////////////////////////////////////////////
static bool 
IntValuesAreClose(int32 value1, int32 value2, int32 resolution) {
    int32 difference;

    if (value1 < 0)  {
        value1 = -value1;
    }
    if (value2 < 0) {
        value2 = -value2;
    }

    // Compare the min Y point.
    difference = value1 - value2;
    if (difference < 0) {
        difference = -difference;
    }
    if (difference <= resolution) {
        return(true);
    }

    return(false);
} // IntValuesAreClose





/////////////////////////////////////////////////////////////////////////////
//
// [DoubleValuesAreClose]
//
/////////////////////////////////////////////////////////////////////////////
static bool 
DoubleValuesAreClose(double value1, double value2, double resolution) {
    double difference;

    if (value1 < 0) {
        value1 = -value1;
    }
    if (value2 < 0) {
        value2 = -value2;
    }

    // Compare the min Y point.
    difference = value1 - value2;
    if (difference < 0) {
        difference = -difference;
    }
    if (difference <= resolution) {
        return(true);
    }

    return(false);
} // DoubleValuesAreClose






/////////////////////////////////////////////////////////////////////////////
//
// [LimitDoubleToFixedPrecision]
//
/////////////////////////////////////////////////////////////////////////////
static double 
LimitDoubleToFixedPrecision(double value, double precision) {
    int32 intNumPrecisionUnits = 0;

    // Express the value as a number of precision units. This should be close
    // to exact, since it is a double.
    double exactNumPrecisionUnits = (value / precision);

    // Cast the double to an int. This will truncate.
    int32 truncatedNumPrecisionUnits = (int32) exactNumPrecisionUnits;
    
    // Find the differences between rounding up and rounding down.
    double roundUpDifference = ((double) (truncatedNumPrecisionUnits + 1)) - exactNumPrecisionUnits;
    double roundDownDifference = exactNumPrecisionUnits - ((double) truncatedNumPrecisionUnits);
    
    if (roundUpDifference < roundDownDifference) {
        intNumPrecisionUnits = (int32) (truncatedNumPrecisionUnits + 1);
    } else {
        intNumPrecisionUnits = (int32) truncatedNumPrecisionUnits;
    }
    
    // Convert this back to a double.
    double newValue = intNumPrecisionUnits * precision;
    return(newValue);
} // LimitDoubleToFixedPrecision






/////////////////////////////////////////////////////////////////////////////
//
// [RoundDoubleToInt]
//
/////////////////////////////////////////////////////////////////////////////
static int32 
RoundDoubleToInt(double value) {
    // First cast the double to an int. This will truncate.
    int32 intValue = (int32) value;
    
    // Find the differences between rounding up and rounding down.
    double roundUpDifference = ((double) (intValue + 1)) - value;
    double roundDownDifference = value - ((double) intValue);
    
    if (roundUpDifference < roundDownDifference) {
        return(intValue + 1);
    } else {
        return(intValue);
    }
} // RoundDoubleToInt







#endif // _APPROXIMATE_MATH_H_




