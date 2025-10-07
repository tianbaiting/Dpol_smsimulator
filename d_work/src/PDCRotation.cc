#include "PDCRotation.hh"

PDCRotation::PDCRotation(double angle_rad)
{
    // Set up the rotation from the Global frame to the Local frame.
    // A positive rotation of the detector by angle `alpha` around the Y-axis
    // means that to transform a vector from Global to Local, we must rotate
    // the vector by `-alpha` around the Y-axis.
    fGlobalToLocal.RotateY(-angle_rad);
}

TVector3 PDCRotation::ToLocal(const TVector3& globalVec) const
{
    TVector3 localVec = globalVec;
    localVec.Transform(fGlobalToLocal);
    return localVec;
}

TVector3 PDCRotation::ToGlobal(const TVector3& localVec) const
{
    TVector3 globalVec = localVec;
    // The inverse of a rotation matrix is its transpose.
    // For TRotation, Inverse() is the correct method to get the reverse transformation.
    globalVec.Transform(fGlobalToLocal.Inverse());
    return globalVec;
}
