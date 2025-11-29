#include "EventDataReader.hh"
#include "SMLogger.hh"
#include "TSystem.h"
#include "TBeamSimData.hh"

EventDataReader::EventDataReader(const char* filePath) 
    : m_file(nullptr), m_tree(nullptr), m_fragSimDataArray(nullptr), m_nebulaDataArray(nullptr),
      m_beamDataVector(nullptr), m_currentEvent(-1), m_totalEvents(0), m_filePath(filePath) 
{
    // It is good practice to load libraries in the main macro,
    // but loading here ensures the class is self-contained.
    gSystem->Load("libsmdata.so");

    m_file = TFile::Open(m_filePath.Data());
    if (!IsOpen()) {
        SM_ERROR("EventDataReader cannot open ROOT file: {}", m_filePath.Data());
        return;
    }
    m_tree = (TTree*)m_file->Get("tree");
    if (!m_tree) {
        SM_ERROR("EventDataReader cannot find tree in ROOT file!");
        m_file->Close();
        m_file = nullptr;
        return;
    }
    m_tree->SetBranchAddress("FragSimData", &m_fragSimDataArray);
    
    // Try to set NEBULA branch
    if (m_tree->GetBranch("NEBULAPla")) {
        m_tree->SetBranchAddress("NEBULAPla", &m_nebulaDataArray);
        SM_INFO("EventDataReader: Found NEBULAPla branch");
    } else {
        SM_DEBUG("EventDataReader: No NEBULAPla branch found in file");
        m_nebulaDataArray = nullptr;
    }
    
    // Try to set beam branch using vector<TBeamSimData> format
    if (m_tree->GetBranch("beam")) {
        m_tree->SetBranchAddress("beam", &m_beamDataVector);
        SM_INFO("EventDataReader: Found beam branch (vector<TBeamSimData> format)");
    } else {
        SM_DEBUG("EventDataReader: No beam branch found in file");
        m_beamDataVector = nullptr;
    }
    
    m_totalEvents = m_tree->GetEntries();
    SM_INFO("EventDataReader: Opened {} with {} events.", m_filePath.Data(), m_totalEvents);
}

EventDataReader::~EventDataReader() {
    if (m_file) {
        m_file->Close();
        // m_file is owned by TFile::Open, ROOT will manage it.
    }
}

bool EventDataReader::GoToEvent(Long64_t eventNumber) {
    if (!IsOpen() || !m_tree) return false;
    if (eventNumber < 0 || eventNumber >= m_totalEvents) {
        SM_ERROR("Event number {} is out of range.", eventNumber);
        return false;
    }
    m_tree->GetEntry(eventNumber);
    m_currentEvent = eventNumber;
    return true;
}

bool EventDataReader::NextEvent() {
    if (m_currentEvent >= m_totalEvents - 1) return false;
    return GoToEvent(m_currentEvent + 1);
}

bool EventDataReader::PrevEvent() {
    if (m_currentEvent <= 0) return false;
    return GoToEvent(m_currentEvent - 1);
}

int EventDataReader::GetBeamParticleCount() const {
    if (!m_beamDataVector) return 0;
    return m_beamDataVector->size();
}

const TBeamSimData* EventDataReader::GetBeamParticle(int index) const {
    if (!m_beamDataVector || index < 0 || index >= (int)m_beamDataVector->size()) {
        return nullptr;
    }
    return &((*m_beamDataVector)[index]);
}
