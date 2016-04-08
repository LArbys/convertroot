#include "BBAnnotationParser.hh"
#include <iostream>

namespace larbys {
  namespace util {

    BBAnnotationParser::BBAnnotationParser( std::string inputfile ) {
      finput = new std::ifstream( inputfile.c_str() );
    }

    BBAnnotationParser::~BBAnnotationParser() {
      delete finput;
    }

    void BBAnnotationParser::parse() {

      int nboxes = 0;
      while ( !finput->eof() ) {
	char buffer[1024];
	char buflabel[10];
	(*finput) >> buffer >> buflabel;
	std::string bbox_key = buffer;

	if ( bboxInfo.find( bbox_key )==bboxInfo.end() ) {
	  bboxInfo.insert( std::map< std::string, std::vector<BBoxInfo> >::value_type( bbox_key, std::vector<BBoxInfo>() ) );
	}

	if ( std::atoi(buflabel)!=0 ) {
	  // if has truth boxes
	  // loop with same keys

	  BBoxInfo bb;

	  for ( int i=0; i<12; i++) {
	    (*finput) >> buffer;
	    if ( i<4 )
	      bb.plane0.push_back( std::atoi( buffer ) );
	    else if ( 4<=i && i<8 )
	      bb.plane1.push_back( std::atoi( buffer ) );
	    else if ( 8<=i && i<12 )
	      bb.plane2.push_back( std::atoi( buffer ) );
	  }
	  (*finput) >> buffer; // string label which we don't use for now
	  bb.label = buffer;

	  bboxInfo[bbox_key].emplace_back( bb );
	  nboxes++;
	}//else if cosmics
      }//annotation file loop
      std::cout << "found " << bboxInfo.size() << " entries. a total of " << nboxes << " bounding boxes." << std::endl;

    }//end of parse

    void BBAnnotationParser::writeInfo( std::string key, std::ofstream& out ) {
 
      auto iter = bboxInfo.find( key );
      if ( iter==bboxInfo.end() ) {
	std::cout << "could not find info for key=" << key << std::endl;
	return;
      }
      std::vector<BBoxInfo>& bboxes = (*iter).second;
      //std::cout << "writing bbox output for " << key << ". nboxes=" << bboxes.size() << std::endl;
      if ( bboxes.size()==0 )
	out << key << " " << 0;
      else {
	for ( auto &bbox : bboxes ) {
	  //std::cout << "  plane0: " << bbox.plane0.size() << " plane1: " << bbox.plane1.size() << " plane2: " << bbox.plane2.size() << std::endl;
	  out << key << " " << 1 << " ";
	  out << bbox.plane0.at(0) << " " << bbox.plane0.at(1) << " " << bbox.plane0.at(2) << " " << bbox.plane0.at(3) << " ";
	  out << bbox.plane1.at(0) << " " << bbox.plane1.at(1) << " " << bbox.plane1.at(2) << " " << bbox.plane1.at(3) << " ";
	  out << bbox.plane2.at(0) << " " << bbox.plane2.at(1) << " " << bbox.plane2.at(2) << " " << bbox.plane2.at(3) << " ";
	  out << bbox.label;
	}
      }
      out << std::endl;
    }
    
  }
}
