#ifndef __EMPTY_FILTER__
#define __EMPTY_FILTER__

#include <vector>

namespace larbys {
  namespace util {

    class EmptyFilter {
      
    public:
      EmptyFilter( float ADC_threshold, float frac_above ) { 
	fADCthreshold=ADC_threshold; 
	fFracPixelAbove=frac_above;
      };
      virtual ~EmptyFilter() {};

      bool passesFilter( const std::vector<int>& imgvec ); // to be used with root data

    protected:
      float fADCthreshold;
      float fFracPixelAbove;


    };

  }
}

#endif
