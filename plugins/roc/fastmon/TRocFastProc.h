#ifndef TUNPACKPROCESSOR_H
#define TUNPACKPROCESSOR_H

#include "TGo4EventProcessor.h"

#include <vector>
#include <stdint.h>


class TRocFastParam;
class TRocFastControl;

class TRocFastProc : public TGo4EventProcessor {
   public:
      TRocFastProc() ;
      TRocFastProc(const char* name);
      virtual ~TRocFastProc() ;

      Bool_t BuildEvent(TGo4EventElement* target); // event processing function

 private:
      TGo4MbsEvent  *fInput;

      TRocFastParam *fParam;
      TRocFastControl *fControl;

      TH1I          *fEvntSize;
      TH1I          *fADC;
      TH1I          *fChannels;
      TH1I          *fTmStamp;
//      TH1I          *fAUXall;

      TH2I          *fGEM;
      
      unsigned       fNumRocs;
      
      UInt_t         fCurrEpoch;
      UInt_t         fCurrEvent;
      UInt_t         fCurrEvntTm;
      
      std::vector<uint64_t>  fLastAUX; // last aux signal (one per roc)      
//      std::vector<TH1I*> fAUX; // distance between two aux signals
      std::vector<TH2I*> fADCs; // individual ADC histogram for each ROC
      std::vector<TH1I*> fChs; // individual ADC histogram for each ROC
      std::vector<TH1I*> fAUXt; // distance between two aux signals
      std::vector<TH1I*> fAUXch; // filling of AUX channels
      std::vector<TH2I*> fCorrel; // correlation
      std::vector<TH1I*> fHitQuality; // hit quality
      std::vector<TH1I*> fMsgTypes; // hit quality

      std::vector<uint64_t> fLastTm;
      std::vector<uint16_t> fLastCh;
      uint64_t fLastAUXtm;

   ClassDef(TRocFastProc,1)
};

#endif //TUNPACKPROCESSOR_H

