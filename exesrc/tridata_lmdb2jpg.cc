#include <iostream>
#include <string>

// Utility to dump out an lmdb archive. A way to check images.
// Program assumes trinocular data (3 plane data)

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

  std::string FLAGS_backend = "lmdb";
  bool write_images = true;
  bool is_color = true;

  boost::scoped_ptr<caffe::db::DB> db(caffe::db::GetDB(FLAGS_backend));
  db->Open( input_lmdb.c_str(), caffe::db::READ );
  boost::scoped_ptr<caffe::db::Cursor> cursor(db->NewCursor());
  
  // Image protobuf
  caffe::Datum datum;
  //cursor->Next();

  // convertor
  larbys::util::Datum2Image convertor;

  int nimages = 0;
  while ( cursor->valid() ) {

    datum.ParseFromString( cursor->value() );
    std::cout << "[ label " << datum.label() << " ] key=" << cursor->key() << " " << datum.height() << " x " << datum.width() << " x " << datum.channels() << std::endl;
    
    if ( datum.label()==0 || datum.label()>3 ) {
      cursor->Next();
      continue;
    }

    cv::Mat tri_img;
    convertor.datum2TriData( tri_img, datum, is_color );
    //std::cout << tri_img << std::endl;
    cv::Mat img;
    convertor.tridata2image( tri_img, img );

    // make the filename: replace all instances of / and . with _
    std::string fname = cursor->key();
    size_t loc = fname.find_first_of("/.");
    while ( loc!=std::string::npos ) {
      fname = fname.substr( 0, loc ) + "_" + fname.substr(loc+1,std::string::npos );
      loc = fname.find_first_of("/.");
    }
    std::string outpath = output_folder + "/" + fname + ".JPEG";

    if ( write_images )
      cv::imwrite( outpath, img );

    cursor->Next();
    nimages++;
    if (nprocess>0 && nimages>=nprocess)
      break;
  }


}
