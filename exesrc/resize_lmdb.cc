#include <iostream>
#include <string>

// This utility exists to split a sample into 
// a train and test set.
// we throw using a random number generator. 
// this way we can create many different cross-validation sets.

// Caffe and LMDB
#include "lmdb.h"
#include "caffe/common.hpp"
#include "caffe/util/db.hpp"
#include "caffe/proto/caffe.pb.h"
#include "caffe/util/format.hpp"
#include "caffe/util/db.hpp"
#include "caffe/util/io.hpp"

// OpenCV
#include "opencv/cv.h"
#include "opencv2/opencv.hpp"

// Boost
#include "boost/scoped_ptr.hpp"

// LArbys
#include "datum2img.h"

int main( int narg, char** argv ) {

  std::string input_lmdb = argv[1];
  std::string output_lmdb = argv[2];

  int resize_factor=2;

  // source database
  std::string FLAGS_backend = "lmdb";
  boost::scoped_ptr<caffe::db::DB> db_src(caffe::db::GetDB(FLAGS_backend));
  db_src->Open(input_lmdb.c_str(), caffe::db::READ);
  boost::scoped_ptr<caffe::db::Cursor> cursor_src(db_src->NewCursor());

  // output train database
  boost::scoped_ptr<caffe::db::DB> db_output(caffe::db::GetDB(FLAGS_backend));
  db_output->Open(output_lmdb.c_str(), caffe::db::NEW);
  boost::scoped_ptr<caffe::db::Transaction> txn_output(db_output->NewTransaction());

  larbys::util::Datum2Image datum2img;

  int nfilled = 0;

  while ( cursor_src->valid() ) {
    // unpack datam
    caffe::Datum input_datum;
    input_datum.ParseFromString(cursor_src->value());
    cv::Mat input_img = datum2img.datum2image( input_datum, true );
    int newheight = input_datum.height()/resize_factor;
    int newwidth = input_datum.width()/resize_factor;
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
	  output_img.at<cv::Vec3b>( cv::Point(w,h) )[c] = maxval;// weird, had to reverse it
	}
      }
    }//end of channel loop
    
    // make back into datum
    caffe::Datum output_datum;
    caffe::CVMatToDatum( output_img, &output_datum );
    output_datum.set_label( input_datum.label() );
    output_datum.set_encoded(false);
    std::string out;
    CHECK( output_datum.SerializeToString(&out) );
    txn_output->Put( cursor_src->key(), out );
    nfilled++;
    if (nfilled>0 && nfilled%100==0) {
      txn_output->Commit();
      txn_output.reset( db_output->NewTransaction() );
      std::cout << "nfilled=" << nfilled << std::endl;
    }
    cursor_src->Next();
  }

  txn_output->Commit();
  txn_output.reset(db_output->NewTransaction() );
  std::cout << "FIN." << std::endl;

}
