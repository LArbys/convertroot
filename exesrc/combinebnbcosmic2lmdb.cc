#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <set>
#include <utility>

// This prepares data stored in LArby's rootfiles into LMDB format for caffe.
// The reason we have a separate program is this task has a number of specializations
// (1) determine labels based on mode, nuscatter and flavor categories
// (2) The BNB data is already shuffled. No need to shuffle classes like the single particle code.
// (3) We split the cosmic samples listed in half.  First half is used as background sample. Second half is used to overlay with BNB neutrino.


// Caffe and LMDB
#include "lmdb.h"
#include "caffe/common.hpp"
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
#include "root2datum.h"
#include "emptyfilter.h"
#include "BNBLabels.h"
#include "UBBadWires.h"

// ROOT
#include "TFile.h"
#include "TChain.h"
#include "TRandom3.h"

void  parse_inputlist( std::string filename, std::vector<std::string>& inputlist ) {

  std::ifstream infile( filename.c_str() );
  char buffer[5120];
  std::string fname;
  std::string lastname = "";
  while ( !infile.eof() ) {
    infile >> buffer;
    fname = buffer;
    if ( fname!="" && fname!=lastname ) {
      inputlist.push_back( fname );
      lastname = fname;
    }
  }
}


int main( int narg, char** argv ) {


  std::string infile_neutrino = argv[1];
  std::string infile_cosmics = argv[2];
  std::string outdb = argv[3];
  std::string outbblist = "out.txt";
  std::string enc = "";
  int SEED = 123567;
  bool fTrinocular = true; // fold in all three views into data
  bool fAddPMT = false;
  std::string producer = "bnbcosmics";
  const int fNumPlanes = 3;
  double fEnergyCut = -1;
  bool fApplyBadWireMask = true;

  typedef enum { kNu=0, kCosmic, NTYPES } FileTypes_t;

  larbys::util::Root2Datum::ColorOption_t fColor = larbys::util::Root2Datum::kGreyScale;
  //larbys::util::Root2Datum::ColorOption_t fColor = larbys::util::Root2Datum::kFalseColor;
  

  // read input files
  std::vector< std::string > inputlist[NTYPES];
  parse_inputlist( infile_neutrino, inputlist[kNu] );
  parse_inputlist( infile_cosmics, inputlist[kCosmic] );
  
  // load trees, setup branches and count number of entries
  // type of tree used for neutrinos
  TChain** classtrees = new TChain*[NTYPES];
  for (int i=0; i<NTYPES; i++) {
    classtrees[i] = new TChain( std::string(producer+"/imgtree").c_str() );
    for ( int j=0; j<inputlist[i].size(); j++)
      classtrees[i]->Add( inputlist[i].at(j).c_str() );
  }

  std::cout << "Setup Image Maker" << std::endl;
  larbys::util::Root2Datum* root2datum[NTYPES];
  for (int i=0; i<NTYPES; i++) {
    if ( fTrinocular ) 
      root2datum[i] = new larbys::util::Root2Datum( classtrees[i], larbys::util::Root2Datum::kTrinocular, fColor, fAddPMT );
    else
      root2datum[i] = new larbys::util::Root2Datum( classtrees[i], larbys::util::Root2Datum::kCollectionOnly, fColor, fAddPMT );
    root2datum[i]->colorscale.setADC_MIN(0);
    root2datum[i]->colorscale.setADC_MAX(255);
  }

  // neutrino tree has MC variables
  int mode, nuscatter, flavor;
  float Enu;
  int nbboxes;
  classtrees[kNu]->SetBranchAddress( "nbboxes", &nbboxes );
  classtrees[kNu]->SetBranchAddress( "mode", &mode );
  classtrees[kNu]->SetBranchAddress( "nuscatter", &nuscatter );
  classtrees[kNu]->SetBranchAddress( "flavor", &flavor );
  classtrees[kNu]->SetBranchAddress( "Enu", &Enu );

  // bounding box info will be needed
  std::vector<std::string>* p_bblabels[NTYPES];
  std::vector<int>* p_LoLeft_t_plane[NTYPES][fNumPlanes];
  std::vector<int>* p_LoLeft_w_plane[NTYPES][fNumPlanes];
  std::vector<int>* p_HiRight_t_plane[NTYPES][fNumPlanes];
  std::vector<int>* p_HiRight_w_plane[NTYPES][fNumPlanes];
  
  for (int i=0; i<NTYPES; i++) {
    p_bblabels[i] = 0;
    for (int p=0; p<fNumPlanes; p++) {

      p_LoLeft_t_plane[i][p] = 0;
      p_LoLeft_w_plane[i][p] = 0;
      p_HiRight_t_plane[i][p] = 0;
      p_HiRight_w_plane[i][p] = 0;

      char branchname[50];
      sprintf( branchname, "LoLeft_t_plane%d", p );
      classtrees[i]->SetBranchAddress( branchname, &(p_LoLeft_t_plane[i][p]) );
      sprintf( branchname, "LoLeft_w_plane%d", p );
      classtrees[i]->SetBranchAddress( branchname, &(p_LoLeft_w_plane[i][p]) );
      sprintf( branchname, "HiRight_t_plane%d", p );
      classtrees[i]->SetBranchAddress( branchname, &(p_HiRight_t_plane[i][p]) );
      sprintf( branchname, "HiRight_w_plane%d", p );
      classtrees[i]->SetBranchAddress( branchname, &(p_HiRight_w_plane[i][p]) );
    }
    classtrees[i]->SetBranchAddress( "bblabels", &(p_bblabels[i]) );
  }
  
  int nbnb_entries = classtrees[kNu]->GetEntries();
  int ncosmic_entries = classtrees[kCosmic]->GetEntries();

  std::cout << "number of BNB entries: " << nbnb_entries << std::endl;
  std::cout << "number of cosmics entries: " << ncosmic_entries << std::endl;


  // Output LMDB
  std::string FLAGS_backend = "lmdb";
  boost::scoped_ptr<caffe::db::DB> db(caffe::db::GetDB(FLAGS_backend));
  db->Open(outdb.c_str(), caffe::db::NEW);
  boost::scoped_ptr<caffe::db::Transaction> txn(db->NewTransaction());

  // Output annotation file
  // outputs per line
  // [key] [img-label] [bounding box] [bounding box type]
  std::ofstream annotation( outbblist.c_str() );

  // filter
  larbys::util::EmptyFilter filter( 10.0, 10.0/(448.0*448.0) );

  // bad wire tool
  larbys::util::UBBadWires* badwires[fNumPlanes];
  badwires[0] = new larbys::util::UBBadWires( 0.01 );
  badwires[1] = new larbys::util::UBBadWires( 0.01 );
  badwires[2] = new larbys::util::UBBadWires( 0.01 );

  // Image Protobuf
  caffe::Datum datum;

  // Event Loop

  int numrejected = 0;
  TRandom3 rand( SEED );

  int entry[NTYPES];
  int filled[NTYPES];
  long bytes[NTYPES];
  for (int itype=0; itype<NTYPES; itype++ ) {
    entry[itype] = 0;
    filled[itype] = 0;
    bytes[itype] = classtrees[itype]->GetEntry( entry[itype] );
  }

  std::cout << "START LOOP" << std::endl;

  FileTypes_t next_type_to_fill = kNu;

  while ( entry[kNu] < nbnb_entries && entry[kCosmic] < ncosmic_entries ) {

    FileTypes_t ftype = kNu;
    if ( rand.Uniform()>0.5 )
      ftype = kCosmic;

    // get the label
    larbys::util::BNBLabels_t bnblabel = larbys::util::kBackground; // default to background
    if ( ftype==kNu ) {
      //bnblabel = larbys::util::labelFromRootVars( mode, nuscatter, flavor ); // if neutrino, we label it
      bnblabel = larbys::util::kNumuCCQE; // this is label '1'.
    }

    // tell people what we're doing
    if ( ftype==kCosmic )
      std::cout << "Make a cosmic (label=" << bnblabel << ") event (using " << entry[kCosmic] << ")" << std::endl;
    else
      std::cout << "Make a neutrino (label=" << bnblabel << ") event (entry " << entry[kNu] << ", cosmic entry " << entry[kCosmic] << ")" << std::endl;	

    // if we are filling a neutrino, we need to make sure the next event actually has one
    // and that the neutrino isn't some low energy event
    if ( ftype==kNu ) {

      // filter out images with little charge (not used for now)
      bool passes_plane0 = true;
      bool passes_plane1 = true;
      bool passes_plane2 = true;
      // bool passes_plane2 = filter.passesTotalFilter( *(p_plane2), 20*100 );
      // if ( kTrinocular ) {
      //   passes_plane0 = filter.passesTotalFilter( *(p_plane0), 20*100 );
      //   passes_plane0 = filter.passesTotalFilter( *(p_plane1), 20*100 );
      // }

      while ( bytes[kNu]>0 && ( nbboxes==0 || p_bblabels[kNu]->at(0)!="neutrino"
				|| !passes_plane0 || !passes_plane1 || !passes_plane2
				|| Enu<fEnergyCut ) ) {
	entry[kNu]++;
	bytes[kNu] = classtrees[kNu]->GetEntry( entry[kNu] );
      }
      if ( bytes[kNu]==0 ) // guess we ran out of neutrinos
	break;
      
    }// end of find useable event
          
    // if neutrino, add in the cosmic (after first removing bad wires
    if ( ftype==kNu ) {
      if ( fApplyBadWireMask ) {
	if ( fTrinocular ) {
	  std::vector<int> imgbadwires0;
	  std::vector<int> imgbadwires1;
	  badwires[0]->inferBadWireList( *(root2datum[kCosmic]->p_plane0), 
					 sqrt(root2datum[kCosmic]->p_plane0->size()), 
					 sqrt(root2datum[kCosmic]->p_plane0->size()), imgbadwires0 );
	  badwires[1]->inferBadWireList( *(root2datum[kCosmic]->p_plane1), 
					 sqrt(root2datum[kCosmic]->p_plane1->size()), 
					 sqrt(root2datum[kCosmic]->p_plane1->size()), imgbadwires1 );
	  badwires[0]->applyBadWires( *(root2datum[kNu]->p_plane0),
				      sqrt(root2datum[kNu]->p_plane0->size()),
				      sqrt(root2datum[kNu]->p_plane0->size()), imgbadwires0 );
	  badwires[1]->applyBadWires( *(root2datum[kNu]->p_plane1), 
				      sqrt(root2datum[kNu]->p_plane1->size()), 
				      sqrt(root2datum[kNu]->p_plane1->size()), imgbadwires1 );
	}
	std::vector<int> imgbadwires2;
	badwires[2]->inferBadWireList( *(root2datum[kCosmic]->p_plane2), 
				       sqrt(root2datum[kCosmic]->p_plane2->size()), 
				       sqrt(root2datum[kCosmic]->p_plane2->size()), imgbadwires2 );
	badwires[2]->applyBadWires( *(root2datum[kNu]->p_plane2), 
				    sqrt(root2datum[kNu]->p_plane2->size()), 
				    sqrt(root2datum[kNu]->p_plane2->size()), imgbadwires2 );      
      }
      root2datum[kNu]->overlayImage( *(root2datum[kCosmic]) );
    }
      
    // extract image into the datum
    root2datum[ftype]->fillDatum( datum );

    // set the label!!
    datum.set_label( (int)bnblabel );

    // serialize
    std::string out;
    CHECK( datum.SerializeToString(&out) );
      
    // make key for db entry
    int numfilled = filled[kNu]+filled[kCosmic];
    std::string key_str = caffe::format_int(numfilled,10) + "_" + caffe::format_int( (int)bnblabel, 2  );
    
    // store in db
    txn->Put( key_str, out );
    filled[ftype]++;

    // output annotation line
    if ( ftype==kCosmic ) {
      annotation << key_str << " " << (int)bnblabel << std::endl;
    }
    else {
      annotation << key_str << " " << (int)bnblabel << " ";
      for (int ibox=0; ibox<nbboxes; ibox++) {
	for (int iplane=0; iplane<fNumPlanes; iplane++) {
	  annotation << p_LoLeft_t_plane[kNu][iplane]->at(ibox) << " " 
		     << p_LoLeft_w_plane[kNu][iplane]->at(ibox) << " "
		     << p_HiRight_t_plane[kNu][iplane]->at(ibox) << " "
		     << p_HiRight_w_plane[kNu][iplane]->at(ibox) << " ";
	}
	annotation << " " << p_bblabels[ftype]->at(ibox) << std::endl;
      }
    }
      
    if ( numfilled>0 && numfilled%100==0 ) {
      txn->Commit();
      txn.reset( db->NewTransaction() );
      //std::cout << "process " << entry << " images" << std::endl;
    }
    
    // if ( numfilled >=20 )
    //break;

    if ( ftype==kCosmic )
      std::cout << "Filled cosmic" << std::endl;
    else
      std::cout << "Filled neutrino" << std::endl;

    for (int f=0; f<NTYPES; f++)  {
      entry[f]++;
      bytes[f] = classtrees[f]->GetEntry( entry[f] );
    }

  }
  
  // last commit
  txn->Commit();
  txn.reset( db->NewTransaction() );
  
  std::cout << "FIN." << std::endl;
  std::cout << "Number of neutrinos filled: " << filled[kNu] << std::endl;
  std::cout << "Number of cosmics filled: " << filled[kCosmic] << std::endl;
  
  return 0;
}
