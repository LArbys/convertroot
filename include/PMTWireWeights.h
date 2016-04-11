#ifndef __PMTWireWeights__
#define __PMTWireWeights__

// this class makes a helper image that "weights" the charge 
// on a wire based on the pmt flash location

#include <string>
#include "TFile.h"

namespace larbys {
  namespace util {

    class PMTWireWeights {

    public:
      PMTWireWeights() {};
      virtual ~PMTWireWeights() {};

      void configure();

      void buildconfig();
      std::string fGeoInfoFile;

      TFile* fGeoFile;
      TTree* fPMTTree;
      int fNPMTs;
      float pmtpos[32][3];
      
      TTree* fWireTree;
      

    };
    
  }
}

#endif
