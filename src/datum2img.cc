#include "datum2img.h"
#include <iostream>

namespace larbys {
  namespace util {
    cv::Mat Datum2Image::datum2image( caffe::Datum& datum, bool is_color ) {
      if ( datum.encoded() )
	caffe::DecodeDatum( &datum, is_color );

      cv::Mat img( datum.height(), datum.width(), CV_8UC3 );
      const std::string& data = datum.data();
      std::vector<char> vec_data( data.c_str(), data.c_str()+data.size());
      int height = datum.height();
      int width = datum.width();
      int nchannels = datum.channels();
      for (int h=0; h<height; h++) {
	for (int w=0; w<width; w++) {
	  for (int c=0; c<nchannels; c++) {
	    int index = (c*height + h)*width + w;
	    img.at<cv::Vec3b>( cv::Point(w,h) )[c] = static_cast<unsigned short>( vec_data.at(index) );
	    //std::cout << "(" << h << "," << w << "," << c << ") " << static_cast<unsigned short>( vec_data.at(index) ) << std::endl;
	  }
	}
      }
      return img;
    }
    
    void Datum2Image::datum2TriData( cv::Mat& img, caffe::Datum& datum, bool is_color ) {
      if ( datum.encoded() ) {
	std::cout << "can't decode 9 channel data!!" << std::endl;
	assert(false); // I need to learn how to use exceptions
      }
      
      img.create( datum.height(), datum.width(), CV_8UC(datum.channels()) );
      const std::string& data = datum.data();
      std::vector<char> vec_data( data.c_str(), data.c_str()+data.size());
      int height = datum.height();
      int width = datum.width();
      int nchannels = datum.channels();
      for (int h=0; h<height; h++) {
	for (int w=0; w<width; w++) {
	  for (int c=0; c<nchannels; c++) {
	    int index = (c*height + h)*width + w;
	    img.at< cv::Vec<uchar,9> >( cv::Point(h,w) )[c] = static_cast<unsigned short>( vec_data.at(index) );
	    //std::cout << "(" << h << "," << w << "," << c << ") " << static_cast<unsigned short>( vec_data.at(index) ) << std::endl;
	  }
	}
      }//end of h loop
    }//end of trinocular datum

    void Datum2Image::tridata2image( const cv::Mat& tridata, cv::Mat& image ) {
      int height = tridata.rows;
      int width = tridata.cols;
      int nchannels = tridata.channels();
      image.create( height, width*3+2, CV_8UC3 ); // additional 2 width is for boundary
      for (int h=0; h<height; h++) {
	for (int w=0; w<width; w++) {
	  for (int c=0; c<nchannels; c++) {
	    int ih = h;
	    int iw = (int)( (c/3)*width+w );
	    if ( c/3>0 )
	      iw+=c/3;
	    image.at< cv::Vec3b >( ih, iw )[c%3] = tridata.at< cv::Vec<uchar,9> >(h,w)[c];
	  }
	}
      }//end of h loop
    }//end of trinocular datum      
  }
}
