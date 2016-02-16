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

// ROOT
#include "TFile.h"
#include "TChain.h"
#include "TRandom3.h"

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
  int SEED = 12356;


  // read input files
  std::vector< std::pair<std::string,int> > inputlist; // [filename, label]
  std::set<int> labelset = parse_inputlist( infile, inputlist );

  // sort input files by label
  int NLABELS = (int)labelset.size();
  std::vector<std::string>** classfiles = new std::vector<std::string>*[NLABELS];
  std::map< int, int > classindex;
  std::map< int, int > entriesleft_per_class;
  std::map< int, int > currententry_per_class;
  // setup an array index for the classes
  int class_index_temp = 0;
  for ( std::set<int>::iterator it_label=labelset.begin(); it_label!=labelset.end(); it_label++ ) {
    classindex[ *it_label ] = class_index_temp;
    class_index_temp++;
    classfiles[ classindex[ *it_label ] ] = new std::vector<std::string>;
  }
  // store input files by class
  for ( std::vector< std::pair<std::string,int> >::iterator it_file=inputlist.begin(); it_file!=inputlist.end(); it_file++ ) {
    int label = (*it_file).second;
    classfiles[ classindex[label] ]->push_back( (*it_file).first );
  }
  // load trees, setup branches and count number of entries
  TChain** classtrees = new TChain*[NLABELS];
  std::vector<int>** p_bb_img_plane2 = new std::vector<int>*[NLABELS];
  for ( std::set<int>::iterator it_label=labelset.begin(); it_label!=labelset.end(); it_label++ ) {
    classtrees[ classindex[*it_label] ] = new TChain( "yolo/bbtree" );
    p_bb_img_plane2[ classindex[*it_label] ] = 0;
    // we could loop, but for now assume number of entries = nfiles*50;
    for ( std::vector<std::string >::iterator it_file=classfiles[ classindex[*it_label] ]->begin(); it_file!=classfiles[ classindex[*it_label] ]->end(); it_file++ ) {
      classtrees[ classindex[*it_label] ]->Add( (*it_file).c_str() );
    }
    classtrees[ classindex[*it_label] ]->SetBranchAddress("img_plane2", &p_bb_img_plane2[ classindex[*it_label] ]);
    entriesleft_per_class[ *it_label ] = classtrees[ classindex[*it_label] ]->GetEntries();
    currententry_per_class[ *it_label ] = 0;
    std::cout << "[ Label " << *it_label << " ] Number of entries: " << entriesleft_per_class[ *it_label ] << std::endl;
  }

  // For each event we do a few things: 
  // (1) Calculate mean of all images
  // (2) put data into datum object
  // (3) put label in datum object
  // (4) store that object into an lmdb database


  // Output LMDB
  std::string FLAGS_backend = "lmdb";
  boost::scoped_ptr<caffe::db::DB> db(caffe::db::GetDB(FLAGS_backend));
  db->Open(outdb.c_str(), caffe::db::NEW);
  boost::scoped_ptr<caffe::db::Transaction> txn(db->NewTransaction());

  // convertor
  larbys::util::Root2Image convertor;

  // Image Protobuf
  caffe::Datum datum;

  // Event Loop
  int tot_entries_left = 0;
  for ( std::set<int>::iterator it_label=labelset.begin(); it_label!=labelset.end(); it_label++ ) {
    tot_entries_left += entriesleft_per_class[ *it_label ];
  }
  int numfilled = 0;
  TRandom3 rand( SEED );

  while ( tot_entries_left>0 ) {

    // we choose at random which one to fill
    float larbys = rand.Uniform();
    float horseysauce = 0.;
    int label = 0;
    int fill_index = 0;
    for ( std::set<int>::iterator it_label=labelset.begin(); it_label!=labelset.end(); it_label++ ) {
      horseysauce += float(entriesleft_per_class[ *it_label ])/float(tot_entries_left);
      if ( larbys < horseysauce ) {
	label = *it_label;
	fill_index = classindex[*it_label];
	break;
      }
    }

    classtrees[ fill_index ]->GetEntry( currententry_per_class[ fill_index ] );
    currententry_per_class[ fill_index ]++;
    entriesleft_per_class[ fill_index ]--;

    // extract image as opencv mat
    int height = sqrt( p_bb_img_plane2[ fill_index ]->size() );
    int width = height;
    cv::Mat cv_img = convertor.vec2image( *(p_bb_img_plane2[fill_index]), height, width );
    if ( cv_img.data ) {
      if ( enc.size() ) {
	// encode the values
	std::vector<uchar> buf;
	cv::imencode("."+enc, cv_img, buf );
	datum.set_data( std::string( reinterpret_cast<char*>(&buf[0]), buf.size() ) );
	datum.set_label( label );
	datum.set_encoded(true);
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
      std::string key_str = "class_" + caffe::format_int( label, 2  ) + "_" + caffe::format_int( currententry_per_class[ fill_index ]-1, 8 );
      
      // store in db
      txn->Put( key_str, out );
      numfilled++;
      

      if ( numfilled%100==0 ) {
	txn->Commit();
	txn.reset( db->NewTransaction() );
	//std::cout << "process " << entry << " images" << std::endl;
      }
    }//if data is good

    // update number of entries
    tot_entries_left = 0;
    for ( std::set<int>::iterator it_label=labelset.begin(); it_label!=labelset.end(); it_label++ ) {
      tot_entries_left += entriesleft_per_class[ *it_label ];
    }

    if ( tot_entries_left%1000==0 ) {
      std::cout << "entries left: ";
      for ( std::set<int>::iterator it_label=labelset.begin(); it_label!=labelset.end(); it_label++ ) {
	std::cout << " [" << *it_label <<  "] " << entriesleft_per_class[ *it_label ] << " ";
      }
      std::cout << std::endl;
    }

  }
  
  // last commit
  txn->Commit();
  txn.reset( db->NewTransaction() );
  
  std::cout << "FIN." << std::endl;
  
  return 0;
}
