#include "root2image.h"

namespace larbys {
  namespace util {

    void Root2Image::vec2image( cv::Mat& img, const std::vector<int>& vec, const int height, const int width ) {

      // Make CV
      img.create( height, width, CV_8UC3 );

      // fill image
      float BASELINE = colorscale.getBaseline(); 
      for (int h=0; h<height; h++) {
	for (int w=0; w<width; w++) {

	  // get pixel value
	  float val = (float)(vec.at( w*height + h )-BASELINE);

	  // apply rgb scale
	  float r,g,b;
	  colorscale.getRGB( val, r, g, b );
      
	  // get pixel
	  cv::Vec3b color = img.at<cv::Vec3b>(cv::Point(h,w));
	  img.at<cv::Vec3b>(cv::Point(h,w))[0] = b*255;
	  img.at<cv::Vec3b>(cv::Point(h,w))[1] = g*255;
	  img.at<cv::Vec3b>(cv::Point(h,w))[2] = r*255;
	}
      }
    }

    // make trinocular matrix
    void Root2Image::vec2image( cv::Mat& img, const std::vector<int>& vec0, const std::vector<int>& vec1, const std::vector<int>& vec2, 
				const int height, const int width ) {
      
      // Make CV
      img.create( height, width, CV_8UC(9) );
      
      // fill image
      float BASELINE = colorscale.getBaseline(); 
      for (int h=0; h<height; h++) {
	for (int w=0; w<width; w++) {
	  
	  // get pixel values
	  float val[3] = { (float)(vec0.at( w*height + h )-BASELINE),
			   (float)(vec1.at( w*height + h )-BASELINE),
			   (float)(vec2.at( w*height + h )-BASELINE) };
	  for (int i=0; i<3; i++) {
	    // apply rgb scale
	    float r,g,b;
	    colorscale.getRGB( val[i], r, g, b );
	    
	    // set pixel
	    img.at<Vec9b>(cv::Point(h,w))[3*i+0] = b*255;
	    img.at<Vec9b>(cv::Point(h,w))[3*i+1] = g*255;
	    img.at<Vec9b>(cv::Point(h,w))[3*i+2] = r*255;
// 	    std::cout << "(" << h << ", " << w << "," << i << ")  " 
// 		      << img.at<Vec9b>(cv::Point(h,w))[3*i+0] << ", "
// 		      << img.at<Vec9b>(cv::Point(h,w))[3*i+0] << ", " 
// 		      << img.at<Vec9b>(cv::Point(h,w))[3*i+0] << std::endl;
	  }
	}
      }
//       std::cout << std::endl;
//       std::cin.get();
    }//end of vec2img    
  }
}
