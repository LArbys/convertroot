#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <set>
#include <utility>
#include <algorithm>

// This prepares data stored in LArby's rootfiles into LMDB format for caffe.
// The reason we have a separate program is this task has a number of specializations
// (1) determine labels based on mode, nuscatter and flavor categories
// (2) The BNB data is already shuffled. No need to shuffle classes like the single particle code.
// (3) We use two different cosmic ray muon sources: (1) EXT unbiased and (2) EXT+BNB SWTRIG
//     The first we combine with MC neutrinos, the second we use for the background-only sample
// (4) We arrange the data for a trimese network. Each plane gets the image, plus addition data:
//     (1) a pmt-charge weighted image and (2) HIP and MIP filtered images
//     This data is arranged [plane0,plane0-pmtweighted,plane0-MIP,plane0-HIP,plane1,...]
// (5) We pad in the time region to allow for different crops during training
// (6) We also reuse the MC neutrinos (several times), changing the cosmic background 
//      and the scaling factor applied to the MC ADCs.
// (7) We vary the ADC threshold slightly as well

// Caffe and LMDB
#include "lmdb.h"
#include "caffe/common.hpp"
#include "caffe/proto/caffe.pb.h"
#include "caffe/util/format.hpp"
#include "caffe/util/db.hpp"
#include "caffe/util/io.hpp"

// Boost
#include "boost/scoped_ptr.hpp"

// OpenCV
#include "opencv/cv.h"
#include "opencv2/opencv.hpp"

// larbys root utilities
#include "adc2rgb.h"
#include "root2image.h"
#include "root2datum.h"
#include "emptyfilter.h"
#include "BNBLabels.h"
#include "UBBadWires.h"
#include "PMTWireWeights.h"
#include "HMFilter.h"

// ROOT
#include "TFile.h"
#include "TChain.h"
#include "TRandom3.h"

void  parse_inputlist( std::string filename, std::vector<std::string>& inputlist ) {

  std::ifstream infile( filename.c_str() );
  char buffer[5120];
  std::string fname;
  std::string lastname = "";
  while ( !infile.eof() ) {
    infile >> buffer;
    fname = buffer;
    if ( fname!="" && fname!=lastname ) {
      inputlist.push_back( fname );
      lastname = fname;
    }
  }
}


