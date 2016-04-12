#include "PMTWireWeights.h"
#include <iostream>
#include <sstream>
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
  //larbys::util::PMTWireWeights weight;
  larbys::util::PMTWireWeights weight( 768*5 );

  cv::Mat* outsize[3];
  for (int p=0; p<3; p++) {
    outsize[p] = new cv::Mat(768, 32, CV_32F );
    cv::resize( weight.planeWeights[p], *(outsize[p]), outsize[p]->size(), CV_INTER_LINEAR );
    //std::cout << (*outsize[p]) << std::endl;
    //std::cin.get();
  }

  std::string infile = argv[1];
  std::string output_folder = argv[2];
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
  root2datum.convertor.colorscale.setADC_MIN( 0.0 );
  root2datum.convertor.colorscale.setADC_MAX( 255.0 );
  root2datum.convertor.setTimePadding( 10 );
  root2datum.convertor.setWirePadding( 0 );
  root2datum.convertor.pmt_colorscale.setADC_MIN( 0.0 );
  root2datum.convertor.pmt_colorscale.setADC_MAX( 1048.0 );
  root2datum.convertor.setPMTimageFormat( larbys::util::Root2Image::kPMTtimescale );

  larbys::util::Datum2Image convertor;
  convertor.setAugment(false);
  convertor.addTPCtrig(true);


  int nentries = tree->GetEntries();
  std::cout << "done." << std::endl;

  std::cout << "Number of entries: " << nentries << std::endl;

  // Image Protobuf
  caffe::Datum datum;


  std::cout << "START LOOP" << std::endl;

  unsigned long entry = 1;
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
    pmtq = cv::Mat::zeros( 32, 1, CV_32F );
    float totalq = 0.0;
    for (int ipmt=0; ipmt<32; ipmt++) {
      float high_q = 0.0;
      float low_q  = 0.0;
      bool usehigh = true;
      
      for (int t=190; t<320; t++) {
	// sum over the trigger window
	//(float)(vec->at( (h)*width + (width-1-w) )-BASELINE);  // pmt 
	//int index = t*768 + (768-1-ipmt*int(768/32));
	int index = t*768 + ipmt*int(768/32);
	float highadc = root2datum.p_pmt_highgain->at( index );
	float lowadc  = root2datum.p_pmt_lowgain->at( index );
	if ( highadc<30 ) { // no single pe hits
	  highadc = 0;
	  lowadc = 0;
	}
	//std::cout << "(" << ipmt << "," << t << "): " << highadc << std::endl;
	high_q += highadc;
	low_q  += lowadc;
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
	  totalq += 10.0*low_q/100.0;
	}
      }
    }

    
    // normalize charge
    if ( totalq > 0 ) {
      for (int ipmt=0; ipmt<32; ipmt++) {
	pmtq.at<float>(ipmt,1) /= totalq;
      }
    }

    std::cout << "TOTAL PMTQ: " << totalq << std::endl;
    //std::cout << "PMTQ: " << pmtq << std::endl;
    //std::cin.get();

    
    cv::Mat wireweights[3];
    for (int p=0; p<3; p++) {
      wireweights[p] = (*outsize[p])*pmtq;

      //std::cout << "W*P = " << (*outsize[p]).size() << " " << pmtq.size() << std::endl;
      //std::cout << "ww: " << wireweights[p].size() << std::endl;
      //std::cout << wireweights[p] << std::endl;
      //std::cin.get();
    }

    datum.set_label( (int)bnblabel );


    // Dump image (w/ PMT)
    bool is_color = true;
    cv::Mat img = convertor.datum2image( datum, is_color, fAddPMT );

    // make the filename: replace all instances of / and . with _
    std::stringstream fname;
    fname <<  "weightweight_test_" << entry;

    std::string outpath = output_folder + "/" + fname.str() + "_unweighted.PNG";
    cv::imwrite( outpath, img );

    img = convertor.datum2image( datum, is_color, false ); // no pmt this time

    // weight the image: draw onto tiled image
    cv::Mat tiled( datum.height(), datum.width()*3+2, CV_8UC3 );
    tiled = cv::Mat::zeros( datum.height(), datum.width()*3+2, CV_8UC3 );

    int chplane[3] = { 0, 1, 2 };
    for (int c=0; c<3; c++) {
      int height = datum.height();
      int width = datum.width();
      for (int h=0; h<height; h++) {
    	for (int w=0; w<width; w++) {
    	  int val = img.at<cv::Vec3b>( cv::Point(w,h) )[c] * wireweights[chplane[c]].at<float>( 1, w );
	  if ( c>=0 ) {
	    // augment
	    val = std::min( 255, val*5 );
	    if ( c==0 )
	      val = std::min( 255, val*5 );
	  }
	  tiled.at<cv::Vec3b>( h, c*datum.width() + c + w )[c] = val;
    	}
      }
    }

    // fill in boundaries
    for (int h=0; h<datum.height(); h++) {
      for (int i=0; i<3; i++) {
	tiled.at<cv::Vec3b>( h, datum.width() )[i] = 255;
	tiled.at<cv::Vec3b>( h, datum.width()*2 + 1 )[i] = 255;
      }
    }
    
    outpath = output_folder + "/" + fname.str() + "_pmtweighted.PNG";
    cv::imwrite( outpath, tiled );

    numfilled++;
    
    if ( numfilled>=50)
      break;
    
    if ( entry%10==0 )
      std::cout << "Entry " << entry << std::endl;

    entry++;
    bytes = tree->GetEntry( entry );
    
    if ( entry>=101 )
      break;
  }
  
  std::cout << "Num Filled: " << numfilled << std::endl;
  std::cout << "FIN." << std::endl;
  
  
  return 0;
}
