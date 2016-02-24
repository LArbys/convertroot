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
// (3) We split the cosmic samples listed in half.  First half is used as background sample. Second half is used to overlay with BNB neutrino.


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
  typedef enum { kEventImage=0, kInteractionImage } NuImageType_t;
  // kCropBG, folds in a background sample consisting of H x H crops of cosmic MC images (default H=224)
  // kOverlayBG, overlays random background with neutrino event.  Folds in examples with no BNB neutrino as well. (default image 448 x 448 )
  //   for overlay mode, we can either combine by choosing the max value between neutrino and cosmic
  //   or we combine by summing pixel values

  std::string infile_neutrino = argv[1];
  std::string infile_cosmics = argv[2]; // future
  std::string outdb = argv[3];
  std::string enc = "";
  int SEED = 123567;
  bool write_rejected = true; // save rejected neutrino events (ones with too few pixels below threshold)
  bool kTrinocular = true; // fold in all three views into data
  float kSigtoBGratio = 1.0; // 1-1 BG to 
  NuImageType_t fNuImageType = kEventImage;
  bool fOverlay = true;
  float fEnergyCut = 0.200;

  typedef enum { kNu=0, kCosmic, NTYPES } FileTypes_t;

  // read input files
  std::vector< std::string > inputlist[2];
  parse_inputlist( infile_neutrino, inputlist[kNu] );
  parse_inputlist( infile_cosmics, inputlist[kCosmic] );
  
  // load trees, setup branches and count number of entries
  // type of tree used for neutrinos
  TChain** classtrees = new TChain*[NTYPES];
  if ( fNuImageType==kEventImage ) {
    std::cout << "use event image" << std::endl;
    classtrees[kNu] = new TChain( "yolo/imgtree" );
  }
  else {
    std::cout << "use cropped interaction image" << std::endl;
    classtrees[kNu] = new TChain( "yolo/bbtree" );
  }
  // cosmics always use the imgtree
  classtrees[kCosmic] = new TChain( "yolo/imgtree" );

  std::vector<int>** p_bb_img_plane0 = new std::vector<int>*[NTYPES];
  std::vector<int>** p_bb_img_plane1 = new std::vector<int>*[NTYPES];
  std::vector<int>** p_bb_img_plane2 = new std::vector<int>*[NTYPES];
  int mode, nuscatter, flavor;
  float Enu;
  for (int itype=0; itype<NTYPES; itype++) {
    // initialize pointer and then set branch address
    p_bb_img_plane0[itype] = 0;
    p_bb_img_plane1[itype] = 0;
    p_bb_img_plane2[itype] = 0;
    // we could loop, but for now assume number of entries = nfiles*50;
    for ( std::vector<std::string>::iterator it_file=inputlist[itype].begin(); it_file!=inputlist[itype].end(); it_file++ )
      classtrees[itype]->Add( (*it_file).c_str() );
    if ( kTrinocular ) {
      // load induction planes
      classtrees[itype]->SetBranchAddress("img_plane0", &p_bb_img_plane0[itype]);
      classtrees[itype]->SetBranchAddress("img_plane1", &p_bb_img_plane1[itype]);
    }
    // always load the collection plane
    classtrees[itype]->SetBranchAddress("img_plane2", &p_bb_img_plane2[itype]);
  }
  // neutrino tree specific variables
  classtrees[kNu]->SetBranchAddress( "mode", &mode );
  classtrees[kNu]->SetBranchAddress( "nuscatter", &nuscatter );
  classtrees[kNu]->SetBranchAddress( "flavor", &flavor );
  classtrees[kNu]->SetBranchAddress( "Enu", &Enu );

  int ncosmic_tot_entries = classtrees[kCosmic]->GetEntries();
  int nbnb_entries = classtrees[kNu]->GetEntries();
  int ncosmic_entries = ncosmic_tot_entries/2;
  if ( nbnb_entries > ncosmic_tot_entries/2 ) {
    nbnb_entries = ncosmic_tot_entries/2; // if we have less cosmics than neutrinos, we have to limit
  }
  else {
    ncosmic_entries = nbnb_entries;
  }
    
  std::cout << "number of BNB entries: " << nbnb_entries << std::endl;
  std::cout << "number of cosmics entries: " << ncosmic_entries << std::endl;
  std::cout << "number of TOTAL cosmic entries: " << ncosmic_tot_entries << std::endl;

  // For each event we do a few things: 
  

  // Output LMDB
  std::string FLAGS_backend = "lmdb";
  boost::scoped_ptr<caffe::db::DB> db(caffe::db::GetDB(FLAGS_backend));
  db->Open(outdb.c_str(), caffe::db::NEW);
  boost::scoped_ptr<caffe::db::Transaction> txn(db->NewTransaction());

  // convertor
  larbys::util::Root2Image convertor;

  // filter
  larbys::util::EmptyFilter filter( 10.0, 10.0/(448.0*448.0) );

  // Image Protobuf
  caffe::Datum datum;

  // Event Loop

  int numrejected = 0;
  TRandom3 rand( SEED );

  int entry[NTYPES];
  int filled[NTYPES];
  long bytes[NTYPES];
  for (int itype=0; itype<NTYPES; itype++ ) {
    entry[itype] = 0;
    filled[itype] = 0;
  }

  std::cout << "START LOOP" << std::endl;
  

  while ( entry[kNu] < nbnb_entries || entry[kCosmic] < ncosmic_entries ) {

    FileTypes_t ftype = kNu;

    // are we filling a cosmic or neutrino event?
    // choose based on how many left
    int nu_remaining = nbnb_entries - entry[kNu];
    int bg_remaining = ncosmic_entries - entry[kCosmic];
    //if ( nu_remaining < (nu_remaining+bg_remaining)*rand.Uniform() ) {
    if ( filled[kCosmic]<filled[kNu] ) {
      ftype = kCosmic;
      // select the cosmic event (we throw)
      classtrees[kCosmic]->GetEntry(entry[kCosmic]);
      entry[kCosmic]++;
    }
    else {
      classtrees[kNu]->GetEntry( entry[kNu] );
      classtrees[kCosmic]->GetEntry(entry[kNu] + ncosmic_entries ); // use back half for cosmic overlay
      entry[kNu]++;
    }

    // set the branch variables we care about
    std::vector<int>* p_plane0 = p_bb_img_plane0[ftype];
    std::vector<int>* p_plane1 = p_bb_img_plane1[ftype];
    std::vector<int>* p_plane2 = p_bb_img_plane2[ftype];

    // filter out images with little charge
    bool passes_plane0 = true;
    bool passes_plane1 = true;
    bool passes_plane2 = filter.passesTotalFilter( *(p_plane2), 20*100 );
    if ( kTrinocular ) {
//       passes_plane0 = filter.passesFilter( *(p_plane0) );
//       passes_plane1 = filter.passesFilter( *(p_plane1) );
      passes_plane0 = filter.passesTotalFilter( *(p_plane0), 20*100 );
      passes_plane0 = filter.passesTotalFilter( *(p_plane1), 20*100 );
    }

    if ( passes_plane0 && passes_plane1 && passes_plane2 && (ftype==kCosmic || Enu>fEnergyCut) ) {
      // passes filter

      // get the label
      larbys::util::BNBLabels_t bnblabel = larbys::util::kBackground; // default to background
      if ( ftype==kNu )
	bnblabel = larbys::util::labelFromRootVars( mode, nuscatter, flavor ); // if neutrino, we label it

      if ( ftype==kCosmic )
	std::cout << "Make a cosmic event (" << entry[kCosmic]-1 << ")" << std::endl;
      else
	std::cout << "Make a neutrino " << getBNBLabelName( bnblabel ) << " event (entry " << entry[kNu]-1 << ", cosmic entry " << entry[kNu]-1 + ncosmic_entries << ")" << std::endl;	
      
      // extract image as opencv mat
      int height = sqrt( p_plane2->size() );
      int width = height;
      int nchannels = 3;
      if ( kTrinocular ) nchannels = 9;
      cv::Mat cv_img; // probably should allocate this once above and just refill
      if ( kTrinocular ) {
	if ( fOverlay && ftype==kNu ) {
	  for (int i=0; i<(*p_plane2).size(); i++) {
	    (*p_plane0).at(i) += (*p_bb_img_plane0[kCosmic]).at(i);
	    (*p_plane1).at(i) += (*p_bb_img_plane1[kCosmic]).at(i);
	    (*p_plane2).at(i) += (*p_bb_img_plane2[kCosmic]).at(i);
	  }
	}
	convertor.vec2image( cv_img, *p_plane0, *p_plane1, *p_plane2, height, width ); // collection plane
      }
      else {
	if ( fOverlay && ftype==kNu ) { //neutrino, we need to overlay the cosmic first before making the image
	  for (int i=0; i<(*p_plane2).size(); i++)
	    (*p_plane2).at(i) += (*p_bb_img_plane2[kCosmic]).at(i);
	}
	convertor.vec2image( cv_img, *p_plane2, height, width ); // collection plane
      }

      if ( cv_img.data ) {
	if ( enc.size() ) {
	  // encode the values into the datum
	  std::vector<uchar> buf;
	  cv::imencode("."+enc, cv_img, buf );
	  datum.set_data( std::string( reinterpret_cast<char*>(&buf[0]), buf.size() ) );
	  datum.set_encoded(true);
	}
	else {
	  // do straightforward conversion into datum object
	  caffe::CVMatToDatum( cv_img, &datum );
	  datum.set_encoded(false);
	}
	datum.set_label( (int)bnblabel );
      }// if data ok

      // serialize
      std::string out;
      CHECK( datum.SerializeToString(&out) );
      
      // make key for db entry
      int numfilled = filled[kNu]+filled[kCosmic];
      std::string key_str = caffe::format_int(numfilled,10) + "_" + caffe::format_int( (int)bnblabel, 2  );
      
      // store in db
      txn->Put( key_str, out );
      filled[ftype]++;
      

      if ( numfilled>0 && numfilled%100==0 ) {
	txn->Commit();
	txn.reset( db->NewTransaction() );
	//std::cout << "process " << entry << " images" << std::endl;
      }
    }//if passes filter
    
    if ( filled[kNu] + filled[kCosmic] >=20 )
      break;
  }
  
  // last commit
  txn->Commit();
  txn.reset( db->NewTransaction() );
  
  std::cout << "FIN." << std::endl;
  
  return 0;
}
