#include "PDCSmearer.hh"

PDCSmearer::PDCSmearer(const PDCRotation& rotation, double sigmaU, double sigmaV, double sigmaX)
    : fRotation(rotation),
      fRandom(0), // Initialize with seed 0, TRandom3 will be seeded automatically
      fSigmaU(sigmaU),
      fSigmaV(sigmaV),
      fSigmaX(sigmaX)
{}

TVector3 PDCSmearer::Smear(const TVector3& globalPos, const TString& moduleName)
{
    double sigma = 0.0;
    if (moduleName == "U") {
        sigma = fSigmaU;
    } else if (moduleName == "V") {
        sigma = fSigmaV;
    } else if (moduleName == "X") {
        sigma = fSigmaX;
    }

    // If sigma is zero, or module name is not matched, return the original position.
    if (sigma <= 0.0) {
        return globalPos;
    }

    // 1. Transform from Global to Local coordinates
    TVector3 localPos = fRotation.ToLocal(globalPos);

    // 2. Apply 2D Gaussian smearing in the local XY plane
    double smearedLocalX = localPos.X() + fRandom.Gaus(0, sigma);
    double smearedLocalY = localPos.Y() + fRandom.Gaus(0, sigma);
    
    TVector3 smearedLocalPos(smearedLocalX, smearedLocalY, localPos.Z());

    // 3. Transform back from Local to Global coordinates
    TVector3 smearedGlobalPos = fRotation.ToGlobal(smearedLocalPos);

    return smearedGlobalPos;
}
