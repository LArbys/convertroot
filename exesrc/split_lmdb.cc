#include <iostream>
#include "TRandom3.h"

// This utility exists to split a sample into 
// a train and test set.
// we throw using a random number generator. 
// this way we can create many different cross-validation sets.

// Caffe and LMDB
#include "lmdb.h"
#include "caffe/util/db.hpp"

// Boost
#include "boost/scoped_ptr.hpp"

// LArbys
#include "BBAnnotationParser.hh"

int main( int nargs, char** argv ) {

  std::string input_lmdb = argv[1];
  std::string input_annote = argv[2];
  std::string output_train = argv[3];
  std::string output_train_annote = argv[4];
  std::string output_test = argv[5];
  std::string output_test_annote = argv[6];

  std::string FLAGS_backend = "lmdb";

  long SEED = 123456;
  TRandom3 rand( SEED );
  float train_fraction = 0.7;

  // source database
  boost::scoped_ptr<caffe::db::DB> db_src(caffe::db::GetDB(FLAGS_backend));
  db_src->Open(input_lmdb.c_str(), caffe::db::READ);
  boost::scoped_ptr<caffe::db::Cursor> cursor_src(db_src->NewCursor());

  // output train database
  boost::scoped_ptr<caffe::db::DB> db_train(caffe::db::GetDB(FLAGS_backend));
  db_train->Open(output_train.c_str(), caffe::db::NEW);
  boost::scoped_ptr<caffe::db::Transaction> txn_train(db_train->NewTransaction());
  
  // output test database
  boost::scoped_ptr<caffe::db::DB> db_test(caffe::db::GetDB(FLAGS_backend));
  db_test->Open(output_test.c_str(), caffe::db::NEW);
  boost::scoped_ptr<caffe::db::Transaction> txn_test(db_test->NewTransaction());

  cursor_src->SeekToFirst();

  // annotations
  larbys::util::BBAnnotationParser annotations( input_annote );
  annotations.parse();

  std::ofstream train_annotation( output_train_annote.c_str() );
  std::ofstream test_annotation(  output_test_annote.c_str() );

  int n_test_filled = 0;
  int n_train_filled = 0;


  while ( cursor_src->valid() ) {

    // read in annotation

    if (  rand.Uniform() < train_fraction ) {
      txn_train->Put( cursor_src->key(), cursor_src->value() );
      annotations.writeInfo( cursor_src->key(), train_annotation );
      n_train_filled++;
    }
    else {
      txn_test->Put( cursor_src->key(), cursor_src->value() );
      annotations.writeInfo( cursor_src->key(), test_annotation );
      n_test_filled++;
    }

    if ( n_train_filled>0 && n_train_filled%100==0 ) {
      txn_train->Commit();
      txn_train.reset( db_train->NewTransaction() );
    }
    else if ( n_test_filled>0 && n_test_filled%100==0 ) {
      txn_test->Commit();
      txn_test.reset( db_test->NewTransaction() );
    }

    cursor_src->Next();
    
    if ( (n_test_filled+n_train_filled)%100==0 )
      std::cout << "Processed: " << n_test_filled+n_train_filled << std::endl;
    if ( n_test_filled+n_train_filled>20 )
      break;
  }
  
  // save last transactions
  txn_train->Commit();
  txn_test->Commit();


  return 0;
};
