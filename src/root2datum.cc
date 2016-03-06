#include "root2datum.h"
#include "assert.h"

// OpenCV
#include "opencv/cv.h"
#include "opencv2/opencv.hpp"

// Caffe
#include "caffe/util/io.hpp"

namespace larbys {
  namespace util {

    Root2Datum::Root2Datum(  TChain* tree_ptr, NumPlanes_t opt_planes, ColorOption_t opt_color, bool wPMT ) {
      if ( tree_ptr==NULL ) {
	// throw
	assert(false);
      }
      
      p_tree = tree_ptr;
      fOptPlanes = opt_planes;
      fOptColors = opt_color;

      // Set branch addresses
      p_plane0 = p_plane1 = p_plane2 = p_pmt_lowgain = p_pmt_highgain = 0;
      if ( fOptPlanes==kTrinocular ) {
	p_tree->SetBranchAddress( "img_plane0", &p_plane0 );
	p_tree->SetBranchAddress( "img_plane1", &p_plane1 );
      }
      p_tree->SetBranchAddress( "img_plane2", &p_plane2 );
      if ( wPMT ) {
	p_tree->SetBranchAddress( "img_pmt_highgain", &p_pmt_highgain );
	p_tree->SetBranchAddress( "img_pmt_lowgain", &p_pmt_lowgain );
      }
      fWithPMT = wPMT;
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
      int height = sqrt( p_plane2->size() ); // hmm, this should be tree variables
      int width = height; /// hmm, this should be tree variables
      if ( !fWithPMT ) {
	if ( fOptPlanes==kTrinocular )
	  convertor.vec2image( cv_img, *p_plane0, *p_plane1, *p_plane2, height, width, rgb );
	else
	  convertor.vec2image( cv_img, *p_plane2, height, width, rgb );
      }
      else {
	// with pmt
	if ( fOptPlanes==kTrinocular )
	  convertor.vec2imageWpmt( cv_img, *p_plane0, *p_plane1, *p_plane2, *p_pmt_highgain, height, width, rgb );
	else
	  convertor.vec2imageWpmt( cv_img, *p_plane2, *p_pmt_highgain, height, width, rgb );
      }
      caffe::CVMatToDatum( cv_img, &datum );

    }

    void Root2Datum::overlayImage( const Root2Datum& source ) {
      std::vector<int>* my_branches[5] = { p_plane2, p_plane0, p_plane1, p_pmt_lowgain, p_pmt_highgain };
      std::vector<int>* their_branches[5] = { source.p_plane2, source.p_plane0, source.p_plane1, source.p_pmt_lowgain, source.p_pmt_highgain };
      for (int b=0; b<5; b++) {
	if ( my_branches[b]!=NULL && their_branches[b]!=NULL) {
	  if ( my_branches[b]->size()!=their_branches[b]->size() )
	    std::cout << "image size mismatch! " << my_branches[b]->size() << " vs. " << their_branches[b]->size() << std::endl;
	  for (int i=0; i<(int)my_branches[b]->size(); i++)
	    my_branches[b]->at(i) += their_branches[b]->at(i);
	}
      }
    }

  }//end of util
}
