#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <set>
#include <utility>
#include <algorithm>

// This prepares data stored in LArby's rootfiles into LMDB format for caffe.
// The reason we have a separate program is this task has a number of specializations
// (1) determine labels based on mode, nuscatter and flavor categories
// (2) The BNB data is already shuffled. No need to shuffle classes like the single particle code.
// (3) We use two different cosmic ray muon sources: (1) EXT unbiased and (2) EXT+BNB SWTRIG
//     The first we combine with MC neutrinos, the second we use for the background-only sample
// (4) We arrange the data for a trimese network. Each plane gets the image, plus addition data:
//     (1) a pmt-charge weighted image and (2) HIP and MIP filtered images
//     This data is arranged [plane0,plane0-pmtweighted,plane0-MIP,plane0-HIP,plane1,...]
// (5) We pad in the time region to allow for different crops during training
// (6) We also reuse the MC neutrinos (several times), changing the cosmic background 
//      and the scaling factor applied to the MC ADCs.
// (7) We vary the ADC threshold slightly as well

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
#include "PMTWireWeights.h"
#include "HMFilter.h"

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


  std::string infile_neutrino            = argv[1];
  std::string infile_cosmics_bnbext      = argv[2];
  std::string infile_cosmics_extprescale = argv[3];
  std::string outdb_train = argv[4];
  std::string outdb_test  = argv[5];
  std::string outbblist_train = "bbox_out_train.txt";
  std::string outbblist_test  = "bbox_out_test.txt";
  std::string enc = "";
  larbys::util::BNBLabels_t target_label = larbys::util::kNumuCCQE;
  int SEED = 123567;
  bool fTrinocular = true; // fold in all three views into data
  bool fAddPMT = true;
  std::string producer = "bnbcosmics";
  const int fNumPlanes = 3;
  double fEnergyCut = 0.400;
  bool fApplyBadWireMask = true;
  float train_val_split = 0.7;

  typedef enum { kNu=0, kCosmicBNBEXT, kCosmicPrescale, NTYPES } FileTypes_t;
  typedef enum { kTRAIN=0, kTEST, NOUTPUTS } Outputs_t;

  larbys::util::Root2Datum::ColorOption_t fColor = larbys::util::Root2Datum::kGreyScale;
  
  // read input files
  std::vector< std::string > inputlist[NTYPES];
  parse_inputlist( infile_neutrino, inputlist[kNu] );
  parse_inputlist( infile_cosmics_bnbext, inputlist[kCosmicBNBEXT] );
  parse_inputlist( infile_cosmics_extprescale, inputlist[kCosmicPrescale] );
  
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
    root2datum[i] = new larbys::util::Root2Datum( classtrees[i], larbys::util::Root2Datum::kTrinocular, fColor, fAddPMT );
    root2datum[i]->convertor.colorscale.setADC_MIN(0);
    root2datum[i]->convertor.colorscale.setADC_MAX(255);
    root2datum[i]->convertor.applyThreshold( true );
    root2datum[i]->convertor.setThreshold( 10.0 );
    root2datum[i]->convertor.setTimePadding( 10 );
    if ( fAddPMT )
      root2datum[i]->convertor.setPMTimageFormat( larbys::util::Root2Image::kPMTtimescale );
  }

  // neutrino tree has MC variables
  int mode, nuscatter, flavor;
  float Enu;
  int nbboxes;
  float bb_vertex[3];
  std::vector<float>* p_bb_vertex_x = 0;
  std::vector<float>* p_bb_vertex_y = 0;
  std::vector<float>* p_bb_vertex_z = 0;
  classtrees[kNu]->SetBranchAddress( "nbboxes", &nbboxes );
  classtrees[kNu]->SetBranchAddress( "mode", &mode );
  classtrees[kNu]->SetBranchAddress( "nuscatter", &nuscatter );
  classtrees[kNu]->SetBranchAddress( "flavor", &flavor );
  classtrees[kNu]->SetBranchAddress( "Enu", &Enu );
  classtrees[kNu]->SetBranchAddress( "bb_vertex_x", &p_bb_vertex_x );
  classtrees[kNu]->SetBranchAddress( "bb_vertex_y", &p_bb_vertex_y );
  classtrees[kNu]->SetBranchAddress( "bb_vertex_z", &p_bb_vertex_z );

  
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

  // Get Total Entries
  int sampleentries[NTYPES];
  for (int i=0; i<NTYPES; i++) {
    sampleentries[i] = classtrees[i]->GetEntries();
  }

  std::cout << "[COMPONENT SAMPLE ENTRIES]" << std::endl;
  std::cout << "EXT UNBIASED: " << sampleentries[kCosmicPrescale] << std::endl;
  std::cout << "EXT SWTRIG  : " << sampleentries[kCosmicBNBEXT] << std::endl;
  std::cout << "MCC7 BNB NU : " << sampleentries[kNu] << std::endl;
  std::cin.get();

  // Output LMDB
  std::string FLAGS_backend = "lmdb";
  boost::scoped_ptr<caffe::db::DB> db_train(caffe::db::GetDB(FLAGS_backend));
  db_train->Open(outdb_train.c_str(), caffe::db::NEW);
  boost::scoped_ptr<caffe::db::Transaction> txn_train(db_train->NewTransaction());

  boost::scoped_ptr<caffe::db::DB> db_test(caffe::db::GetDB(FLAGS_backend));
  db_test->Open(outdb_test.c_str(), caffe::db::NEW);
  boost::scoped_ptr<caffe::db::Transaction> txn_test(db_test->NewTransaction());

  // Output annotation file
  // outputs per line
  // [key] [img-label] [bounding box] [bounding box type]
  std::ofstream annotation_train( outbblist_train.c_str() );
  std::ofstream annotation_test(  outbblist_test.c_str() );

  // bad wire tool
  larbys::util::UBBadWires* badwires[fNumPlanes];
  badwires[0] = new larbys::util::UBBadWires( 0.01 );
  badwires[1] = new larbys::util::UBBadWires( 0.01 );
  badwires[2] = new larbys::util::UBBadWires( 0.01 );

  // PMT wire weight tool
  larbys::util::PMTWireWeights pmtweights( 768*5 );

  // HIP/MIP Filter
  larbys::util::HMFilter* hmfilter[3];
  for (int i=0; i<3; i++) {
    hmfilter[i] = new larbys::util::HMFilter(768,768);
    hmfilter[i]->setMIPbounds( 15, 45 );
  }
  
  // Image Protobuf
  caffe::Datum datum;
  caffe::Datum datum_final;

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
  
  while ( entry[kCosmicBNBEXT]<sampleentries[kCosmicBNBEXT]
	  && entry[kCosmicPrescale]<sampleentries[kCosmicPrescale] ) {
    // the cosmic data sample is our limiting factor
    // we are willing to reuse MC events, augmenting by verying the scale factor

    FileTypes_t ftype = kNu;
    larbys::util::BNBLabels_t bnblabel = target_label;
    if ( rand.Uniform()>0.5 ) {
      ftype = kCosmicBNBEXT; // only cosmic alone
      bnblabel = larbys::util::kBackground; // default to background
    }

    // if we are filling a neutrino, we need to make sure the next event actually has one
    // and that the neutrino isn't some low energy event
    if ( ftype==kNu ) {
      // get the next neutrino entry
      bytes[kNu] = classtrees[kNu]->GetEntry( entry[kNu] );

      // get the neutrino label
      larbys::util::BNBLabels_t evt_bnblabel = larbys::util::labelFromRootVars( mode, nuscatter, flavor ); // if neutrino, we label it
      //bnblabel = larbys::util::kNumuCCQE; // this is label '1'.

      while ( bytes[kNu]>0 && ( nbboxes==0 || p_bblabels[kNu]->at(0)!="neutrino"
				|| Enu<fEnergyCut || evt_bnblabel!=target_label
				) ) {
	// failed out selection, we get some more
	std::cout << "  bad neutrino pick: " << evt_bnblabel << " " << Enu << " " << nbboxes << std::endl;
	entry[kNu]++;
	if ( entry[kNu]==sampleentries[kNu] ) entry[kNu] = 0; // roll over
	bytes[kNu] = classtrees[kNu]->GetEntry( entry[kNu] );
	evt_bnblabel = larbys::util::labelFromRootVars( mode, nuscatter, flavor ); // if neutrino, we label it
      }
      
      // get prescale cosmic
      bytes[kCosmicPrescale] = classtrees[kCosmicPrescale]->GetEntry( entry[kCosmicPrescale] );
      if ( bytes[kCosmicPrescale]==0 ) // guess we ran out of ext prescale cosmic windows
	break;
      entry[kCosmicPrescale]++;
      entry[kNu]++;
      std::cout << "  neutrino pick: " << evt_bnblabel << " " << Enu << " " << nbboxes << std::endl; 
    }// end of find useable event
    else {
      bytes[kCosmicBNBEXT] = classtrees[kCosmicBNBEXT]->GetEntry( entry[kCosmicBNBEXT] );
      if ( bytes[kCosmicBNBEXT]==0 )
	break;
      entry[kCosmicBNBEXT]++;
    }
    
    // tell the people what we're doing
    if ( ftype==kCosmicBNBEXT )
      std::cout << "Make a cosmic-only event (label=" << bnblabel << ") event (using BNBEXT entry " << entry[kCosmicBNBEXT] << ")" << std::endl;
    else
      std::cout << "Make a neutrino/cosmic overlay (label=" << bnblabel << ") event (NU entry " << entry[kNu] << ", prescale cosmic entry " << entry[kCosmicPrescale] << ")" << std::endl;
          
    // if neutrino, add in the cosmic (after first removing bad wires
    cv::Mat plane_images;
    if ( ftype==kNu ) {
      if ( fApplyBadWireMask ) {
	if ( fTrinocular ) {
	  std::vector<int> imgbadwires0;
	  std::vector<int> imgbadwires1;
	  badwires[0]->inferBadWireList( *(root2datum[kCosmicPrescale]->p_plane0), 
					 sqrt(root2datum[kCosmicPrescale]->p_plane0->size()), 
					 sqrt(root2datum[kCosmicPrescale]->p_plane0->size()), imgbadwires0 );
	  badwires[1]->inferBadWireList( *(root2datum[kCosmicPrescale]->p_plane1), 
					 sqrt(root2datum[kCosmicPrescale]->p_plane1->size()), 
					 sqrt(root2datum[kCosmicPrescale]->p_plane1->size()), imgbadwires1 );
	  badwires[0]->applyBadWires( *(root2datum[kNu]->p_plane0),
				      sqrt(root2datum[kNu]->p_plane0->size()),
				      sqrt(root2datum[kNu]->p_plane0->size()), imgbadwires0 );
	  badwires[1]->applyBadWires( *(root2datum[kNu]->p_plane1), 
				      sqrt(root2datum[kNu]->p_plane1->size()), 
				      sqrt(root2datum[kNu]->p_plane1->size()), imgbadwires1 );
	}
	std::vector<int> imgbadwires2;
	badwires[2]->inferBadWireList( *(root2datum[kCosmicPrescale]->p_plane2), 
				       sqrt(root2datum[kCosmicPrescale]->p_plane2->size()), 
				       sqrt(root2datum[kCosmicPrescale]->p_plane2->size()), imgbadwires2 );
	badwires[2]->applyBadWires( *(root2datum[kNu]->p_plane2), 
				    sqrt(root2datum[kNu]->p_plane2->size()), 
				    sqrt(root2datum[kNu]->p_plane2->size()), imgbadwires2 );      
      }

      double mcscale_factor = rand.Gaus( 2.0, 0.05 );
      mcscale_factor = std::max( 0.0, mcscale_factor );
      root2datum[kCosmicPrescale]->overlayImage( *(root2datum[kNu]),  mcscale_factor ); // add neutrino to cosmic so that we can scale MC


      // extract image into the datum
      // randomize threshold
      float adc_threshold = rand.Gaus( 10.0, 0.5 );
      root2datum[kCosmicPrescale]->convertor.setThreshold( adc_threshold );
      plane_images = root2datum[kCosmicPrescale]->fillDatum( datum, -1, false ); // we either use cosmic or we add neutrino to cosmics (double noise?)
      filled[kNu]++;
    }//end of if Nu
    else {
      // extract BNB EXT Cosmic into datum
      // randomize threshold
      float adc_threshold = rand.Gaus( 10.0, 0.5 );
      root2datum[kCosmicBNBEXT]->convertor.setThreshold( adc_threshold );
      plane_images = root2datum[kCosmicBNBEXT]->fillDatum( datum, -1, false );
      filled[kCosmicBNBEXT]++;
    }


    // [ now do all our fancy crap to the image ]

    // PMT Weighted
    cv::Mat pmtweighted;
    if ( ftype==kNu )
      pmtweights.applyWeights( plane_images, *(root2datum[kCosmicPrescale]->p_pmt_highgain), *(root2datum[kCosmicPrescale]->p_pmt_lowgain), pmtweighted );
    else
      pmtweights.applyWeights( plane_images, *(root2datum[kCosmicBNBEXT]->p_pmt_highgain), *(root2datum[kCosmicBNBEXT]->p_pmt_lowgain), pmtweighted );

    cv::Mat chimages[3];
    cv::Mat chweighted[3];
    cv::split( plane_images, chimages );
    cv::split( pmtweighted, chweighted );
    for (int i=0; i<3; i++) {
      std::cout << "[plane " << i << "] " << chimages[i].size() << std::endl;
    }

    // HIP/MIP Filter
    cv::Mat chmip[3];
    cv::Mat chhip[3];
    for (int p=0; p<3; p++) {
      hmfilter[p]->applyFilter( chimages[p], chmip[p], chhip[p] );
    }

    // MERGE
    cv::Mat merge( plane_images.size().height, plane_images.size().width, CV_8UC(12) );
    for (int p=0; p<3; p++) {
      for (int h=0; h<plane_images.size().height; h++) {
	for (int w=0; w<plane_images.size().width; w++) {
	  merge.at< cv::Vec<uchar,12> >(h,w)[4*p + 0 ] = chimages[p].at< cv::Vec<uchar,1> >(h,w)[0];
	  merge.at< cv::Vec<uchar,12> >(h,w)[4*p + 1 ] = chweighted[p].at< cv::Vec<uchar,1> >(h,w)[0];
	  merge.at< cv::Vec<uchar,12> >(h,w)[4*p + 2 ] = chmip[p].at< cv::Vec<uchar,1> >(h,w)[0];
	  merge.at< cv::Vec<uchar,12> >(h,w)[4*p + 3 ] = chhip[p].at< cv::Vec<uchar,1> >(h,w)[0];
	}
      }
    }
    
    caffe::CVMatToDatum( merge, &datum_final );

    // set the label!!
    if ( bnblabel==larbys::util::kBackground )
      datum_final.set_label( 0 );
    else
      datum_final.set_label( 1 );

    // serialize
    std::string out;
    CHECK( datum_final.SerializeToString(&out) );
      
    // make key for db entry
    int numfilled = filled[kNu]+filled[kCosmicBNBEXT];
    std::string key_str = caffe::format_int(numfilled,10) + "_" + caffe::format_int( (int)bnblabel, 2  );
    
    // determine output stream
    Outputs_t outstream;
    if ( rand.Uniform()<train_val_split ) {
      // store in db
      txn_train->Put( key_str, out );
      outstream = kTRAIN;
    }
    else {
      txn_test->Put( key_str, out );
      outstream = kTEST;
    }

    // output annotation line
    if ( ftype==kCosmicBNBEXT ) {
      if ( outstream==kTRAIN )
	annotation_train << key_str << " " << (int)bnblabel << std::endl;
      else
	annotation_test << key_str << " " << (int)bnblabel << std::endl;
    }
    else {
      if ( outstream==kTRAIN )
	annotation_train << key_str << " " << (int)bnblabel << " ";
      else
	annotation_test << key_str << " " << (int)bnblabel << " ";
      for (int ibox=0; ibox<nbboxes; ibox++) {
	for (int iplane=0; iplane<fNumPlanes; iplane++) {
	  if ( outstream==kTRAIN )
	    annotation_train << p_LoLeft_t_plane[kNu][iplane]->at(ibox) << " " 
			     << p_LoLeft_w_plane[kNu][iplane]->at(ibox) << " "
			     << p_HiRight_t_plane[kNu][iplane]->at(ibox) << " "
			     << p_HiRight_w_plane[kNu][iplane]->at(ibox) << " ";
	  else
	    annotation_test << p_LoLeft_t_plane[kNu][iplane]->at(ibox) << " " 
			    << p_LoLeft_w_plane[kNu][iplane]->at(ibox) << " "
			    << p_HiRight_t_plane[kNu][iplane]->at(ibox) << " "
			    << p_HiRight_w_plane[kNu][iplane]->at(ibox) << " ";
	    
	}
	if ( outstream==kTRAIN )
	  annotation_train << " " << p_bblabels[ftype]->at(ibox) << std::endl;
	else
	  annotation_test << " " << p_bblabels[ftype]->at(ibox) << std::endl;
      }
    }
    
    if ( numfilled>0 && numfilled%100==0 ) {
      txn_train->Commit();
      txn_train.reset( db_train->NewTransaction() );
      txn_test->Commit();
      txn_test.reset( db_test->NewTransaction() );
      //std::cout << "process " << entry << " images" << std::endl;
    }
    
    if ( numfilled>=50000 )
      break;

    if ( ftype==kCosmicBNBEXT )
      std::cout << "Filled cosmic" << std::endl;
    else
      std::cout << "Filled neutrino" << std::endl;

  }//end of while loop
  
  // last commit
  txn_train->Commit();
  txn_train.reset( db_train->NewTransaction() );
  txn_test->Commit();
  txn_test.reset( db_test->NewTransaction() );
  
  std::cout << "FIN." << std::endl;
  std::cout << "Number of neutrinos filled: " << filled[kNu] << std::endl;
  std::cout << "Number of cosmics filled: " << filled[kCosmicBNBEXT] << std::endl;
  
  return 0;
}
