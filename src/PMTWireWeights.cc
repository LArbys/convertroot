#include "PMTWireWeights.h"

namespace larbys {
  namespace util {
    
    PMTWireWeights::PMTWireWeights() {
      fGeoInfoFile = "configfiles/geoinfo.root";
      fGeoFile = new TFile( fGeoInfoFile.c_str(), "OPEN" );
      fNPMTs = 32;
      fPMTTree  = (TTree*)fGeoFile->Get( "imagedivider/pmtInfo" );
      int femch;
      float pos[3];
      fPMTTree->SetBranchAddress( "femch", &femch );
      fPMTTree->SetBranchAddress( "pos", pos );
      for (int n=0; n<fNPMTs; n++) {
	fPMTTree->GetEntry();
	for (int i=0; i<3; i++)
	  pmtpos[femch][i] = pos[i];
      }

      fWireTree = (TTree*)fGeoFile->Get( "imagedivider/wireInfo" );
      
    }

    PMTWireWeights::~PMTWireWeights() {
    }

    void PMTWireWeights::configure() {
    }

    void PMTWireWeights::buildconfig() {
    }

    


  }
}
