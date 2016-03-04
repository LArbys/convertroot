#include "adc2rgb.h"

namespace larbys {
  
  namespace util {
    

    void ADC2RGB::getRGB( float ADC, float&r, float&g, float& b ) {
      // out of range
      if ( ADC<ADC_MIN ) {
	r = 0; g = 0; b = 1.0;
	return;
      }
      if ( ADC>ADC_MAX ) {
	r = 1.0; g = 0; b = 0;
      }
      
      // 0 to 1.0 MIPs: blue to green
      if ( ADC < ADC_MIP ) {
	float colorlen = ADC_MIP - ADC_MIN;
	g = (ADC-ADC_MIN)/colorlen;
	b = (1 - (ADC-ADC_MIN)/colorlen);
	r = 0;
      }
      // 1.0 to 2.0 MIPs green to red
      else if ( ADC>=ADC_MIP ) {
	float colorlen = ADC_MAX - ADC_MIP;
	b = 0;
	g = (ADC-ADC_MIP)/colorlen;
	r = (1.0 - (ADC-ADC_MIP)/colorlen);
      }

    }


    float ADC2RGB::getGreyscale( float ADC ) {
      // out of range
      if ( ADC<ADC_MIN ) {
	return 0.0;
      }
      if ( ADC>ADC_MAX ) {
	return 1.0;
      }
      
      return (ADC-ADC_MIN)/(ADC_MAX-ADC_MIN);
    }

    
  }//end of larbys::util namespace
  
}
