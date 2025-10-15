#ifndef EVENTPROCESSOR_HH
#define EVENTPROCESSOR_HH

#include "TObject.h"

class TTree;

class EventProcessor : public TObject {
public:
    EventProcessor(TTree *inTree = 0, TTree *outTree = 0);
    virtual ~EventProcessor();

    // Methods for analysis loop
    void Begin();
    void ProcessEvent();
    void End();

private:
    TTree *fInTree;  // Input TTree
    TTree *fOutTree; // Output TTree

    // Declare your input branches here
    // Example:
    // Double_t fInputVariable;

    // Declare your output branches here
    // Example:
    // Double_t fOutputVariable;

    ClassDef(EventProcessor, 1); // ROOT RTTI
};

#endif // EVENTPROCESSOR_HH
