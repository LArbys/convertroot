#ifndef __DATUM2IMAGE__
#define __DATUM2IMAGE__

#include "opencv/cv.h"
#include "caffe/util/io.hpp"

namespace larbys {
  namespace util {

    class Datum2Image {

    public:
      Datum2Image() {};
      virtual ~Datum2Image() {};

      cv::Mat datum2image( caffe::Datum& datum, bool is_color=true );

    };

  }
}

#endif
