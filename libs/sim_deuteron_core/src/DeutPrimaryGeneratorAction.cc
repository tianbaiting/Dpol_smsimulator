#include "DeutPrimaryGeneratorAction.hh"
#include "TBeamSimData.hh"
#include "TFragSimParameter.hh"
#include "SimDataManager.hh"
#include "SMLogger.hh"

#include "G4ParticleTable.hh"
#include "G4IonTable.hh"
#include "G4ParticleGun.hh"
#include "G4SystemOfUnits.hh"

#include <cmath>

//____________________________________________________________________
DeutPrimaryGeneratorAction::DeutPrimaryGeneratorAction(G4int seed)
  : PrimaryGeneratorActionBasic(seed),
    fUseTargetParameters(true)
{}
//____________________________________________________________________
DeutPrimaryGeneratorAction::~DeutPrimaryGeneratorAction()
{}
//____________________________________________________________________
void DeutPrimaryGeneratorAction::SetPrimaryVertex(G4Event* anEvent)
{
  if(!gBeamSimDataArray) return;

  int n=gBeamSimDataArray->size();
  for (int i=0;i<n;++i){
    TBeamSimData beam = (*gBeamSimDataArray)[i];
    //std::cout<<"primaryID="<<i<<" "<<beam<<std::endl;

    if      (beam.fParticleName=="neutron") {if (fSkipNeutron) continue;}
    else if (beam.fParticleName=="gamma")   {if (fSkipGamma) continue;}
    else                                    {if (fSkipHeavyIon) continue;}
    
    G4ParticleDefinition *particle = 0;
    particle = fParticleTable->FindParticle(beam.fParticleName.Data());
    if (particle==0) 
      particle = fParticleTable->GetIonTable()->GetIon(beam.fZ, beam.fA, 0.0);
    if (particle==0){
      SM_ERROR("Cannot find particle: {}", beam.fParticleName.Data());
    }
    fParticleGun->SetParticleDefinition(particle);
    G4ThreeVector dir(beam.fMomentum.Px(),
		      beam.fMomentum.Py(),
		      beam.fMomentum.Pz() );
    const G4double mass_mev = particle->GetPDGMass()/MeV;
    const G4double momentum2 = dir.mag2();
    const G4double kinetic_mev = std::sqrt(momentum2 + mass_mev * mass_mev) - mass_mev;
    // [EN] Derive kinetic energy from the Geant4 particle mass so ion primaries remain consistent even when the text input carries approximate rest masses. / [CN] 由Geant4粒子质量反推动能，使文本输入只给出近似静质量时重离子初级粒子仍保持一致。
    fParticleGun->SetParticleEnergy(kinetic_mev*MeV);
    G4ThreeVector pos_lab(beam.fPosition.x()*mm,
			  beam.fPosition.y()*mm,
			  beam.fPosition.z()*mm );

    /* The collision point is set to the target center and the beam direction is
     * assumed to be perpendicular to the target. Those angles in the input tree
     * are defined with the beam direction as Z-axis, therefore they need to be
     * rotated *-angle_tgt* for the labortory frame.
     */
    if (fUseTargetParameters){
      auto *sman = SimDataManager::GetSimDataManager();
      auto *frag_prm = (TFragSimParameter*)sman->FindParameter("FragParameter");
      
      TVector3 pos_tgt = frag_prm->fTargetPosition;
      Double_t angle_tgt = frag_prm->fTargetAngle;
      pos_lab.set(pos_tgt.X(), pos_tgt.Y(), pos_tgt.Z());
      dir.rotateY(-angle_tgt);
    }

    if (fSkipLargeYAng){
      G4ExceptionDescription ed;
      ed << "The parameter *fSkipLargeYAng* is set to true, " \
         << "but it's not applicable to this experiment"; 
      G4Exception("DeutPrimaryGeneratorAction::SetPrimaryVertex()", 
                  "/action/gun/SkipLargeYAng", JustWarning, ed);
    }

    fParticleGun->SetParticleMomentumDirection(dir);
    fParticleGun->SetParticlePosition(pos_lab);
    fParticleGun->GeneratePrimaryVertex(anEvent);
    fParticleGun->SetParticleTime(beam.fTime*ns);
  }
}
