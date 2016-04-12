#include "HMFilter.h"

namespace larbys {
  namespace util {

    void HMFilter::applyFilter( const cv::Mat& src, cv::Mat& mip_img, cv::Mat& hip_img ) {
      // applies it to one plane
      mip_img.create( src.size().height, src.size().width, CV_8UC1 );
      hip_img.create( src.size().height, src.size().width, CV_8UC1 );
      
      for (int r=0; r<src.size().height; r++) {
	for (int c=0; c<src.size().width; c++) {
	  unsigned short val = src.at<unsigned short>( r, c );

	  if ( (float)val < fMIPbounds[0] ) {
	    mip_img.at<unsigned short>( r, c ) = 0;
	    hip_img.at<unsigned short>( r, c ) = 0;
	  }
	  else if ( (float)val>=fMIPbounds[0] && (float)val<=fMIPbounds[1] ) {
	    mip_img.at<unsigned short>( r, c ) = val;
	    hip_img.at<unsigned short>( r, c ) = 0;
	  }
	  else {
	    mip_img.at<unsigned short>( r, c ) = 0;
	    hip_img.at<unsigned short>( r, c ) = val;
	  }
	}
      }

    }

  }
}
