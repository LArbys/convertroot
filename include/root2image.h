#ifndef __ROOT2IMAGE__
#define __ROOT2IMAGE__

#include "opencv/cv.h"
#include "adc2rgb.h"

namespace larbys {
  namespace util {

    typedef cv::Vec<uchar, 1> Vec1b;   // greyscale, collection only, no pmt
    // prebuilt cv::Vec2b              // greyscale collection only, w pmt
    // prebuilt cv::Vec3b              // rgb collection only no pmt
    typedef cv::Vec<uchar, 4> Vec4b;   // greyscale, 3 planes + 1 pmt; rgb collection-only + 1 PMT
    typedef cv::Vec<uchar, 9> Vec9b;   // rgb trinocular, no pmt
    typedef cv::Vec<uchar, 10> Vec10b; // rgb trinocular, with pmt

    class Root2Image {

    public:

      Root2Image() {
	// we set the PMT color scale
	pmt_colorscale.setBaseline( 0.0 );
	pmt_colorscale.setADC_MIP( 1024.0 );
	pmt_colorscale.setADC_MIN( -40.0 );
	pmt_colorscale.setADC_MAX( 2048.0 );
      };
      virtual ~Root2Image() {};

      ADC2RGB colorscale;
      ADC2RGB pmt_colorscale;

      void vec2image( cv::Mat& mat, const std::vector<int>& img, const int height, const int width, bool rgb=true );
      void vec2image( cv::Mat& mat, const std::vector<int>& img0, const std::vector<int>& img1, const std::vector<int>& img2, 
		      const int height, const int width, bool rgb=true );
      void vec2imageWpmt( cv::Mat& mat, const std::vector<int>& img, const std::vector<int>& pmtimg, const int height, const int width, bool rgb=true );
      void vec2imageWpmt( cv::Mat& mat, const std::vector<int>& img0, const std::vector<int>& img1, const std::vector<int>& img2, const std::vector<int>& pmtimg,
			  const int height, const int width, bool rgb=true );
			 
      

    };

  }
}

#endif
