#ifndef EVENT_DATA_READER_H
#define EVENT_DATA_READER_H

#include "TFile.h"
#include "TTree.h"
#include "TClonesArray.h"
#include "TString.h"
#include "TLorentzVector.h"
#include <vector>

// Forward declaration for TBeamSimData from smg4lib
class TBeamSimData;
class TNEBULASimParameter;
class TNEBULAPlusSimParameter;

class EventDataReader {
public:
    EventDataReader(const char* filePath);
    ~EventDataReader();

    bool IsOpen() const { return m_file != nullptr && !m_file->IsZombie(); }

    bool NextEvent();
    bool PrevEvent();
    bool GoToEvent(Long64_t eventNumber);

    TClonesArray* GetHits() const { return m_fragSimDataArray; }
    TClonesArray* GetNEBULAHits() const { return m_nebulaDataArray; }
    TClonesArray* GetNEBULAPlusHits() const { return m_nebulaPlusDataArray; }
    bool HasNEBULABranch() const { return m_hasNEBULABranch; }
    bool HasNEBULAPlusBranch() const { return m_hasNEBULAPlusBranch; }
    const TNEBULASimParameter* GetNEBULAParameter() const { return m_nebulaParameter; }
    const TNEBULAPlusSimParameter* GetNEBULAPlusParameter() const { return m_nebulaPlusParameter; }
    
    // Beam data access methods
    const std::vector<TBeamSimData>* GetBeamData() const { return m_beamDataVector; }
    int GetBeamParticleCount() const;
    const TBeamSimData* GetBeamParticle(int index) const;
    
    Long64_t GetCurrentEventNumber() const { return m_currentEvent; }
    Long64_t GetTotalEvents() const { return m_totalEvents; }
    const char* GetFilePath() const { return m_filePath.Data(); }

private:
    TFile* m_file;
    TTree* m_tree;
    TClonesArray* m_fragSimDataArray;
    TClonesArray* m_nebulaDataArray;
    TClonesArray* m_nebulaPlusDataArray;
    std::vector<TBeamSimData>* m_beamDataVector;
    TNEBULASimParameter* m_nebulaParameter;
    TNEBULAPlusSimParameter* m_nebulaPlusParameter;
    bool m_hasNEBULABranch;
    bool m_hasNEBULAPlusBranch;
    Long64_t        m_currentEvent;
    Long64_t        m_totalEvents;
    TString         m_filePath;
};

#endif // EVENT_DATA_READER_H
