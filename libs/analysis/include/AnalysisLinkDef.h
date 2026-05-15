#ifdef __CINT__

#pragma link off all globals;
#pragma link off all classes;
#pragma link off all functions;

// 需要 ROOT 字典的类
#pragma link C++ class MagneticField+;
#pragma link C++ class EventProcessor+;
#pragma link C++ class PDCSimAna+;
// NEBULAReconstructor+ removed from dict: NEBULABaseReco.hh is now canonical
// for NEBULAHit; NEBULAReconstructor will be deleted in P2.7.
#pragma link C++ class NEBULABaseReco+;
#pragma link C++ class NEBULAReco+;
#pragma link C++ class RecoEvent+;
#pragma link C++ class RecoTrack+;
#pragma link C++ class RecoHit+;
#pragma link C++ class RecoNeutron+;

#endif
