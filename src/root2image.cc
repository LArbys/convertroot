#include "root2image.h"

namespace larbys {
  namespace util {

    cv::Mat Root2Image::vec2image( const std::vector<int>& vec, const int height, const int width ) {

      // Make CV
      cv::Mat img( height, width, CV_8UC3 );

      // fill image
      float BASELINE = colorscale.getBaseline(); 
      for (int h=0; h<height; h++) {
	for (int w=0; w<width; w++) {

	  // get pixel value
	  float val = (float)(vec.at( w*height + h )-BASELINE);

	  // apply rgb scale
	  float r,g,b;
	  colorscale.getRGB( val, r, g, b );
      
	  // get pixel
	  cv::Vec3b color = img.at<cv::Vec3b>(cv::Point(h,w));
	  img.at<cv::Vec3b>(cv::Point(h,w))[0] = b*255;
	  img.at<cv::Vec3b>(cv::Point(h,w))[1] = g*255;
	  img.at<cv::Vec3b>(cv::Point(h,w))[2] = r*255;
	}
      }

      return img;
    }
  }
}
