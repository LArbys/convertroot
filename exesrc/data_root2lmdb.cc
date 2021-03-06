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
#include "root2datum.h"
#include "emptyfilter.h"
#include "BNBLabels.h"
#include "datum2img.h"

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

  std::string infile = argv[1];
  std::string outdb = argv[2];
  std::string enc = "";
  int SEED = 123567;
  bool kTrinocular = false; // fold in all three views into data
  NuImageType_t fNuImageType = kEventImage;
  std::string producer = "bnbcosmics";
  bool fAddPMT = true;
  int resize_factor = 1;

  // read input files
  std::vector< std::string > inputlist;
  parse_inputlist( infile, inputlist );
  
  // load trees, setup branches and count number of entries
  // type of tree used for neutrinos
  TChain* tree;
  if ( fNuImageType==kEventImage ) { 
    std::cout << "use whole view image" << std::endl;
    tree = new TChain( std::string( producer+"/imgtree" ).c_str() );
  }
  else {
    std::cout << "use cropped interaction image" << std::endl;
    tree = new TChain( "yolo/bbtree" );
  }
  for ( int ifile=0; ifile<inputlist.size(); ifile++ )
    tree->Add( inputlist.at(ifile).c_str() );

  // root vars
  int run, subrun, event;
  tree->SetBranchAddress( "run", &run );
  tree->SetBranchAddress( "subrun", &subrun );
  tree->SetBranchAddress( "event", &event );

  std::cout << "Setup Converter...";
  larbys::util::Root2Datum root2datum( tree, larbys::util::Root2Datum::kTrinocular, larbys::util::Root2Datum::kGreyScale, fAddPMT );
  root2datum.convertor.colorscale.setADC_MIN( 0.0 );
  root2datum.convertor.colorscale.setADC_MAX( 255.0 );
  root2datum.convertor.setTimePadding( 10 );
  root2datum.convertor.setWirePadding( 0 );
  root2datum.convertor.applyThreshold( true );
  root2datum.convertor.setThreshold( 10.0 );
  root2datum.convertor.applyScaling( true );
  root2datum.convertor.setScaling( 1.5 );
  root2datum.convertor.pmt_colorscale.setADC_MIN( 0.0 );
  root2datum.convertor.pmt_colorscale.setADC_MAX( 255.0 );
  root2datum.convertor.setPMTimageFormat( larbys::util::Root2Image::kPMTtimescale );
  larbys::util::Datum2Image datum2img;

  int nentries = tree->GetEntries();
  std::cout << "done." << std::endl;

  std::cout << "Number of entries: " << nentries << std::endl;


  // Output LMDB
  std::string FLAGS_backend = "lmdb";
  boost::scoped_ptr<caffe::db::DB> db(caffe::db::GetDB(FLAGS_backend));
  db->Open(outdb.c_str(), caffe::db::NEW);
  boost::scoped_ptr<caffe::db::Transaction> txn(db->NewTransaction());

  // filter
  //larbys::util::EmptyFilter filter( 10.0, 10.0/(448.0*448.0) );

  // Image Protobuf
  caffe::Datum datum;

  // Event Loop

  //int numrejected = 0;


  std::cout << "START LOOP" << std::endl;

  unsigned long entry = 0;
  unsigned long bytes = tree->GetEntry( entry );
  int numfilled = 0;
  
  while ( bytes>0 ) {

    // set label to background
    larbys::util::BNBLabels_t bnblabel = larbys::util::kBackground; // default to background

    // extract image as opencv mat
    root2datum.fillDatum( datum );
    datum.set_label( (int)bnblabel );

    std::string out;
    if (resize_factor>1 ) {
      // resize image
      cv::Mat input_img = datum2img.datum2image( datum, true );
      int newheight = datum.height()/resize_factor;
      int newwidth = datum.width()/resize_factor;
      cv::Mat output_img( newheight, newwidth, CV_8UC3 );
      for (int c=0; c<3; c++) {
	for (int h=0; h<newheight; h++) {
	  for (int w=0; w<newwidth; w++) {
	    unsigned int maxval = 0;
	    for (int h2=0; h2<resize_factor; h2++) {
	      for (int w2=0; w2<resize_factor; w2++) {
		if ( maxval < input_img.at<cv::Vec3b>( cv::Point(h*resize_factor+h2,w*resize_factor+w2 ) )[c] )
		  maxval = input_img.at<cv::Vec3b>( cv::Point(h*resize_factor +h2,w*resize_factor+w2 ) )[c];
	      }
	    }
	    output_img.at<cv::Vec3b>( cv::Point(h,w) )[c] = maxval;// weird, had to reverse it
	  }
	}
      }//end of channel loop
      caffe::Datum output_datum;
      caffe::CVMatToDatum( output_img, &output_datum );
      output_datum.set_label( datum.label() );
      output_datum.set_encoded(false);
      CHECK( output_datum.SerializeToString(&out) );
    }
    else {
      // serialize
      CHECK( datum.SerializeToString(&out) );
    }
      
    // make key for db entry
    std::string key_str = caffe::format_int(run,5) + "_" + caffe::format_int(subrun,5) + "_" + caffe::format_int(event,5) + caffe::format_int( (int)bnblabel, 2  );
      
    // store in db
    txn->Put( key_str, out );
    numfilled++;
    
    if ( numfilled>0 && numfilled%100==0 ) {
      txn->Commit();
      txn.reset( db->NewTransaction() );
    }
    
    // if ( numfilled>=50)
    //   break;
    
    if ( entry%10==0 )
      std::cout << "Entry " << entry << std::endl;

    entry++;
    bytes = tree->GetEntry( entry );
    
    if ( entry>=500 )
      break;
  }
  
  // last commit
  txn->Commit();
  txn.reset( db->NewTransaction() );

  std::cout << "Num Filled: " << numfilled << std::endl;
  std::cout << "FIN." << std::endl;
  
  return 0;
}
