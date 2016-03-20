#ifndef __BBAnnotationParser__
#define __BBAnnotationParser__

#include <fstream>
#include <string>
#include <vector>
#include <map>

namespace larbys {
  namespace util {

    class BBAnnotationParser {
      
    public:
      
      BBAnnotationParser( std::string inputfile );
      virtual ~BBAnnotationParser();

      std::ifstream* finput;

      class BBoxInfo {
      public:
	BBoxInfo() {};
	virtual ~BBoxInfo(){};

	std::vector<int> plane0;
	std::vector<int> plane1;
	std::vector<int> plane2;
	std::string label;
	
      };

      std::map< std::string, std::vector<BBoxInfo> >  bboxInfo;
      
      void parse();
      
      void writeInfo( std::string key, std::ofstream& out );

    };

  }
}

#endif
