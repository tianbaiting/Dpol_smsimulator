#include "EventDataReader.hh"
#include "TSystem.h"
#include "TBeamSimData.hh"
#include <iostream>

EventDataReader::EventDataReader(const char* filePath) 
    : m_file(nullptr), m_tree(nullptr), m_fragSimDataArray(nullptr), m_nebulaDataArray(nullptr),
      m_beamDataVector(nullptr), m_currentEvent(-1), m_totalEvents(0), m_filePath(filePath) 
{
    // It is good practice to load libraries in the main macro,
    // but loading here ensures the class is self-contained.
    gSystem->Load("libsmdata.so");

    m_file = TFile::Open(m_filePath.Data());
    if (!IsOpen()) {
        std::cerr << "Error: EventDataReader cannot open ROOT file: " << m_filePath << std::endl;
        return;
    }
    m_tree = (TTree*)m_file->Get("tree");
    if (!m_tree) {
        std::cerr << "Error: EventDataReader cannot find tree in ROOT file!" << std::endl;
        m_file->Close();
        m_file = nullptr;
        return;
    }
    m_tree->SetBranchAddress("FragSimData", &m_fragSimDataArray);
    
    // Try to set NEBULA branch
    if (m_tree->GetBranch("NEBULAPla")) {
        m_tree->SetBranchAddress("NEBULAPla", &m_nebulaDataArray);
        std::cout << "EventDataReader: Found NEBULAPla branch" << std::endl;
    } else {
        std::cout << "EventDataReader: No NEBULAPla branch found in file" << std::endl;
        m_nebulaDataArray = nullptr;
    }
    
    // Try to set beam branch using vector<TBeamSimData> format
    if (m_tree->GetBranch("beam")) {
        m_tree->SetBranchAddress("beam", &m_beamDataVector);
        std::cout << "EventDataReader: Found beam branch (vector<TBeamSimData> format)" << std::endl;
    } else {
        std::cout << "EventDataReader: No beam branch found in file" << std::endl;
        m_beamDataVector = nullptr;
    }
    
    m_totalEvents = m_tree->GetEntries();
    std::cout << "EventDataReader: Opened " << m_filePath << " with " << m_totalEvents << " events." << std::endl;
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
        std::cerr << "Error: Event number " << eventNumber << " is out of range." << std::endl;
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
