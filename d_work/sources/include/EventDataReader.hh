#ifndef EVENT_DATA_READER_H
#define EVENT_DATA_READER_H

#include "TFile.h"
#include "TTree.h"
#include "TClonesArray.h"
#include "TString.h"

class EventDataReader {
public:
    EventDataReader(const char* filePath);
    ~EventDataReader();

    bool IsOpen() const { return m_file != nullptr && !m_file->IsZombie(); }

    bool NextEvent();
    bool PrevEvent();
    bool GoToEvent(Long64_t eventNumber);

    TClonesArray* GetHits() const { return m_fragSimDataArray; }
    Long64_t GetCurrentEventNumber() const { return m_currentEvent; }
    Long64_t GetTotalEvents() const { return m_totalEvents; }
    const char* GetFilePath() const { return m_filePath.Data(); }

private:
    TFile* m_file;
    TTree* m_tree;
    TClonesArray* m_fragSimDataArray;
    Long64_t        m_currentEvent;
    Long64_t        m_totalEvents;
    TString         m_filePath;
};

#endif // EVENT_DATA_READER_H