int main( int narg, char** argv ) {


  std::string infile = argv[1];
  std::string outdb  = argv[2];
  std::string enc = "";
  int SEED = 123567;
  bool fTrinocular = true; // fold in all three views into data
  bool fAddPMT = true;
  std::string producer = "bnbcosmics";
  const int fNumPlanes = 3;
  double fEnergyCut = 0.400;
  bool fApplyBadWireMask = true;
  float train_val_split = 0.7;

  typedef enum { kNu=0, NTYPES } FileTypes_t;
  typedef enum { kTRAIN=0, kTEST, NOUTPUTS } Outputs_t;

  larbys::util::Root2Datum::ColorOption_t fColor = larbys::util::Root2Datum::kGreyScale;
  
  // read input files
  std::vector< std::string > inputlist[NTYPES];
  parse_inputlist( infile, inputlist[kNu] );
  
  // load trees, setup branches and count number of entries
  // type of tree used for neutrinos
  TChain** classtrees = new TChain*[NTYPES];
  for (int i=0; i<NTYPES; i++) {
    classtrees[i] = new TChain( std::string(producer+"/imgtree").c_str() );
    for ( int j=0; j<inputlist[i].size(); j++)
      classtrees[i]->Add( inputlist[i].at(j).c_str() );
  }

  std::cout << "Setup Image Maker" << std::endl;
  larbys::util::Root2Datum* root2datum[NTYPES];
  for (int i=0; i<NTYPES; i++) {
    root2datum[i] = new larbys::util::Root2Datum( classtrees[i], larbys::util::Root2Datum::kTrinocular, fColor, fAddPMT );
    root2datum[i]->convertor.colorscale.setADC_MIN(0);
    root2datum[i]->convertor.colorscale.setADC_MAX(255);
    root2datum[i]->convertor.applyThreshold( true );
    root2datum[i]->convertor.setThreshold( 10.0 );
    root2datum[i]->convertor.setTimePadding( 10 );
    if ( fAddPMT )
      root2datum[i]->convertor.setPMTimageFormat( larbys::util::Root2Image::kPMTtimescale );
  }

  // Get Total Entries
  int sampleentries[NTYPES];
  for (int i=0; i<NTYPES; i++) {
    sampleentries[i] = classtrees[i]->GetEntries();
  }

  std::cout << "[COMPONENT SAMPLE ENTRIES]" << std::endl;
  std::cout << "ENTRIES TO PROCESS : " << sampleentries[kNu] << std::endl;
  std::cin.get();

  // Output LMDB
  std::string FLAGS_backend = "lmdb";
  boost::scoped_ptr<caffe::db::DB> db(caffe::db::GetDB(FLAGS_backend));
  db->Open(outdb.c_str(), caffe::db::NEW);
  boost::scoped_ptr<caffe::db::Transaction> txn(db->NewTransaction());

  // bad wire tool
  larbys::util::UBBadWires* badwires[fNumPlanes];
  badwires[0] = new larbys::util::UBBadWires( 0.01 );
  badwires[1] = new larbys::util::UBBadWires( 0.01 );
  badwires[2] = new larbys::util::UBBadWires( 0.01 );

  // PMT wire weight tool
  larbys::util::PMTWireWeights pmtweights( 768*5 );

  // HIP/MIP Filter
  larbys::util::HMFilter* hmfilter[3];
  for (int i=0; i<3; i++) {
    hmfilter[i] = new larbys::util::HMFilter(768,768);
    hmfilter[i]->setMIPbounds( 15, 45 );
  }
  
  // Image Protobuf
  caffe::Datum datum;
  caffe::Datum datum_final;

  // Event Loop

  int numrejected = 0;
  TRandom3 rand( SEED );

  int entry[NTYPES];
  int filled[NTYPES];
  long bytes[NTYPES];
  for (int itype=0; itype<NTYPES; itype++ ) {
    entry[itype] = 0;
    filled[itype] = 0;
    bytes[itype] = classtrees[itype]->GetEntry( entry[itype] );
  }

  std::cout << "START LOOP" << std::endl;

  FileTypes_t next_type_to_fill = kNu;
  
  while ( entry[kNu]<sampleentries[kNu] ) {

    FileTypes_t ftype = kNu;
    larbys::util::BNBLabels_t bnblabel = larbys::util::kBackground;

    bytes[kNu] = classtrees[kNu]->GetEntry( entry[kNu] );
    if ( bytes[kNu]==0 )
      break;
    entry[kNu]++;
              
    // if neutrino, add in the cosmic (after first removing bad wires
    cv::Mat plane_images;

    // extract BNB EXT Cosmic into datum
    // randomize threshold
    //float adc_threshold = rand.Gaus( 10.0, 0.5 );
    float adc_threshold = 10.0;
    root2datum[kNu]->convertor.setThreshold( adc_threshold );
    plane_images = root2datum[kNu]->fillDatum( datum, -1, false );
    filled[kNu]++;

    // [ now do all our fancy crap to the image ]

    // PMT Weighted
    cv::Mat pmtweighted;
    pmtweights.applyWeights( plane_images, *(root2datum[kNu]->p_pmt_highgain), *(root2datum[kNu]->p_pmt_lowgain), pmtweighted );

    cv::Mat chimages[3];
    cv::Mat chweighted[3];
    cv::split( plane_images, chimages );
    cv::split( pmtweighted, chweighted );
    for (int i=0; i<3; i++) {
      std::cout << "[plane " << i << "] " << chimages[i].size() << std::endl;
    }

    // HIP/MIP Filter
    cv::Mat chmip[3];
    cv::Mat chhip[3];
    for (int p=0; p<3; p++) {
      hmfilter[p]->applyFilter( chimages[p], chmip[p], chhip[p] );
    }

    // MERGE
    cv::Mat merge( plane_images.size().height, plane_images.size().width, CV_8UC(12) );
    for (int p=0; p<3; p++) {
      for (int h=0; h<plane_images.size().height; h++) {
	for (int w=0; w<plane_images.size().width; w++) {
	  merge.at< cv::Vec<uchar,12> >(h,w)[4*p + 0 ] = chimages[p].at< cv::Vec<uchar,1> >(h,w)[0];
	  merge.at< cv::Vec<uchar,12> >(h,w)[4*p + 1 ] = chweighted[p].at< cv::Vec<uchar,1> >(h,w)[0];
	  merge.at< cv::Vec<uchar,12> >(h,w)[4*p + 2 ] = chmip[p].at< cv::Vec<uchar,1> >(h,w)[0];
	  merge.at< cv::Vec<uchar,12> >(h,w)[4*p + 3 ] = chhip[p].at< cv::Vec<uchar,1> >(h,w)[0];
	}
      }
    }
    
    caffe::CVMatToDatum( merge, &datum_final );

    // set the label!!
    datum_final.set_label( 0 );

    // serialize
    std::string out;
    CHECK( datum_final.SerializeToString(&out) );
      
    // make key for db entry
    int numfilled = filled[kNu];
    std::string key_str = caffe::format_int(numfilled,10) + "_" + caffe::format_int( (int)bnblabel, 2  );
    
    // store in db
    txn->Put( key_str, out );
    
    if ( numfilled>0 && numfilled%100==0 ) {
      txn->Commit();
      txn.reset( db->NewTransaction() );
    }
    
    if ( numfilled>=50000 )
      break;

  }//end of while loop
  
  // last commit
  txn->Commit();
  txn.reset( db->NewTransaction() );
  
  std::cout << "FIN." << std::endl;
  std::cout << "Number of events filled: " << filled[kNu] << std::endl;
  
  return 0;
}
