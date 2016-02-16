#ifndef __ROOT2IMAGE__
#define __ROOT2IMAGE__

#include "opencv/cv.h"
#include "adc2rgb.h"

namespace larbys {
  namespace util {

    class Root2Image {

    public:

      Root2Image() {};
      virtual ~Root2Image() {};

      ADC2RGB colorscale;

      cv::Mat vec2image( const std::vector<int>& img, const int height, const int width );
			 
      

    };

  }
}

#endif
