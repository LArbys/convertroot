#include "PMTWireWeights.h"
#include <iostream>
#include "opencv2/imgproc/imgproc.hpp"

namespace larbys {
  namespace util {
    
    PMTWireWeights::PMTWireWeights( int wire_rows ) {
      fNWires = wire_rows;
      fCurrentWireWidth = -1;

      fGeoInfoFile = "configfiles/geoinfo.root";
      std::cout << "Filling Weights using " << fGeoInfoFile << std::endl;
      fGeoFile = new TFile( fGeoInfoFile.c_str(), "OPEN" );

      // Get the PMT Info
      fNPMTs = 32;
      fPMTTree  = (TTree*)fGeoFile->Get( "imagedivider/pmtInfo" );
      int femch;
      float pos[3];
      fPMTTree->SetBranchAddress( "femch", &femch );
      fPMTTree->SetBranchAddress( "pos", pos );
      for (int n=0; n<fNPMTs; n++) {
	fPMTTree->GetEntry(n);
	//std::cout << "[pmt " << femch << "] ";
	for (int i=0; i<3; i++) {
	  pmtpos[femch][i] = pos[i];
	  // if ( i==2 )
	  //   pmtpos[femch][i] += 10.0; // alignment issues?
	  //std::cout << pmtpos[femch][i] << " ";
	}
	//std::cout << std::endl;
      }

      // Get the Wire Info
      fWireTree = (TTree*)fGeoFile->Get( "imagedivider/wireInfo" );
      int wireID;
      int planeID;
      float start[3];
      float end[3];
      fWireTree->SetBranchAddress( "wireID", &wireID );
      fWireTree->SetBranchAddress( "plane",  &planeID );
      fWireTree->SetBranchAddress( "start", start );
      fWireTree->SetBranchAddress( "end", end );
      
      int nentries = fWireTree->GetEntries();
      for ( int ientry=0; ientry<nentries; ientry++ ) {
	fWireTree->GetEntry(ientry);
	if ( m_WireData.find( planeID )==m_WireData.end() ) {
	  // cannot find instance of wire data for plane id. make one.
	  m_WireData[planeID] = WireData( planeID );
	}
	m_WireData[planeID].addWire( wireID, start, end );
      }

      std::cout << "Number of wire data stored (per plane)" << std::endl;
      for ( std::map<int,WireData>::iterator it=m_WireData.begin(); it!=m_WireData.end(); it++) {
	std::cout << " [Plane " << (*it).first << "]: " << (*it).second.nwires() << std::endl;
      }

      
      // Configure
      configure();
    }
    
    PMTWireWeights::~PMTWireWeights() {
    }
    
    void PMTWireWeights::configure() {
      // here build the set of weights
      // each PMT gets assigned a weight that is: w = d/(D)
      // d is the shortest distance between the wire and pmt center
      // D is the sum of all such distances such that sum(w,NPMTS=32) = 1.0
      // we make a weight matrix W = [ N, M ] where N is the number of wires and M is the number of PMTs
      // the way we will use this is to assign each PMT a value: z = q/Q so that z is the fraction of charge seen in the trigger window
      // the weight assigned to each wire will be W*Z, where Z is the vector of z values for all M PMTs.

      for ( std::map<int,WireData>::iterator it=m_WireData.begin(); it!=m_WireData.end(); it++ ) {
	int plane = (*it).first;
	WireData const& data = (*it).second;
	int nwires = data.nwires();
	if ( fNWires>0 )
	  nwires = fNWires;

	cv::Mat mat( nwires, fNPMTs, CV_32F );
	mat = cv::Mat::zeros( nwires, fNPMTs, CV_32F );

	int iwires = 0;
	for ( std::set< int >::iterator it_wire=data.wireIDs.begin(); it_wire!=data.wireIDs.end(); it_wire++ ) {
	  int wireid = (*it_wire);
	  // we first need to project the data into 2D: z,y -> (x,y)
	  std::vector< float > const& start = (*(data.wireStart.find(wireid))).second;
	  std::vector< float > const& end   = (*(data.wireEnd.find(wireid))).second;
	  float s2[2] = { start.at(2), start.at(1) };
	  float e2[2] = { end.at(2),   end.at(1)   };
	  float l2 = (*(data.wireL2.find(wireid))).second;

	  std::vector<float> dists(fNPMTs,0.0);
	  float denom = 0;
	  //std::cout << "[plane " << plane << ", wire " << wireid << "] ";
	  for (int ipmt=0; ipmt<fNPMTs; ipmt++) {
	    float p2[2] = { pmtpos[ipmt][2], pmtpos[ipmt][1] };
	    float d = getDistance2D( s2, s2, p2, l2 );
	    float w = 1.0/(d*d);
	    
	    if ( plane<=1 ) // we use a different, wider weight for the V plane -- alignement problems?
	      w = 1.0/d;

	    dists[ipmt] = w;
	    if ( w>denom )
	      denom = w;

	    //denom += 1.0/(d*d);
	    //std::cout << d << " ";
	    //denom += d;
	  }
	  //std::cout << std::endl;

	  // populate the matrix
	  for (int ipmt=0; ipmt<fNPMTs; ipmt++) {
	    mat.at<float>( wireid, ipmt ) = dists.at(ipmt)/denom;
	    if ( wireid==2399 && plane<=1 ) {
	      // we pad out the right side a bit, to help with some alignment issues on the edge
	      for (int pad=0;pad<768; pad++) {
		mat.at<float>( wireid+pad, ipmt ) = dists.at(ipmt)/denom;
	      }
	    }
	  }
	  iwires++;
	}//end of wire loop
	planeWeights[plane] = mat;
      }//end of plane loop
    }//end of configure()
    
