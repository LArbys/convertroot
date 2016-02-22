#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <set>
#include <utility>

// This prepares data stored in LArby's rootfiles into LMDB format for caffe.
// The reason we have a separate program is this task has a number of specializations
// (1) determine labels based on mode, nuscatter and flavor categories
// (2) The BNB data is already shuffled. No need to shuffle classes like the single particle code.



// Caffe and LMDB
#include "lmdb.h"
#include "caffe/common.hpp"
//#include "caffe.pb.h" // datum definition
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
#include "emptyfilter.h"
#include "BNBLabels.h"

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

  typedef enum { kCropBG, kOverlayBG } ProcessModes_t;
  typedef enum { kOverlayMax, kOverlaySum } OverlayMode_t;
  // kCropBG, folds in a background sample consisting of H x H crops of cosmic MC images (default H=224)
  // kOverlayBG, overlays random background with neutrino event.  Folds in examples with no BNB neutrino as well. (default image 448 x 448 )
  //   for overlay mode, we can either combine by choosing the max value between neutrino and cosmic
  //   or we combine by summing pixel values

  std::string infile_neutrino = argv[1];
  std::string infile_cosmics = argv[2]; // future
  std::string outdb = argv[3];
  std::string enc = "";
  int SEED = 12356;
  bool write_rejected = true; // save rejected neutrino events (ones with too few pixels below threshold)
  bool kTrinocular = false; // fold in all three views into data
  float kBGratio = 1.0; // 1-1 BG to 

  typedef enum { kNu=0, kCosmic, NTYPES } FileTypes_t;

  // read input files
  std::vector< std::string > inputlist[2];
  parse_filelist( infile_neutrino, inputlist[kNu] );
  //parse_filelist( infile_cosmics, inputlist[kCosmic] );
  
  // load trees, setup branches and count number of entries
  TChain** classtrees = new TChain*[NTYPES];
  std::vector<int>** p_bb_img_plane0 = new std::vector<int>*[NTYPES];
  std::vector<int>** p_bb_img_plane1 = new std::vector<int>*[NTYPES];
  std::vector<int>** p_bb_img_plane2 = new std::vector<int>*[NTYPES];
  int mode, nuscatter, flavor;
  for (int itype=0; itype<NTYPES; itype++) {
    classtrees[itype] = new TChain( "yolo/bbtree" );
    p_bb_img_plane2[itype] = 0;
    // we could loop, but for now assume number of entries = nfiles*50;
    for ( std::vector<std::string>::iterator it_file=inputlist[itype]->begin(); it_file!=inputlist[itype]->end(); it_file++ )
      classtrees[itype]->Add( (*it_file).c_str() );
    if ( kTrinocular ) {
      classtrees[itype]->SetBranchAddress("img_plane0", &p_bb_img_plane0[itype]);
      classtrees[itype]->SetBranchAddress("img_plane1", &p_bb_img_plane1[itype]);
    }
    classtrees[itype]->SetBranchAddress("img_plane2", &p_bb_img_plane2[itype]);
  }
  classtrees[kNu]->SetBranchAddress( "mode", &mode );
  classtrees[kNu]->SetBranchAddress( "nuscatter", &nuscatter );
  classtrees[kNu]->SetBranchAddress( "flavor", &flavor );

  int ncosmic_entries = classtrees[kCosmic]->GetEntries();

  // For each event we do a few things: 
  // (1) Calculate mean of all images -- NIXED. We use Caffe's compute image mean utility instead.
  // (2) put data into datum object
  // (3) put label in datum object
  // (4) store that object into an lmdb database

  // Output LMDB
  std::string FLAGS_backend = "lmdb";
  boost::scoped_ptr<caffe::db::DB> db(caffe::db::GetDB(FLAGS_backend));
  db->Open(outdb.c_str(), caffe::db::NEW);
  boost::scoped_ptr<caffe::db::Transaction> txn(db->NewTransaction());

  // convertor
  larbys::util::Root2Image convertor;

  // filter
  larbys::util::EmptyFilter filter( 5.0, 5.0/(224.0*224.0) );

  // Image Protobuf
  caffe::Datum datum;

  // Event Loop

  int num_nufilled = 0;
  int num_bgfilled = 0;
  int numrejected = 0;
  TRandom3 rand( SEED );

  int nu_entry = 0;
  long nu_bytes = classtrees[kNu]->GetEntry( nu_entry );
  
  while ( nu_bytes>0 ) {

    bool passes_plane0 = true;
    bool passes_plane1 = true;
    bool passes_plane2 = filter.passesFilter( *(p_bb_img_plane2[kNu]) );
    if ( kTrinocular ) {
      filter.passesFilter( *(p_bb_img_plane0[kNu]) );
      filter.passesFilter( *(p_bb_img_plane1[kNu]) );
    }

    if ( passes_plane0 && passes_plane1 && passes_plane2 ) {

      BNBLabels_t bnbmode = larbys::util::labelFromRootVars( mode, nuscatter, flavor );

      // extract image as opencv mat
      int height = sqrt( p_bb_img_plane2[kNu]->size() );
      int width = height;
      cv::Mat cv_img_plane2 = convertor.vec2image( *(p_bb_img_plane2[kNu]), height, width );
      if ( cv_img.data2  ) {
	if ( enc.size() ) {
	  // encode the values
	  std::vector<uchar> buf;
	  cv::imencode("."+enc, cv_img, buf );
	  datum.set_data( std::string( reinterpret_cast<char*>(&buf[0]), buf.size() ) );
	  datum.set_label( label );
	  datum.set_encoded(true);
	}
	else {
	  // convert into datum object
	  caffe::CVMatToDatum( cv_img, &datum );
	  datum.set_label( label );
	  datum.set_encoded(false);
	}

      // serialize
      std::string out;
      CHECK( datum.SerializeToString(&out) );
      
      // make key for db entry
      std::string key_str = caffe::format_int(numfilled,10) + "_" + caffe::format_int( label, 2  );
      
      // store in db
      txn->Put( key_str, out );
      numfilled++;
      

      if ( numfilled%100==0 ) {
	txn->Commit();
	txn.reset( db->NewTransaction() );
	//std::cout << "process " << entry << " images" << std::endl;
      }
    }//if data is good
    else{
      if ( cv_img.data && !passes && write_rejected ) {
	numrejected++;
	char rejectname[500];
	sprintf( rejectname, "rejected_by_filter_%05d.jpg", numrejected );
	cv::imwrite( rejectname, cv_img );	
      }
    }

    // update number of entries
    tot_entries_left = 0;
    for ( std::set<int>::iterator it_label=labelset.begin(); it_label!=labelset.end(); it_label++ ) {
      tot_entries_left += entriesleft_per_class[ *it_label ];
    }

    if ( tot_entries_left%1000==0 ) {
      std::cout << "entries left: ";
      for ( std::set<int>::iterator it_label=labelset.begin(); it_label!=labelset.end(); it_label++ ) {
	std::cout << " [" << *it_label <<  "] " << entriesleft_per_class[ *it_label ] << " ";
      }
      std::cout << std::endl;
    }
    
    //if ( numfilled>=10 )
    //break;
  }
  
  // last commit
  txn->Commit();
  txn.reset( db->NewTransaction() );
  
  std::cout << "FIN." << std::endl;
  
  return 0;
}
