#include <iostream>
#include <string>

// Utility to dump out an lmdb archive. A way to check images.
#include "caffe/util/db.hpp"
#include "caffe/util/io.hpp"

// Boost
#include "boost/scoped_ptr.hpp"

// OpenCV
#include "opencv/cv.h"
#include "opencv2/opencv.hpp"

// LArbys
#include "datum2img.h"

int main( int nargs, char** argv ) {

  std::string input_lmdb = argv[1];
  std::string output_folder = argv[2];
  int nprocess = atoi(argv[3]);
  std::string bbannotationfile = "__none__";
  if (nargs==5) {
    bbannotationfile = argv[4];
  }

  std::string FLAGS_backend = "lmdb";
  bool write_images = true;
  bool is_color = true;

  boost::scoped_ptr<caffe::db::DB> db(caffe::db::GetDB(FLAGS_backend));
  db->Open( input_lmdb.c_str(), caffe::db::READ );
  boost::scoped_ptr<caffe::db::Cursor> cursor(db->NewCursor());

  std::ifstream bboxes( bbannotationfile.c_str() );
  cv::Scalar r(100,0,0);
  cv::Scalar g(0,100,0);
  cv::Scalar b(0,0,100);
  cv::Scalar color[3] = { r, g, b};
  
  // Image protobuf
  caffe::Datum datum;
  //cursor->Next();

  // convertor
  larbys::util::Datum2Image convertor;

  int nimages = 0;
  while ( cursor->valid() ) {

    // if ( cursor->key()!="05001_00034_0173600" ) {
    //   cursor->Next();
    //   continue;
    // }
    // else {
    //   std::cout << "Found image" << std::endl;
    // }

    datum.ParseFromString( cursor->value() );
    std::cout << "[ label " << datum.label() << " ] key=" << cursor->key() << " " << datum.height() << " x " << datum.width() << " x " << datum.channels() << std::endl;
    
    // if ( datum.label()==0 || datum.label()>3 ) {
    //   cursor->Next();
    //   continue;
    // }

    
    // look for bounding boxes annotation
    std::vector< std::vector<int> > truth_bboxes;
    if ( bbannotationfile!="__none__" ) {
      bool found = false;
      while ( !bboxes.eof() && !found ) {
	char buffer[1024];
	char buflabel[10];
	bboxes >> buffer >> buflabel;
	std::string bbox_key = buffer;
	if ( std::atoi(buflabel)==1 ) {
	  // if has truth boxes
	  // loop with same keys
	  std::vector<int> box;
	  for ( int i=0; i<12; i++) {
	    bboxes >> buffer;
	    box.push_back( std::atoi( buffer ) );
	  }
	  bboxes >> buffer; // string label which we don't use for now
	  if ( bbox_key==std::string( cursor->key() ) ) {
	    truth_bboxes.push_back( box );
	    found = true;
	  }
	}//else if cosmics
	else {
	  if ( bbox_key==std::string( cursor->key() ) )
	    found = true;
	}
      }//annotation file loop
      std::cout << "found " << truth_bboxes.size() << " boxes for image with key=" << cursor->key() << std::endl;
    }//if annotation file provided
    
    cv::Mat img = convertor.datum2image( datum, is_color );

    // make the filename: replace all instances of / and . with _
    std::string fname = cursor->key();
    size_t loc = fname.find_first_of("/.");
    while ( loc!=std::string::npos ) {
      fname = fname.substr( 0, loc ) + "_" + fname.substr(loc+1,std::string::npos );
      loc = fname.find_first_of("/.");
    }
    std::string outpath = output_folder + "/" + fname + ".PNG";

    if ( truth_bboxes.size()>0 ) {
      for ( auto &bbox : truth_bboxes ) {
	for (int p=0; p<3; p++) {
	  cv::rectangle( img, cv::Point( bbox.at(4*p+1), bbox.at(4*p+0) ), cv::Point( bbox.at(4*p+3), bbox.at(4*p+2) ), color[p] );
	}
      }
    }

    cv::imwrite( outpath, img );

    cursor->Next();
    nimages++;
    if (nprocess>0 && nimages>=nprocess)
      break;
  }

  std::cout << "FIN." << std::endl;
}
