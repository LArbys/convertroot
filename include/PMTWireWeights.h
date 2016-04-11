#ifndef __PMTWireWeights__
#define __PMTWireWeights__

// this class makes a helper image that "weights" the charge 
// on a wire based on the pmt flash location

#include <string>
#include <vector>
#include <map>
#include <set>

#include "TFile.h"
#include "TTree.h"

#include "opencv/cv.h"

namespace larbys {
  namespace util {

    class WireData {
      public:
      WireData( int pid=0 ) { planeID=pid; };
      ~WireData() {};
      
      int planeID; // plane number
      std::set< int > wireIDs;                       // set of wireIDs
      std::map< int, std::vector<float> > wireStart; // index is wireID, key is the start position of a wire
      std::map< int, std::vector<float> > wireEnd;   // index is wireID, key is the end position of a wire
      std::map< int, float >              wireL2;    // index is wireID, key is normalized wire direction
      std::map< int, std::vector<float> > wireDir;   // index is wireID, key is normalized wire direction
      std::map< int, std::vector<float> > wirePDir;  // index is wireID, key is cross product of wire dir and (1,0,0), the direction of the wire plane      

      int nwires() const { return (int)wireIDs.size(); };
      
      void addWire( int id, float start[], float end[] ) {
	wireIDs.insert(id);
	std::vector<float> vstart(3,0.0);
	std::vector<float> vend(3,0.0);
	std::vector<float> vdir(3, 0.0 );
	std::vector<float> pdir(3, 0.0 );

	// store start, end, calculate dir
	float norm = 0;
	for (int i=0; i<3; i++) {
	  vstart[i] = start[i];
	  vend[i] = end[i];
	  float dx = vend[i]-vstart[i];
	  norm += dx*dx;
	  vdir[i] = dx;
	}
	float l2 = norm;
	norm = sqrt(norm);
	for (int i=0; i<3; i++)
	  vdir[i] /= norm;
	
	// take cross product of x+ and dir
	pdir[0] = 0.0;
	pdir[1] = -vdir[2];
	pdir[2] = vdir[1];
	norm = 0.0;
	for (int i=0; i<3; i++) {
	  norm += pdir[i]*pdir[i];
	}
	norm = sqrt(norm);
	for (int i=0; i<3; i++)
	  pdir[i] /= norm;

	wireStart[id] = vstart;
	wireEnd[id]    = vend;
	wireDir[id]    = vdir;
	wirePDir[id]   = pdir;
	wireL2[id]     = l2;
	
      };
      
      
    };

    class PMTWireWeights {

    public:
      PMTWireWeights( int wire_rows=-1 );
      // by default, we allocate just enough memory to fill weights for the number of wires in each plane.
      // however, our ultimate intention is to apply this to an image, so we can set the width of that image here
      // if wire_rows < number of wires we trunctate. if wire_rows > number of wires we zero pad after the highest wire number
      virtual ~PMTWireWeights();

      //void configure();

      //void buildconfig();
      std::string fGeoInfoFile;

      TFile* fGeoFile;
      TTree* fPMTTree;
      int fNPMTs;
      int fNWires;
      int femch;
      float pmtpos[32][3];
      
      TTree* fWireTree;
      std::map<int,WireData> m_WireData; // key is plane ID, value is class with wire info

      // The object we end up filling
      std::map<int, cv::Mat> planeWeights; // index is the plane number

      float getDistance2D( float s[], float e[], float p[], float l2 );
      void configure();

    };
    
  }
}

/* 
CODE USES THE FOLLOWING ROOT TREE

root [5] wireInfo->Print()
******************************************************************************
*Tree    :wireInfo  : Wire Geo Info                                          *
*Entries :     8256 : Total =          267376 bytes  File  Size =      72869 *
*        :          : Tree compression factor =   3.68                       *
******************************************************************************
*Br    0 :plane     : plane/I                                                *
*Entries :     8256 : Total  Size=      33650 bytes  File Size  =        386 *
*Baskets :        2 : Basket Size=      32000 bytes  Compression=  85.95     *
*............................................................................*
*Br    1 :wireID    : wireID/I                                               *
*Entries :     8256 : Total  Size=      33656 bytes  File Size  =      11542 *
*Baskets :        2 : Basket Size=      32000 bytes  Compression=   2.87     *
*............................................................................*
*Br    2 :start     : start[3]/F                                             *
*Entries :     8256 : Total  Size=      99856 bytes  File Size  =      30375 *
*Baskets :        4 : Basket Size=      32000 bytes  Compression=   3.27     *
*............................................................................*
*Br    3 :end       : end[3]/F                                               *
*Entries :     8256 : Total  Size=      99840 bytes  File Size  =      29822 *
*Baskets :        4 : Basket Size=      32000 bytes  Compression=   3.33     *
*............................................................................*
root [5] pmtInfo->Print()
******************************************************************************
*Tree    :pmtInfo   : PMT Geo Info                                           *
*Entries :       32 : Total =            1966 bytes  File  Size =       1063 *
*        :          : Tree compression factor =   1.10                       *
******************************************************************************
*Br    0 :femch     : femch/I                                                *
*Entries :       32 : Total  Size=        677 bytes  File Size  =        145 *
*Baskets :        1 : Basket Size=      32000 bytes  Compression=   1.40     *
*............................................................................*
*Br    1 :pos       : pos[3]/F                                               *
*Entries :       32 : Total  Size=        929 bytes  File Size  =        457 *
*Baskets :        1 : Basket Size=      32000 bytes  Compression=   1.00     *
*............................................................................*
*/

#endif
