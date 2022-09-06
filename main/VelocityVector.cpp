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

void VelocityVector::operator= (const VelocityVector& v) {
    mfSumU = v.mfSumU;
    mfSumV = v.mfSumV;
    muiCount = v.muiCount;
}

void VelocityVector::operator+= (const VelocityVector& v) {
    mfSumU += v.mfSumU;
    mfSumV += v.mfSumV;
    muiCount += v.muiCount;
}

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


// matches AvgLong intervals from Maximet

VelocityVectorMovingAverage::VelocityVectorMovingAverage(unsigned short intervals) {
    musIntervals= intervals;
    mpVelocityVectorArray = new VelocityVector[musIntervals];

    /*mpuiCounts = { new unsigned int[musIntervals]{} };
    mpSumsU = { new float[musIntervals]{} };
    mpSumsV = { new float[musIntervals]{} }; */
}; 

short VelocityVectorMovingAverage::getDir()
{
    return mVelocityVectorAvg.getDir();
};

float VelocityVectorMovingAverage::getSpeed()
{
    return mVelocityVectorAvg.getSpeed();
};

float VelocityVectorMovingAverage::getU() {
    return mVelocityVectorAvg.getU();
}

float VelocityVectorMovingAverage::getV() {
    return mVelocityVectorAvg.getV();
};
unsigned int VelocityVectorMovingAverage::getCount() {
    return mVelocityVectorAvg.getCount();
}


// this is moving average calculation is intended for very small arrays 
// it intentionally iterates over all entries, to avoid floating point rounding issues
void VelocityVectorMovingAverage::add(VelocityVector& velocityVector) {
    if (musEntryPos >= musIntervals) {
        musEntryPos = 0;
    }
    /*mpSumsU[musEntryPos] = velocityVector.mfSumU;
    mpSumsV[musEntryPos] = velocityVector.mfSumV;
    mpuiCounts[musEntryPos] = velocityVector.muiCount; */
    mpVelocityVectorArray[musEntryPos] = velocityVector;

    if (musEntries < musIntervals) {
        musEntries++;
    }

    mVelocityVectorAvg.clear();
    for (unsigned short i = 0; i < musEntries; i++) {
        /*mVelocityVectorAvg.mfSumU += mpSumsU[i];
        mVelocityVectorAvg.mfSumV += mpSumsV[i];
        mVelocityVectorAvg.muiCount += mpuiCounts[i]; */
        mVelocityVectorAvg += mpVelocityVectorArray[i];
    }

    musEntryPos++;
};


VelocityVectorMovingAverage::~VelocityVectorMovingAverage() {
    delete[] mpVelocityVectorArray;
}