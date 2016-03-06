#include "UBBadWires.h"
#include <cmath>
#include <iostream>

namespace larbys {
  namespace util {

    void UBBadWires::inferBadWireList( const std::vector<int>& imgvec, int height, int width, std::vector<int>& badwirelist ) {
      
      // simple: get mean and rms of each wire/pixel col
      // if below threshold, then we define it as a dead wire
      badwirelist.clear();
      for (int w=0; w<width; w++) {
	float x = 0;
	float xx = 0;
	for (int h=0; h<height; h++) {
	  float adc = (float)(imgvec.at( height*w + h ));
	  x += adc;
	  xx += adc*adc;
	  //std::cout << "adc: " << adc << std::endl;
	}
	x /= (float)height;
	xx /= (float)height;
	float rms = sqrt( xx - x*x );
	if ( rms < fADCrmsThresh ) {
	  //std::cout << "bad wire: " << w << " rms=" << rms << " (" << height << ", " << width << ")" << std::endl;
	  badwirelist.push_back( w );
	}
	else {
	  //std::cout << "ok wire: " << w << " rms=" << rms << std::endl;
	}
      }
      
      // std::cout << "badwirelist (n=" << badwirelist.size() << "): ";
      // for (int i=0; i<badwirelist.size(); i++)
      // 	std::cout << " " << badwirelist.at(i);
      // std::cout << std::endl;
      // std::cin.get();
    }

    void UBBadWires::applyBadWires( std::vector<int>& imgvec, int height, int width, const std::vector<int>& badwirelist ) {
      for (int iw=0; iw<(int)badwirelist.size(); iw++) {
	int w=badwirelist.at(iw);
	for (int h=0; h<height;h++) {
	  imgvec.at( height*w + h ) = 0;
	}
      }
    }
  }
}
