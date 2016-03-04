#include "root2datum.h"
#include "assert.h"

// OpenCV
#include "opencv/cv.h"
#include "opencv2/opencv.hpp"

// Caffe
#include "caffe/util/io.hpp"

namespace larbys {
  namespace util {

    Root2Datum::Root2Datum(  TChain* tree_ptr, NumPlanes_t opt_planes, ColorOption_t opt_color ) {
      if ( tree_ptr==NULL ) {
	// throw
	assert(false);
      }
      
      p_tree = tree_ptr;
      fOptPlanes = opt_planes;
      fOptColors = opt_color;

      // Set branch addresses
      p_plane0 = p_plane1 = p_plane2 = 0;
      if ( fOptPlanes==kTrinocular ) {
	p_tree->SetBranchAddress( "img_plane0", &p_plane0 );
	p_tree->SetBranchAddress( "img_plane1", &p_plane1 );
      }
      p_tree->SetBranchAddress( "img_plane2", &p_plane2 );

    }
    
    void Root2Datum::fillDatum( caffe::Datum& datum, long entry ) {

      if ( entry>=0 ) {
	long bytes = p_tree->GetEntry( entry );
	if (bytes==0)
	  assert(false);
      }

      bool rgb=true;
      if ( fOptColors==kGreyScale ) rgb = false;
      
      cv::Mat cv_img;
      int height = sqrt( p_plane0->size() ); // hmm, this should be tree variables
      int width = height; /// hmm, this should be tree variables
      if ( fOptPlanes==kTrinocular )
	convertor.vec2image( cv_img, *p_plane0, *p_plane1, *p_plane2, height, width, rgb );
      else
	convertor.vec2image( cv_img, *p_plane2, height, width, rgb );
      
      caffe::CVMatToDatum( cv_img, &datum );

    }
    
  }
}
