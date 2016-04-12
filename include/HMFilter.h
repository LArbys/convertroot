#ifndef __HMFilter__
#define __HMFilter__

// HIP/MIP Filter
// Takes in images of 3 planes and produces another image showing where the MIP and HIP pixels are

#include <vector>
#include "opencv/cv.h"

namespace larbys {
  namespace util {

    class HMFilter {

    public:
      
      HMFilter( int width, int height ) {
	fWidth = width;
	fHeight = height;
	fMIPbounds[0] = 10.0;
	fMIPbounds[1] = 30.0;
      };
      ~HMFilter() {};

      int fWidth;
      int fHeight;

      float fMIPbounds[2];
      void setMIPbounds( float lower, float upper ) { 
	fMIPbounds[0] = lower;
	fMIPbounds[1] = upper;
      };

      void applyFilter( const cv::Mat& src, cv::Mat& mip_img, cv::Mat& hip_img );

    };

  }
}

#endif
