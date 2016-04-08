#include "datum2img.h"
#include <iostream>
#include <assert.h>

namespace larbys {
  namespace util {
    cv::Mat Datum2Image::datum2image( caffe::Datum& datum, bool is_color, bool has_pmt ) {
      if ( datum.encoded() )
	caffe::DecodeDatum( &datum, is_color );

      cv::Mat img( datum.height(), datum.width(), CV_8UC3 );
      img = cv::Mat::zeros( datum.height(), datum.width(), CV_8UC3 );
      const std::string& data = datum.data();
      std::vector<char> vec_data( data.c_str(), data.c_str()+data.size());
      int height = datum.height();
      int width = datum.width();
      int nchannels = datum.channels();
      if ( nchannels!=3 && nchannels!=4 ) {
	std::cout << "UNSUPPORTED NCHANNELS" << std::endl;
	assert(false);
      }
      // tpc layers
      for (int h=0; h<height; h++) {
	for (int w=0; w<width; w++) {
	  for (int c=0; c<3; c++) {
	    int index = (c*height + h)*width + w;
	    unsigned int val = static_cast<unsigned short>( vec_data.at(index) );
	    if ( fAugment ) {
	      // brighten the track
	      if ( c!=0 )
		if ( val>10 )
		  val  *= 2;
	      else {
		// blue gets extra help
		if ( val>10 ) {
		  val *= 2;
		}
	      }
	    }
	    val += img.at<cv::Vec3b>( cv::Point(w,h) )[c];
	    if ( fAddTPCtrig && h==(int)(1600-400)/7 )
	      val += 50;

	    if ( val>=255) val = 255;
	    if ( !fAugment )
	      img.at<cv::Vec3b>( cv::Point(w,h) )[c] = (unsigned short)val;
	    else {
	      if ( c==0 ) {
		// blue becomes orange
		int fill1 = img.at<cv::Vec3b>( cv::Point(w,h) )[1] + val*0.647;
		int fill2 = img.at<cv::Vec3b>( cv::Point(w,h) )[2] + val;
		if ( fill1>=255 ) fill1=255;
		if ( fill2>=255 ) fill2 =255;
		img.at<cv::Vec3b>( cv::Point(w,h) )[1] = fill1;
		img.at<cv::Vec3b>( cv::Point(w,h) )[2] = fill2;
	      }
	      else
		img.at<cv::Vec3b>( cv::Point(w,h) )[c] = (unsigned short )val;
	    }
	    
	    //std::cout << "(" << h << "," << w << "," << c << ") " << static_cast<unsigned short>( vec_data.at(index) ) << std::endl;
	  }
	}
      }
      if ( has_pmt ) {
	for (int h=0; h<height; h++) {
	  for (int w=0; w<width; w++) {
	    //int pmtval = (int)img.at<cv::Vec3b>( cv::Point(w,h) )[3]; // pmt value
	    int pmtval = (int)static_cast<unsigned short>( vec_data.at((3*height+h)*width+w) );
	    if ( fAugment ) pmtval *= 1;
	    if ( fAddTPCtrig && ( h==190 || h==310) )
	      pmtval += 50;

	    if ( pmtval>=128  ) pmtval = 128;
	    for (int c=0; c<3; c++) {
	      int index = (c*height + h)*width + w;
	      int chval = (int)img.at<cv::Vec3b>( cv::Point(w,h) )[c]+pmtval;
	      
	      if ( chval>=255 ) chval = 255;
	      img.at<cv::Vec3b>( cv::Point(w,h) )[c] = chval;
	    }
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
