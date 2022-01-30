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

    mbIsAvgCalculated = true;
    //mfAvgSpeed = 0;
    //musAvgDir = 0;
};

void VelocityVector::add(float speed, short direction)
{
    mbIsAvgCalculated = false;
    float rad = direction / 360.0 * M_TWOPI;
    mfSumU += speed * cos(rad);
    mfSumV += speed * sin(rad);
    muiCount++;
};

unsigned short VelocityVector::getDir()
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

/*GustVector::GustVector()
{
    clear();
};

void GustVector::clear()
{
    VelocityVector::clear();
    mfMaxAvgSpeed = 0;
    mfMaxAvgDir = 0;
};

void GustVector::add(float speed, short direction){
    VelocityVector::add(speed, direction);
    if (muiCount >= 3) {
        float avgSpeed = VelocityVector::getSpeed();
        if (avgSpeed > mfMaxAvgSpeed) {
            mfMaxAvgSpeed = avgSpeed;
            mfMaxAvgDir = VelocityVector::getDir();
        }
        VelocityVector::clear();
    }
};

unsigned short GustVector::getDir(){
    return mfMaxAvgSpeed;
};

float GustVector::getSpeed(){
    return mfMaxAvgDir;
}; */
