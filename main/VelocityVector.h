#ifndef INCLUDE_VELOCITY_VECTOR_H_
#define INCLUDE_VELOCITY_VECTOR_H_

class VelocityVector
{
public:
    VelocityVector();

    void clear();
    void add(float speed, short direction);
    unsigned short getDir();
    float getSpeed();
    unsigned int getCount() { return muiCount; };

protected:
    unsigned int muiCount;
    float mfSumU;
    float mfSumV;

    //float mfAvgSpeed;
    //unsigned short musAvgDir; 

    bool mbIsAvgCalculated;
};

/*
class GustVector: public VelocityVector
{
    public:
        GustVector();
        void clear();
        void add(float speed, short direction);
        unsigned short getDir();
        float getSpeed();
    private:
        float mfMaxAvgSpeed;
        unsigned short mfMaxAvgDir;
};
*/

#endif