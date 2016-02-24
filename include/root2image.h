#ifndef __ROOT2IMAGE__
#define __ROOT2IMAGE__

#include "opencv/cv.h"
#include "adc2rgb.h"

namespace larbys {
  namespace util {

    typedef cv::Vec<uchar, 9> Vec9b;

    class Root2Image {

    public:

      Root2Image() {};
      virtual ~Root2Image() {};

      ADC2RGB colorscale;

      void vec2image( cv::Mat& mat, const std::vector<int>& img, const int height, const int width );
      void vec2image( cv::Mat& mat, const std::vector<int>& img0, const std::vector<int>& img1, const std::vector<int>& img2, 
		      const int height, const int width );
			 
      

    };

  }
}

#endif
