#ifndef PDCSMEARER_HH
#define PDCSMEARER_HH

#include "PDCRotation.hh"
#include <TVector3.h>
#include <TString.h>
#include <TRandom3.h>

/**
 * @class PDCSmearer
 * @brief Applies Gaussian position smearing to PDC hits.
 *
 * This class uses a PDCRotation object to transform hits to the local frame,
 * applies a 2D Gaussian smearing in the local XY plane, and transforms
 * the result back to the global frame.
 */
class PDCSmearer {
public:
    /**
     * @brief Constructor.
     * @param rotation The PDCRotation object defining the coordinate system.
     * @param sigmaU The position resolution (sigma) for the U layer in mm.
     * @param sigmaV The position resolution (sigma) for the V layer in mm.
     * @param sigmaX The position resolution (sigma) for the X layer in mm.
     */
    PDCSmearer(const PDCRotation& rotation, double sigmaU, double sigmaV, double sigmaX);
    ~PDCSmearer() = default;

    /**
     * @brief Applies smearing to a global position vector based on the module type.
     * @param globalPos The original hit position in the global frame.
     * @param moduleName The name of the detector layer ("U", "V", or "X").
     * @return TVector3 The new, smeared position in the global frame.
     */
    TVector3 Smear(const TVector3& globalPos, const TString& moduleName);

private:
    PDCRotation fRotation;
    TRandom3 fRandom;
    double fSigmaU;
    double fSigmaV;
    double fSigmaX;
};

#endif // PDCSMEARER_HH
