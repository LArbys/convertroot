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


void parselist( std::string fname, std::vector<std::string>& vec ) {
  std::ifstream f( fname.c_str() );

  while ( !f.eof() && f.good() ) {
    char buffer[2054];
    f >> buffer;
    std::string sbuf = buffer;
    if (sbuf!="") {
      if ( vec.size()==0 || vec.at(vec.size()-1)!=std::string(sbuf) )
	vec.push_back( sbuf );
    }
  }
  f.close();
}

int main( int nargs, char** argv ) {

  std::string input_lmdb = argv[1];
  std::string output_folder = argv[2];
  int nprocess = atoi(argv[3]);
  std::string utility_file = "__none__";
  if (nargs==5) {
    utility_file = argv[4];
  }

  std::string FLAGS_backend = "lmdb";
  bool write_images = true;
  bool is_color = true;
  bool has_pmt =false;
  bool util_drawboxes = false;
  bool util_makecut = true;
  int timepad = 20;

  boost::scoped_ptr<caffe::db::DB> db(caffe::db::GetDB(FLAGS_backend));
  db->Open( input_lmdb.c_str(), caffe::db::READ );
  boost::scoped_ptr<caffe::db::Cursor> cursor(db->NewCursor());

  std::ifstream bboxes( utility_file.c_str() );
  cv::Scalar r(100,0,0);
  cv::Scalar g(0,100,0);
  cv::Scalar b(0,0,100);
  cv::Scalar color[3] = { r, g, b};

  std::vector<std::string> keylist;
  if ( util_makecut && nargs==5 ) {
    parselist( utility_file, keylist );
  }
  else {
    util_makecut = false;
  }
  
  // Image protobuf
  caffe::Datum datum;

  // convertor
  larbys::util::Datum2Image convertor;
  convertor.setAugment(true);
  convertor.addTPCtrig(true);

  int nimages = 0;
  while ( cursor->valid() ) {

    if ( util_makecut ) {
      bool foundit = false;
      for ( std::vector<std::string>::iterator it=keylist.begin(); it!=keylist.end(); it++ )  {
	if ( cursor->key()==(*it) ) {
	  foundit = true;
	  break;
	}
      }
      if ( !foundit ) {
	cursor->Next();
	continue;
      }
    }
    else {
      std::cout << "Found image" << std::endl;
    }

    datum.ParseFromString( cursor->value() );
    std::cout << "[ label " << datum.label() << "] key=" << cursor->key() << " " << datum.width() << " x " << datum.height() << " x " << datum.channels() << std::endl;

    cv::Mat totimg( datum.height(), datum.width(), CV_8UC(12) );
    totimg = cv::Mat::zeros( datum.height(), datum.width(), CV_8UC(12) );
    const std::string& data = datum.data();
    std::vector<char> vec_data( data.c_str(), data.c_str()+data.size());
    int height = datum.height();
    int width = datum.width();
    int nchannels = datum.channels();
    
    // tpc layers
    for (int c=0; c<nchannels; c++) { 
      for (int h=0; h<height; h++) {
	for (int w=0; w<width; w++) {
	  int index = (c*height + h)*width + w;
	  unsigned int val = static_cast<unsigned short>( vec_data.at(index) );
	  totimg.at< cv::Vec<uchar,12> >( h, w )[c] = val;
	}
      }
    }
    
    std::cout << "decoded image: " << totimg.size() << " x " << totimg.channels() << std::endl;

    // we know what we are drawing here, so don't use the datum2img tool
    cv::Mat tiled( datum.height()*3+2, datum.width()*4+3, CV_8UC3 );
    tiled = cv::Mat::zeros( datum.height()*3+2, datum.width()*4+3, CV_8UC3 );
    for (int p=0; p<3; p++) {
      for (int h=0; h<height; h++) {
	for (int w=0; w<width; w++) {
	  int multi = 5;
	  tiled.at< cv::Vec3b >(p*height + p + h, w)[p]           = std::min(255,multi*totimg.at< cv::Vec<uchar,12> >( h, w )[4*p+0]);    // original image
	  tiled.at< cv::Vec3b >(p*height + p + h, 1*width+1+w)[p] = std::min(255,2*multi*totimg.at< cv::Vec<uchar,12> >( h, w )[4*p+1]);  // pmt weight
	  tiled.at< cv::Vec3b >(p*height + p + h, 2*width+2+w)[p] = std::min(255,multi*totimg.at< cv::Vec<uchar,12> >( h, w )[4*p+2]);    // MIP range
	  tiled.at< cv::Vec3b >(p*height + p + h, 3*width+3+w)[p] = std::min(255,multi*totimg.at< cv::Vec<uchar,12> >( h, w )[4*p+3]);    // HIP range
	}
      }
    }

    // grids
    for (int p=0; p<3; p++) {
      for (int h=0; h<height*3+2; h++) {
	for (int c=0; c<3; c++)
	  tiled.at< cv::Vec3b >(h, (p+1)*width+p)[c] = 255;
      }
    }

    for (int p=0; p<2; p++) {
      for (int w=0; w<width*4+3; w++) {
	for (int c=0; c<3; c++)
	  tiled.at< cv::Vec3b >((p+1)*height+p, w)[c] = 255;
      }
    }
    
    // look for bounding boxes annotation
    std::vector< std::vector<int> > truth_bboxes;
    if ( util_drawboxes && utility_file!="__none__" ) {
      bool found = false;
      while ( !bboxes.eof() && !found ) {
    	char buffer[1024];
    	char buflabel[10];
    	bboxes >> buffer >> buflabel;
    	std::string bbox_key = buffer;
    	if ( std::atoi(buflabel)!=0 ) {
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
    
    // cv::Mat img = convertor.datum2image( datum, is_color, has_pmt );

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
	  for (int c=0; c<4; c++) {
	    cv::rectangle( tiled, 
			   cv::Point( bbox.at(4*p+1)+(c*width+c), bbox.at(4*p+0)+timepad+(p*height+p) ), 
			   cv::Point( bbox.at(4*p+3)+(c*width+c), bbox.at(4*p+2)+timepad+(p*height+p) ), 
			   color[p] );
	  }
    	}
      }
    }
    
    cv::imwrite( outpath, tiled );
    
    cursor->Next();
    nimages++;
    if (nprocess>0 && nimages>=nprocess)
      break;
  }

  std::cout << "FIN." << std::endl;
}
