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

      typedef enum { kNotDefined=-1, kGreyScale, kGreyScalewPMT, kRGB, kRGBwPMT } ImageData_t;

      Root2Image() {
	// we set the PMT color scale
	pmt_colorscale.setBaseline( 0.0 );
	pmt_colorscale.setADC_MIP( 1024.0 );
	pmt_colorscale.setADC_MIN( -40.0 );
	pmt_colorscale.setADC_MAX( 2048.0 );
	setTimePadding(0);
	setWirePadding(0);
      };
      virtual ~Root2Image() {};

      ADC2RGB colorscale;
      ADC2RGB pmt_colorscale;
      int fTimePad;
      int fWirePad;
      void setTimePadding( int pad ) { fTimePad = pad; };
      void setWirePadding( int pad ) { fWirePad = pad; };

      void vec2image( cv::Mat& mat, 
		      const std::vector< std::vector<int>* >& tpc_plane_images, 
		      const std::vector< std::vector<int>* >& pmt_images,
		      int tpc_height, int tpc_width,
		      int pmt_height, int pmt_width, bool rgb, bool wpmt );
		      
      

    };

  }
}

#endif
