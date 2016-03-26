#include "root2datum.h"
#include "assert.h"
#include <iostream>

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
      bool wpmt=false;
      if ( fWithPMT ) wpmt = true;
      
      cv::Mat cv_img;
      int height = sqrt( p_plane2->size() ); // hmm, this should be tree variables
      int width = height; /// hmm, this should be tree variables

      std::vector< std::vector<int>* > tpc_vecs;
      if ( fOptPlanes==kTrinocular ) {
	tpc_vecs.push_back( p_plane0 );
	tpc_vecs.push_back( p_plane1 );
	tpc_vecs.push_back( p_plane2 );
      }
      else {
	tpc_vecs.push_back( p_plane2 );
      }

      std::vector< std::vector<int>* > pmt_vecs;
      if ( fWithPMT )
	pmt_vecs.push_back( p_pmt_highgain );

      
      convertor.vec2image( cv_img, tpc_vecs, pmt_vecs, height, width, height, width, rgb, wpmt );
      caffe::CVMatToDatum( cv_img, &datum );

    }

    void Root2Datum::overlayImage( const Root2Datum& source, float scale_factor ) {
      std::vector<int>* my_branches[5] = { p_plane2, p_plane0, p_plane1, p_pmt_lowgain, p_pmt_highgain };
      std::vector<int>* their_branches[5] = { source.p_plane2, source.p_plane0, source.p_plane1, source.p_pmt_lowgain, source.p_pmt_highgain };
      for (int b=0; b<5; b++) {
	if ( my_branches[b]!=NULL && their_branches[b]!=NULL) {
	  if ( my_branches[b]->size()!=their_branches[b]->size() )
	    std::cout << "image size mismatch! " << my_branches[b]->size() << " (" << sqrt( my_branches[b]->size() ) << ")"
		      << " vs. " << their_branches[b]->size() << " (" << sqrt(their_branches[b]->size()) << ")" << std::endl;
	  for (int i=0; i<(int)my_branches[b]->size(); i++)
	    my_branches[b]->at(i) += their_branches[b]->at(i)*scale_factor;
	}
      }
    }

  }//end of util
}
