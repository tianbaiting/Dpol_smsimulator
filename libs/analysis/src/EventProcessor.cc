#include "EventProcessor.hh"
#include "SMLogger.hh"

#include "TTree.h"

ClassImp(EventProcessor); // ROOT RTTI

EventProcessor::EventProcessor(TTree *inTree, TTree *outTree)
    : fInTree(inTree), fOutTree(outTree) {
    if (fInTree) {
        // Set branch addresses for input tree
        // Example:
        // fInTree->SetBranchAddress("InputBranchName", &fInputVariable);
    }
    if (fOutTree) {
        // Create branches for output tree
        // Example:
        // fOutTree->Branch("OutputBranchName", &fOutputVariable);
    }
}

EventProcessor::~EventProcessor() {
    // Destructor
}

void EventProcessor::Begin() {
    // Called once at the beginning of the analysis
    SM_INFO("EventProcessor: Starting analysis...");
}

void EventProcessor::ProcessEvent() {
    // Called for each event in the input tree
    // This is where the main analysis logic goes.

    // Example:
    // fOutputVariable = fInputVariable * 2.0;
    // fOutTree->Fill();
}

void EventProcessor::End() {
    // Called once at the end of the analysis
    SM_INFO("EventProcessor: Finished analysis.");
}
