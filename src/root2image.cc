#include "root2image.h"
#include "assert.h"
#include <iostream>

namespace larbys {
  namespace util {


    void Root2Image::vec2image( cv::Mat& mat,
				const std::vector< std::vector<int>* >& tpc_plane_images,
				const std::vector< std::vector<int>* >& pmt_images,
				int tpc_height, int tpc_width,
				int pmt_height, int pmt_width, 
				bool rgb, bool wpmt ) {

      // configuration parameters
      int nplanes = (int)tpc_plane_images.size();
      int npmtsets = (int)pmt_images.size();

      int eff_height = std::max(tpc_height,pmt_height)+2*fTimePad;
      int eff_width  = std::max(tpc_width, pmt_width) +2*fWirePad;

      int channels_per_tpc_plane = ( rgb ) ? 3 : 1;
      int pmtchannels = ( wpmt ) ? npmtsets : 0;
      const int nchannels = nplanes*channels_per_tpc_plane + pmtchannels;

      float BASELINE = colorscale.getBaseline(); 
      float PMT_BASELINE = pmt_colorscale.getBaseline();

      // Set size of image set
      mat.create( eff_height, eff_width, CV_8UC(nchannels) );

      std::cout << "root2image: nchannels=" << nchannels 
      		<< " ntpcvecs=" << nplanes
      		<< " channels_per_plane=" << channels_per_tpc_plane
      		<< " npmtvecs=" << pmtchannels 
		<< " " << eff_width << " x " << eff_height << std::endl;

      for ( int iplane=0; iplane<nplanes+pmtchannels; iplane++ ) {

	int height, width;
	std::vector<int>* vec = NULL;
	if ( iplane<nplanes ) {
	  vec = tpc_plane_images.at(iplane);
	  height = tpc_height;
	  width  = tpc_width;
	}
	else {
	  vec = pmt_images.at( iplane-nplanes );
	  height = pmt_height;
	  width  = pmt_width;
	}

	for (int h=0; h<height; h++) {
	  for (int w=0; w<width; w++) {

	    // get pixel value
	    float val = (float)(vec->at( (w)*height + (h) )-BASELINE);
	    
	    if ( iplane<nplanes && rgb ) {
	      // apply rgb scale to tpc planes
	      float r,g,b;
	      colorscale.getRGB( val, r, g, b );
	      float bgr[3] = { b, g, r };
	      
	      // set pixel ( this is gross )
	      for (int i=0; i<3; i++) {
		switch( nchannels ) {
		case 1:
		  mat.at< cv::Vec<uchar,1> >(cv::Point(w+fWirePad,h+fTimePad))[ iplane*channels_per_tpc_plane + i ] = bgr[i]*255;
		  break;
		case 3:
		  mat.at< cv::Vec<uchar,3> >(cv::Point(w+fWirePad,h+fTimePad))[ iplane*channels_per_tpc_plane + i ] = bgr[i]*255;
		  break;
		case 4:
		  mat.at< cv::Vec<uchar,4> >(cv::Point(w+fWirePad,h+fTimePad))[ iplane*channels_per_tpc_plane + i ] = bgr[i]*255;
		  break;
		case 9:
		  mat.at< cv::Vec<uchar,9> >(cv::Point(w+fWirePad,h+fTimePad))[ iplane*channels_per_tpc_plane + i ] = bgr[i]*255;
		  break;
		case 10:
		  mat.at< cv::Vec<uchar,10> >(cv::Point(w+fWirePad,h+fTimePad))[ iplane*channels_per_tpc_plane + i ] = bgr[i]*255;
		  break;
		default:
		  std::cout << "root2image: unsupported number of channels=" << nchannels << std::endl;
		  assert(false);
		  break;
		}//end of switch
	      }//end of loop over bgr channels
	    }
	    else {
	      // pmts or greyscale tpc
	      float gsval = colorscale.getGreyscale( val );
	      int gs = std::min( (int)255, (int)(255*gsval) );
	      if ( gs<0 ) gs = 0;
	      int index;
	      if ( iplane<nplanes ) 		// tpc channels
		index = iplane;
	      else // pmt channels
		index = nplanes*channels_per_tpc_plane+(iplane-nplanes);
	      //std::cout << gsval << " " << gs << " " << val << std::endl;
	      switch( nchannels ) {
	      case 1:
		mat.at< cv::Vec<uchar,1> >(cv::Point(w+fWirePad,h+fTimePad))[ index ] = gs;
		break;
	      case 3:
		mat.at< cv::Vec<uchar,3> >(cv::Point(w+fWirePad,h+fTimePad))[ index ] = gs;
		break;
	      case 4:
		mat.at< cv::Vec<uchar,4> >(cv::Point(w+fWirePad,h+fTimePad))[ index ] = gs;
		break;
	      case 9:
		mat.at< cv::Vec<uchar,9> >(cv::Point(w+fWirePad,h+fTimePad))[ index ] = gs;
		break;
	      case 10:
		mat.at< cv::Vec<uchar,10> >(cv::Point(w+fWirePad,h+fTimePad))[ index ] = gs;
		break;
	      default:
		std::cout << "root2image: unsupported number of channels=" << nchannels << std::endl;
		assert(false);
		break;
	      }//end of switch
	      
	    }//end of if pmt or greyscale tpc

	    //std::cout << "img (" << h << "," << w << "," << iplane << "): " << val << "->" << mat.at<cv::Vec<uchar,3> >(cv::Point(h+fTimePad,w+fWirePad)) << std::endl;
	    //std::cin.get();
	  }//end of width loop
	}//end of height loop
      }//end of plane loop
      
    }//end of general vec2image setup

  }
}
