#include "datum2img.h"


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
	    img.at<cv::Vec3b>( cv::Point(h,w) )[c] = static_cast<unsigned short>( vec_data.at(index) );
	  }
	}
      }
      return img;
    }
  }
}
