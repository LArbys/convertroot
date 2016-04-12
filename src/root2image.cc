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
      mat = cv::Mat::zeros( eff_height, eff_width, CV_8UC(nchannels) );

      std::cout << "root2image: nchannels=" << nchannels 
      		<< " ntpcvecs=" << nplanes
      		<< " channels_per_plane=" << channels_per_tpc_plane
      		<< " npmtvecs=" << pmtchannels 
		<< " zeros=" << colorscale.getGreyscale( 0.0 ) << "," << PMT_BASELINE
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
	  //height = pmt_height;
	  //width  = pmt_width;
	  height = tpc_height;
	  width  = tpc_width;
	  if ( fPMTformat==kTPCtimescale )
	    continue; // if fancy overlay mode, we skip the PMT portion for now. we do this later
	}

	for (int h=0; h<height; h++) {
	  for (int w=0; w<width; w++) {

	    // get pixel value
	    float val;
	    if ( iplane<nplanes )
	      val = (float)(vec->at( (w)*height + (h) )-BASELINE); // tpc
	    else
	      val = (float)(vec->at( (h)*width + (width-1-w) )-BASELINE);  // pmt

	    // apply scaling
	    if ( fApplyScaling )
	      val *= fScaling;

	    // noise threshold
	    if ( fApplyThreshold && val<fADCThreshold )
	      val = 0.0;
	    
	    if ( iplane<nplanes && rgb ) {
	      // apply rgb scale to tpc planes
	      float r,g,b;
	      colorscale.getRGB( val, r, g, b );
	      float bgr[3] = { b, g, r };
	      
	      // set pixel ( this is gross )
	      for (int i=0; i<3; i++) {
		switch( nchannels ) {
		case 1:
		  mat.at< cv::Vec<uchar,1> >(cv::Point(h+fTimePad,w+fWirePad))[ iplane*channels_per_tpc_plane + i ] = bgr[i]*255;
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

      // 
      if ( fPMTformat==kTPCtimescale ) {
	for (int ipmtplane=0; ipmtplane<pmtchannels; ipmtplane++ ) {
	  int index = nplanes*channels_per_tpc_plane+(ipmtplane);
	  fillPMTImage( mat, *(tpc_plane_images.at(ipmtplane)), index, nchannels, pmt_height, pmt_width );
	}
      }
      
    }//end of general vec2image setup

    void Root2Image::pmtpos2pixel( const std::vector<float>& pos, int& wirepix, int& timepix, int tlen, int wlen,  int rowsperpmt ) {
      // we sum the pe in the trigger window between 50 and 150, dividing sum into 3 bins
      // then we artifically past it into an overlay of the TPC image

      int ncollectionwires = 3456;
      int nblocks = (ncollectionwires/wlen);
      if ( ncollectionwires%wlen!=0 )
	nblocks+=1;
      int wirewidth = nblocks*wlen;
      float wirescale = float(ncollectionwires)/float(wirewidth); // wires per pixel
      int endpixel = float(wirescale) * float(wlen);

      float z = 1000.0 - pos.at(2);
      float y = pos.at(1);
      int t0pix = (int)(fTtrig_TimePix-fTstart_TimePix)*(tlen/fTimeOrigScale); // position in our image when the pmts fire

      int yrow = 0;
      if ( y>50 )
	yrow = rowsperpmt*4;
      else if (y<50 && y>20 )
	yrow = rowsperpmt*3;
      else if ( y<20 && y>-20 )
	yrow = rowsperpmt*2;
      else if ( y<-20 && y>-50 )
	yrow = rowsperpmt*1;
      else
	yrow = rowsperpmt*0;
    
      timepix = t0pix+yrow;
      wirepix = z*(endpixel/1000.0);
      
    }
    
    void Root2Image::fillPMTImage( cv::Mat& mat, const std::vector<int>& pmtvec, int fillchannel, int nchannels, int pmt_height, int pmt_width ) {

      int chanblock = (int)pmt_width/32;
      int tickblock = (int)(1500/pmt_height);
      int rowsperpmt = 5;

      int winstart = 2.90/(tickblock*15.625e-3);
      int winend   = 4.85/(tickblock*15.625e-3);
      int winbin = (winend-winstart)/rowsperpmt;
      std::cout << "fill pmt with pe integral between " << winstart << " and " << winend << " binsize=" << winbin << std::endl;

      for ( int chan=0; chan<32; chan++ ) {
	float pmtbinsum = 0;
	int tpix, wire;
	std::vector<float> pmtpos;
	fpmtposmap.getPMTPos( chan, pmtpos );
	pmtpos2pixel( pmtpos, wire, tpix, pmt_height, pmt_width, rowsperpmt );
	for (int ibin=0; ibin<rowsperpmt; ibin++) {
	  for (int t=winstart+ibin*winbin; t<winstart+(ibin+1)*winbin; t++) {
	    if ( t>= pmt_height )
	      break;
	    int idx = pmt_width*(t) + ((float(chan)+0.5)*chanblock);
	    pmtbinsum += pmtvec.at( idx );
	  }

	  float gsval = pmt_colorscale.getGreyscale( pmtbinsum );
	  int gs = std::min( (int)255, (int)(255*gsval) );

	  for (int w=-10; w<=10; w++) {
	    switch( nchannels ) {
	    case 4:
	      mat.at< cv::Vec<uchar,4> >(cv::Point(w+wire+fWirePad,tpix+ibin+fTimePad))[ fillchannel ] = gs;
	      break;
	    case 10:
	      mat.at< cv::Vec<uchar,10> >(cv::Point(w+wire+fWirePad,tpix+ibin+fTimePad))[ fillchannel ] = gs;
	      break;
	    default:
	      std::cout << "root2image: unsupported number of channels=" << nchannels << std::endl;
	      assert(false);
	      break;
	    }//end of switch
	  }
	}
      }
    }
    
    
  }
}
