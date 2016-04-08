#ifndef __ROOT2DATA__
#define __ROOT2DATA__

// ROOT
#include "TChain.h"

// Caffe
#include "caffe/proto/caffe.pb.h"

// larbys root utilities
#include "adc2rgb.h"
#include "root2image.h"
#include "UBBadWires.h"

// class provides convenience functions to make datum objects

namespace larbys {
  namespace util {

    class Root2Datum {
      
      friend UBBadWires;

    public:
      // config parameters
      typedef enum { kCollectionOnly, kTrinocular } NumPlanes_t;
      typedef enum { kGreyScale, kFalseColor } ColorOption_t;

      Root2Datum( TChain* tree_ptr, NumPlanes_t opt_planes, ColorOption_t opt_color, bool wPMT=false );
      virtual ~Root2Datum() {};
      
      void fillDatum( caffe::Datum& datum, long entry=-1 ); ///< fill a datum instance. if entry is specified, first set the Tree entry
      void overlayImage( const Root2Datum& source, float scale_factor=1.0, float add_thresh=-1.0 );

    protected:
      
      NumPlanes_t fOptPlanes;
      ColorOption_t fOptColors;
      bool fWithPMT;
      
    public:

      larbys::util::Root2Image convertor;
      larbys::util::ADC2RGB colorscale;

      TChain* p_tree;
      std::vector<int>* p_plane0;
      std::vector<int>* p_plane1;
      std::vector<int>* p_plane2;
      std::vector<int>* p_pmt_highgain;
      std::vector<int>* p_pmt_lowgain;
      int nticks;
      int nwires[3];
      
    };
    
  }
}

#endif
