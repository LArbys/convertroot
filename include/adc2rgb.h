#ifndef __ADC2RBG__
#define __ADC2RBG__

// Class holds ADC to RGB conversion parameters and provides conversion function
namespace larbys {

  namespace util {

    class ADC2RGB {

    public:

      ADC2RGB() {
	// defaults
	setDefault();
      };
      virtual ~ADC2RGB() {};

    protected:

      float BASELINE;
      float ADC_MIP;
      float ADC_MIN;
      float ADC_MAX;

    public:
      void getRGB( float ADC, float&r, float&g, float& b );
      void setBaseline( float baseline_ ) { BASELINE = baseline_; };
      void setADC_MIP( float ADC_MIP_ ) { ADC_MIP = ADC_MIP_; };
      void setADC_MIN( float ADC_MIN_ ) { ADC_MIN = ADC_MIN_; };
      void setADC_MAX( float ADC_MAX_ ) { ADC_MAX = ADC_MAX_; };
      float getBaseline() { return BASELINE; };
      void setDefault() {
	BASELINE = 0;
	ADC_MIP = 20.0;
	ADC_MIN = -10.0;
	ADC_MAX = 190.0;
      };

    };

  }

}

#endif
