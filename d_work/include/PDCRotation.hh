#ifndef PDCROTATION_HH
#define PDCROTATION_HH

#include <TRotation.h>
#include <TVector3.h>

/**
 * @class PDCRotation
 * @brief Handles coordinate transformations between global and PDC-local frames.
 *
 * This class is initialized with the PDC's rotation angle around the Y-axis
 * and provides methods to transform a TVector3 to and from the PDC's
 * local coordinate system.
 */
class PDCRotation {
public:
    /**
     * @brief Constructor.
     * @param angle_rad The rotation angle of the PDC around the Y-axis in radians.
     *                  The sign should match the definition used in the simulation.
     */
    PDCRotation(double angle_rad);
    ~PDCRotation() = default;

    /**
     * @brief Transforms a vector from the Global frame to the PDC's Local frame.
     * @param globalVec A TVector3 in the global coordinate system.
     * @return TVector3 The corresponding vector in the local coordinate system.
     */
    TVector3 ToLocal(const TVector3& globalVec) const;

    /**
     * @brief Transforms a vector from the PDC's Local frame to the Global frame.
     * @param localVec A TVector3 in the local coordinate system.
     * @return TVector3 The corresponding vector in the global coordinate system.
     */
    TVector3 ToGlobal(const TVector3& localVec) const;

private:
    TRotation fGlobalToLocal;
};

#endif // PDCROTATION_HH
