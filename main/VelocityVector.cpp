#include "VelocityVector.h"
#include "math.h"

VelocityVector::VelocityVector()
{
    clear();
}

void VelocityVector::clear()
{
    muiCount = 0;
    mfSumU = 0;
    mfSumV = 0;

};

void VelocityVector::add(float speed, short direction)
{
    float rad = direction / 360.0 * M_TWOPI;
    mfSumU += speed * cos(rad);
    mfSumV += speed * sin(rad);
    muiCount++;
};

short VelocityVector::getDir()
{
    if (!muiCount) 
        return 0;

    float avgDir = atan2(mfSumV, mfSumU) * 360.0 / M_TWOPI;
    return round(avgDir < 0.0 ? 360.0 + avgDir : avgDir);
};

float VelocityVector::getSpeed()
{
    if (!muiCount) 
        return 0.0;

    float avgU = mfSumU / muiCount;
    float avgV = mfSumV / muiCount;
    return sqrt(avgU * avgU + avgV * avgV);
};
