#ifndef __PMTpos__
#define __PMTpos__

#include <vector>

// Hacky class to provide PMT position information

namespace larbys {
  namespace util {

    class PMTPos {
      
    public:
      PMTPos() {};
      virtual ~PMTPos() {};

      void getPMTPos( int femch, std::vector<float>& pmtpos );
      
    protected:
      
      static const float fPospmtposmap[32][3];
      
    };

  }
}

#endif
