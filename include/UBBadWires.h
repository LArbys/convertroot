#ifndef __UBBADWIRES__
#define __UBBADWIRES__

// This utility class 

#include <vector>

namespace larbys {
  namespace util {


    class UBBadWires {

    public:
      UBBadWires( float adc_rms_thresh ) {
	fADCrmsThresh = adc_rms_thresh;
      };
      virtual ~UBBadWires() {};

      void inferBadWireList( const std::vector<int>& imgvec, int height, int width, std::vector<int>& badwirelist );
      void applyBadWires( std::vector<int>& imgvec, int height, int width, const std::vector<int>& badwirelist );      

    protected:
      float fADCrmsThresh;

    };

  }
}


#endif
