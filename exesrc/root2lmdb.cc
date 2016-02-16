#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <set>
#include <utility>

// Caffe and LMDB
#include "lmdb.h"
#include "caffe/common.hpp"
//#include "caffe.pb.h" // datum definition
#include "caffe/proto/caffe.pb.h"
#include "caffe/util/format.hpp"
#include "caffe/util/db.hpp"
#include "caffe/util/io.hpp"

// Boost
#include "boost/scoped_ptr.hpp"

// OpenCV
#include "opencv/cv.h"
#include "opencv2/opencv.hpp"

// larbys root utilities
#include "adc2rgb.h"
#include "root2image.h"

#include "TFile.h"
#include "TChain.h"

std::set<int>  parse_inputlist( std::string filename, std::vector< std::pair<std::string,int> >& inputlist ) {

  std::ifstream infile( filename.c_str() );
  char buffer[5120];
  std::string fname;
  std::string lastname = "";
  int label;
  std::set< int > labels;
  while ( !infile.eof() ) {
    infile >> buffer >> label;
    fname = buffer;
    labels.insert( label );
    if ( fname!="" && fname!=lastname ) {
      inputlist.push_back( std::make_pair(fname,label) );
      lastname = fname;
    }
  }
  return labels;
}


bool ReadImageToDatum(const cv::Mat& cv_img, const int label,
		      const int height, const int width, const bool is_color,
		      const std::string & encoding, caffe::Datum* datum) {

  if (cv_img.data) {
    if (encoding.size()) {
      std::vector<uchar> buf;
      cv::imencode("."+encoding, cv_img, buf);
      datum->set_data( std::string(reinterpret_cast<char*>(&buf[0]), buf.size()) );
      datum->set_label(label);
      datum->set_encoded(true);
      return true;
    }
    caffe::CVMatToDatum(cv_img, datum);
    datum->set_label(label);
    return true;
  } else {
    return false;
  }
}

int main( int narg, char** argv ) {

  std::string infile = argv[1];
  std::string outdb = argv[2];
  std::string enc = "";

  std::vector< std::pair<std::string,int> > inputlist; // [filename, label]
  std::set<int> labelset = parse_inputlist( infile, inputlist );


  // For each event we do a few things: 
  // (1) Calculate mean of all images
  // (2) put data into datum object
  // (3) store that object into an lmdb database
  // (4) make the training list with labels

  // Output LMDB
  std::string FLAGS_backend = "lmdb";
  boost::scoped_ptr<caffe::db::DB> db(caffe::db::GetDB(FLAGS_backend));
  db->Open(outdb.c_str(), caffe::db::NEW);
  boost::scoped_ptr<caffe::db::Transaction> txn(db->NewTransaction());


  // convertor
  larbys::util::Root2Image convertor;

  // Image Protobuf
  caffe::Datum datum;

  // File loop
  std::map<int,int> counter;
  for ( std::set<int>::iterator it_label=labelset.begin(); it_label!=labelset.end(); it_label++ ) {
    counter[*it_label] = 0;
  }


  for ( std::vector< std::pair<std::string,int> >::iterator it_file=inputlist.begin(); it_file!=inputlist.end(); it_file++ ) {
    // get filename, label pair
    int label = (*it_file).second;
    std::string input_fname = (*it_file).first;

  // Load ROOT Tree
  
    std::cout << "[LOAD TREE]" << std::endl;
    TChain* bbtree = new TChain("yolo/bbtree");
    int nfiles = 0;
    bbtree->Add( input_fname.c_str() );
    std::cout << "[Label " << label << "] " << input_fname << std::endl;

    // setup branches
    //char label[100];
    std::vector<int>* p_bb_img_plane2 = 0;
    //bbtree->SetBranchAddress("label",label);
    bbtree->SetBranchAddress("img_plane2", &p_bb_img_plane2);

    int entry = 0;
    unsigned long bytes = bbtree->GetEntry(entry);
    while ( bytes!=0 ) {
      std::cout << "Entry " << entry << ": " << label << std::endl;
      // extract image as opencv mat
      int height = sqrt( p_bb_img_plane2->size() );
      int width = height;
      cv::Mat cv_img = convertor.vec2image( *p_bb_img_plane2, height, width );
      if ( enc.size() ) {
	if ( cv_img.data ) {
	  // encode the values
	  std::vector<uchar> buf;
	  cv::imencode("."+enc, cv_img, buf );
	  datum.set_data( std::string( reinterpret_cast<char*>(&buf[0]), buf.size() ) );
	  datum.set_label( label );
	  datum.set_encoded(true);
	}
	else {
	  std::cout << "[ ERROR ] Image could not be converted into matrix" << std::endl;
	  entry++;
	  bytes = bbtree->GetEntry(entry);
	  continue;
	}
      }
      else {
	// convert into datum object
	caffe::CVMatToDatum( cv_img, &datum );
	datum.set_label( label );
	datum.set_encoded(false);
      }

      // serialize
      std::string out;
      CHECK( datum.SerializeToString(&out) );

      // make key for db entry
      std::string key_str = "class_" + caffe::format_int( label, 2  ) + "_" + caffe::format_int( entry, 8 ) + "_" + input_fname;

      // store in db
      txn->Put( key_str, out );

      entry++;
      bytes = bbtree->GetEntry(entry);

      if ( entry%100==0 ) {
	txn->Commit();
	txn.reset( db->NewTransaction() );
	std::cout << "process " << entry << " images" << std::endl;
      }


      if ( entry>10 )
	break;
    }//end of loop over root entries

    // last commit
    txn->Commit();
    txn.reset( db->NewTransaction() );

  }//end of loop over files

  
  return 0;
}
