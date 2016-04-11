
#include "PMTWireWeights.h"
#include <iostream>
#include "opencv/cv.h"


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
#include "root2datum.h"
#include "emptyfilter.h"
#include "BNBLabels.h"
#include "datum2img.h"

// ROOT
#include "TFile.h"
#include "TChain.h"
#include "TRandom3.h"

// executable to test PMT charge wire weighting code


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


int main( int nargs, char** argv ) {

  // Wire Weights Code
  larbys::util::PMTWireWeights weight;

  cv::Mat* outsize[3];
  for (int p=0; p<3; p++) {
    outsize[p] = new cv::Mat(768, 32, CV_32F );
    cv::resize( weight.planeWeights[p], *(outsize[p]), outsize[p]->size(), CV_INTER_LINEAR );
  }

  std::string infile = argv[1];
  std::string outdb = argv[2];
  std::string enc = "";
  int SEED = 123567;
  bool kTrinocular = false; // fold in all three views into data
  std::string producer = "bnbcosmics";
  bool fAddPMT = true;
  int resize_factor = 1;

  // read input files
  std::vector< std::string > inputlist;
  parse_inputlist( infile, inputlist );

  // load trees, setup branches and count number of entries
  // type of tree used for neutrinos
  TChain* tree;
  tree = new TChain( std::string( producer+"/imgtree" ).c_str() );
  
  for ( int ifile=0; ifile<inputlist.size(); ifile++ )
    tree->Add( inputlist.at(ifile).c_str() );

  // root vars
  int run, subrun, event;
  tree->SetBranchAddress( "run", &run );
  tree->SetBranchAddress( "subrun", &subrun );
  tree->SetBranchAddress( "event", &event );

  std::cout << "Setup Converter...";
  larbys::util::Root2Datum root2datum( tree, larbys::util::Root2Datum::kTrinocular, larbys::util::Root2Datum::kGreyScale, fAddPMT );
  root2datum.colorscale.setADC_MIN( 0.0 );
  root2datum.colorscale.setADC_MAX( 255.0 );
  root2datum.convertor.setTimePadding( 10 );
  root2datum.convertor.setWirePadding( 0 );
  root2datum.convertor.pmt_colorscale.setADC_MIN( 0.0 );
  root2datum.convertor.pmt_colorscale.setADC_MAX( 1048.0 );
  root2datum.convertor.setPMTimageFormat( larbys::util::Root2Image::kPMTtimescale );
  larbys::util::Datum2Image datum2img;

  int nentries = tree->GetEntries();
  std::cout << "done." << std::endl;

  std::cout << "Number of entries: " << nentries << std::endl;

  // Image Protobuf
  caffe::Datum datum;


  std::cout << "START LOOP" << std::endl;

  unsigned long entry = 0;
  unsigned long bytes = tree->GetEntry( entry );
  int numfilled = 0;
  
  while ( bytes>0 ) {

    // set label to background
    larbys::util::BNBLabels_t bnblabel = larbys::util::kBackground; // default to background

    // extract image as opencv mat
    root2datum.fillDatum( datum );

    // sum pmt charge in the trigger window
    //std::vector<float> totalq( 32, 0.0 );
    cv::Mat pmtq( 32, 1, CV_32F );
    float totalq = 0.0;
    for (int ipmt=0; ipmt<32; ipmt++) {
      float high_q = 0.0;
      float low_q  = 0.0;
      bool usehigh = true;
      for (int t=190; t<310; t++) {
	// sum over the trigger window
	high_q += root2datum.p_pmt_highgain->at( 768*ipmt + t );
	low_q  += root2datum.p_pmt_lowgain->at( 768*ipmt + t );
	if ( root2datum.p_pmt_highgain->at( 768*ipmt + t )>1040 )
	  usehigh = false;
      }
      if ( high_q > 0.0 ) {
	if ( usehigh ) {
	  pmtq.at<float>(ipmt,1) = high_q/100.0;
	totalq += high_q/100.0;
	}
	else {
	  pmtq.at<float>(ipmt,1) = 10.0*low_q/100.0;
	  totalq += low_q/100.0;
	}
      }
    }
    
    // normalize charge
    if ( totalq > 0 ) {
      for (int ipmt=0; ipmt<32; ipmt++) {
	pmtq.at<float>(ipmt,1) /= totalq;
      }
    }
    
    cv::Mat wireweights[3];
    for (int p=0; p<3; p++) {
      wireweights[p] = (*outsize[p])*pmtq;
    }
    
    std::cout << "ww: " << wireweights[2] << std::endl;

    datum.set_label( (int)bnblabel );

    numfilled++;
    
    if ( numfilled>=50)
      break;
    
    if ( entry%10==0 )
      std::cout << "Entry " << entry << std::endl;

    entry++;
    bytes = tree->GetEntry( entry );
    
    if ( entry>=0 )
      break;
  }
  
  std::cout << "Num Filled: " << numfilled << std::endl;
  std::cout << "FIN." << std::endl;
  
  
  return 0;
}
