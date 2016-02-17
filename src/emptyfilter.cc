#include "emptyfilter.h"

namespace larbys {
  namespace util {

    bool EmptyFilter::passesFilter( const std::vector<int>& imgvec ) {
      float npixelsabove = 0;
      float totpixels = 0;
      for ( std::vector<int>::const_iterator it_pixel=imgvec.begin(); it_pixel!=imgvec.end(); it_pixel++ ) {
	if ( (float)(*it_pixel)>fADCthreshold)
	  npixelsabove+=1.0;
	totpixels += 1.0;
      }
      if ( npixelsabove/npixelsabove > fFracPixelAbove )
	return true;
      return false;
    }

  }
}
