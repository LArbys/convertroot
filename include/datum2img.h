#ifndef __DATUM2IMAGE__
#define __DATUM2IMAGE__

#include "opencv/cv.h"
#include "caffe/util/io.hpp"

namespace larbys {
  namespace util {

    class Datum2Image {

    public:
      Datum2Image() { fAugment=false; };
      virtual ~Datum2Image() {};

      cv::Mat datum2image( caffe::Datum& datum, bool is_color=true, bool has_pmt=true );
      void datum2TriData( cv::Mat& mat, caffe::Datum& datum, bool is_color=true );
      void tridata2image( const cv::Mat& tridata, cv::Mat& image );

      void setAugment( bool augment ) { fAugment=augment; }; // augment contrast
      void addTPCtrig( bool add ) { fAddTPCtrig=add; };

    protected:
      bool fAugment;
      bool fAddTPCtrig;

    };

  }
}

#endif