    float PMTWireWeights::getDistance2D( float s[], float e[], float p[], float l2 ) {
      // we assume that the user has projected the information into 2D
      // we calculate the projection of p (the pmt pos) onto the line segment formed by (e-s), the end point of the wires
      // where s is the origin of the coorindate system

      // since we want distance from wire to pmt, perpendicular to the wire, we can form
      // a right triangle. the distance is l = sqrt(P^2 - a^2), where a is the projection vector

      float ps[2]; // vector from wire start to PMT pos
      float es[2]; // vector from wire start to wire end
      float dot = 0.0;  // dot product of the above
      float psnorm = 0.0; // distane from wire start to pmt pos
      for (int i=0; i<2; i++) {
	ps[i] = p[i]-s[i];
	es[i] = e[i]-s[i];
	dot += ps[i]*es[i];
	psnorm += ps[i]*ps[i];
      }
      float dist = sqrt( psnorm - dot*dot/l2 );
      return dist;
    }//end of getDistance2D
    
    void PMTWireWeights::applyWeights( const cv::Mat& plane_images_src, 
				       const std::vector<int>& pmt_highgain_adcvec, 
				       const std::vector<int>& pmt_lowgain_adcvec, 
				       cv::Mat& plane_images_weighted ) {
      // the pmt inputs are awkward
      // expecting a vector containing an image of the PMTs
      // this is going to be hacky. In the future, use a better data source
      
      // first, set the weight image size
      if ( plane_images_src.size().width!=fCurrentWireWidth || fCurrentWireWidth==-1) {
	fCurrentWireWidth = plane_images_src.size().width;
	for (int p=0; p<3; p++) {
	  cv::Mat weightimg( fCurrentWireWidth, fNPMTs, CV_32F );
	  resize( planeWeights[p], weightimg, weightimg.size(), CV_INTER_LINEAR );
	  weightImage[p] = weightimg;
	}
      }
      
      cv::Mat pmtq( fNPMTs, 1, CV_32F );
      pmtq = cv::Mat::zeros( fNPMTs, 1, CV_32F );
      float totalq = 0.0;
      for (int ipmt=0; ipmt<fNPMTs; ipmt++) {
	float high_q = 0.0;
	float low_q  = 0.0;
	bool usehigh = true;
      
	for (int t=190; t<320; t++) {
	  // sum over the trigger window
	  int index = t*768 + ipmt*int(768/fNPMTs);
	  float highadc = pmt_highgain_adcvec.at( index );
	  float lowadc  = pmt_lowgain_adcvec.at( index );
	  if ( highadc<30 ) { // no single pe hits
	    highadc = 0;
	    lowadc = 0;
	  }
	  //std::cout << "(" << ipmt << "," << t << "): " << highadc << std::endl;
	  high_q += highadc;
	  low_q  += lowadc;
	  if ( highadc>1040 )
	    usehigh = false;
	}
	if ( high_q > 0.0 ) {
	  if ( usehigh ) {
	    pmtq.at<float>(ipmt,1) = high_q/100.0;
	    totalq += high_q/100.0;
	  }
	  else {
	    pmtq.at<float>(ipmt,1) = 10.0*low_q/100.0;
	    totalq += 10.0*low_q/100.0;
	  }
	}
      }//end of PMT loop
      
      
      // normalize charge
      if ( totalq > 0 ) {
	for (int ipmt=0; ipmt<fNPMTs; ipmt++) {
	  pmtq.at<float>(ipmt,1) /= totalq;
	}
      }

      std::cout << "TOTAL PMTQ: " << totalq << std::endl;
      //std::cout << "PMTQ: " << pmtq << std::endl;
      //std::cin.get();

      int height = plane_images_src.size().height;
      int width  = plane_images_src.size().width;
      plane_images_weighted.create( height, width, CV_8UC3 );
      plane_images_weighted = cv::Mat::zeros( height, width, CV_8UC3 );

      cv::Mat wireweights[3];
      for (int p=0; p<3; p++) {

	wireweights[p] = weightImage[p]*pmtq;
	
	for (int w=0; w<width; w++) {
	  float weight = wireweights[p].at<float>( 1, w );
	  for (int h=0; h<height; h++) {
	    double val = static_cast<double>( plane_images_src.at<cv::Vec3b>( h, w )[p] ) * weight;
	    // if ( c>=0 ) {
	    //   // augment
	    //   val = std::min( 255, val*5 );
	    //   if ( c==0 )
	    // 	val = std::min( 255, val*5 );
	    // }
	    val = std::min( 254.99, val );
	    plane_images_weighted.at<cv::Vec3b>( h, w )[p] = (int)val;
	  } // end of time loop
	  //std::cout << "W*P = " << (*outsize[p]).size() << " " << pmtq.size() << std::endl;
	  //std::cout << "ww: " << wireweights[p].size() << std::endl;
	  //std::cout << wireweights[p] << std::endl;
	  //std::cin.get();
	}//end of wire loop
      }
      
    }// end of apply wire weights
  }
}
