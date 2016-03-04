#include "root2image.h"

namespace larbys {
  namespace util {

    void Root2Image::vec2image( cv::Mat& img, const std::vector<int>& vec, const int height, const int width, bool rgb ) {

      // 1 Plane into CV matrix

      // Make CV
      if ( rgb )
	img.create( height, width, CV_8UC3 );
      else
	img.create( height, width, CV_8UC1 );

      // fill image
      float BASELINE = colorscale.getBaseline(); 
      for (int h=0; h<height; h++) {
	for (int w=0; w<width; w++) {

	  // get pixel value
	  float val = (float)(vec.at( w*height + h )-BASELINE);

	  if ( rgb ) {
	    // apply rgb scale
	    float r,g,b;
	    colorscale.getRGB( val, r, g, b );
	    
	    // set pixel
	    cv::Vec3b color = img.at<cv::Vec3b>(cv::Point(h,w));
	    img.at<cv::Vec3b>(cv::Point(h,w))[0] = b*255;
	    img.at<cv::Vec3b>(cv::Point(h,w))[1] = g*255;
	    img.at<cv::Vec3b>(cv::Point(h,w))[2] = r*255;
	  }
	  else {
	    val = colorscale.getGreyscale( val );
	    int gs = std::max( (int)255, (int)(255*val) );
	    if ( gs<0 ) gs = 0;
	    img.at<Vec1b>(cv::Point(h,w))[0] = gs;
	  }
	}
      }
    }
    
    // make trinocular matrix
    void Root2Image::vec2image( cv::Mat& img, const std::vector<int>& vec0, const std::vector<int>& vec1, const std::vector<int>& vec2, 
				const int height, const int width, bool rgb ) {
      
      // Make CV
      if ( rgb ) 
	img.create( height, width, CV_8UC(9) );
      else
	img.create( height, width, CV_8UC(3) );
      
      // fill image
      float BASELINE = colorscale.getBaseline(); 
      for (int h=0; h<height; h++) {
	for (int w=0; w<width; w++) {
	  
	  // get pixel values
	  float val[3] = { (float)(vec0.at( w*height + h )-BASELINE),
			   (float)(vec1.at( w*height + h )-BASELINE),
			   (float)(vec2.at( w*height + h )-BASELINE) };
	  for (int iplane=0; iplane<3; iplane++) {
	    
	    if ( rgb ) {
	      // apply rgb scale
	      float r,g,b;
	      colorscale.getRGB( val[iplane], r, g, b );
	      
	      // set pixel
	      img.at<Vec9b>(cv::Point(h,w))[3*iplane+0] = b*255;
	      img.at<Vec9b>(cv::Point(h,w))[3*iplane+1] = g*255;
	      img.at<Vec9b>(cv::Point(h,w))[3*iplane+2] = r*255;
	    }
	    else {
	      // apply greyscale
	      img.at<cv::Vec3b>(cv::Point(h,w))[iplane] = colorscale.getGreyscale( val[iplane] )*255;
	    }
	  }
	}
      }
    }//end of vec2img    
  }
}
