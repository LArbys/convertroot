#include <iostream>
#include <sstream>
#include "opencv/cv.h"


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

// executable to test HIP/MIP Filter
#include "HMFilter.h"

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
  larbys::util::HMFilter hmfilter( 768, 768 );
  hmfilter.setMIPbounds( 15, 70 );

  std::string infile = argv[1];
  std::string output_folder = argv[2];
  std::string enc = "";
  int SEED = 123567;
  std::string producer = "bnbcosmics";
  bool fAddPMT = false;
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
  root2datum.convertor.colorscale.setBaseline( 0.0 );
  root2datum.convertor.colorscale.setADC_MIN( 0.0 );
  root2datum.convertor.colorscale.setADC_MAX( 255.0 );
  root2datum.convertor.applyThreshold( true );
  root2datum.convertor.setThreshold( 10.0 );
  root2datum.convertor.setTimePadding( 0 );
  root2datum.convertor.setWirePadding( 0 );
  root2datum.convertor.pmt_colorscale.setADC_MIN( 0.0 );
  root2datum.convertor.pmt_colorscale.setADC_MAX( 1048.0 );
  root2datum.convertor.setPMTimageFormat( larbys::util::Root2Image::kPMTtimescale );

  larbys::util::Datum2Image convertor;
  convertor.setAugment(false);
  convertor.addTPCtrig(false);


  int nentries = tree->GetEntries();

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
    // by default it creates 3 channels, one with each plane
    root2datum.fillDatum( datum );
    datum.set_label( (int)bnblabel );

    // Dump image (w/ PMT)
    bool is_color = true;
    cv::Mat img = convertor.datum2image( datum, is_color, fAddPMT );


    cv::Mat chimg[3];
    cv::Mat chmip[3];
    cv::Mat chhip[3];

    std::cout << "split: " << img.size() << " into " << img.channels() << " channels." << std::endl;

    cv::split( img, chimg );

    for (int p=0; p<3; p++) {
      std::cout << "filter plane " << p << ", img: " << chimg[p].size() << std::endl;
      //chimg[p] *= 2.0;
      hmfilter.applyFilter( chimg[p], chmip[p], chhip[p] );
    }

    // prepare tiled image, beautiful!
    cv::Mat tiled( img.size().height*3+2, img.size().width*3+2, CV_8UC3 );
    tiled = cv::Mat::zeros( img.size().height*3+2, img.size().width*3+2, CV_8UC3 );

    for (int p=0; p<3; p++) {
      for (int h=0; h<img.size().height; h++) {
	for (int w=0; w<img.size().width; w++) {
	  tiled.at< cv::Vec3b >(p*img.size().height + h + p,w)[p] = chimg[p].at< cv::Vec<uchar,1> >(h,w)[0];
	  tiled.at< cv::Vec3b >(p*img.size().height + h + p,img.size().width+1+w)[p] = chmip[p].at< cv::Vec<uchar,1>  >(h,w)[0];
	  tiled.at< cv::Vec3b >(p*img.size().height + h + p,2*img.size().width+2+w)[p] = chhip[p].at< cv::Vec<uchar,1> >(h,w)[0];
	}
      }
    }

    for (int h=0; h<tiled.size().height; h++) {
      for (int i=0; i<3; i++) {
	tiled.at< cv::Vec3b >( h, img.size().width )[i] = 255;
	tiled.at< cv::Vec3b >( h, 2*img.size().width+1 )[i] = 255;
      }
    }

    for (int w=0; w<tiled.size().width; w++) {
      for (int i=0; i<3; i++) {
	tiled.at< cv::Vec3b >( img.size().height, w )[i] = 255;
	tiled.at< cv::Vec3b >( 2*img.size().height+1, w )[i] = 255;
      }
    }

    // make the filename: replace all instances of / and . with _
    std::stringstream fname;
    fname <<  "hmfilter_test_" << entry;

    std::string outpath = output_folder + "/" + fname.str() + ".PNG";
    cv::imwrite( outpath, tiled );

    numfilled++;
    
    if ( numfilled>=50)
      break;
    
    if ( entry%10==0 )
      std::cout << "Entry " << entry << std::endl;

    entry++;
    bytes = tree->GetEntry( entry );
    
    if ( entry>=20 )
      break;
  }
  
  std::cout << "Num Filled: " << numfilled << std::endl;
  std::cout << "FIN." << std::endl;
  
  
  return 0;
}
