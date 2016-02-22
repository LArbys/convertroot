#ifndef __BNBLabels__
#define __BNBLabels__

#include <string>
#include <assert.h>

namespace larbys {
  namespace util {
    
    typedef enum { kBackground=0, 
	kNumuCCQE, kNumuCCRES, kNumuCCOther,
	kNueCCQE, kNueCCRES, kNueCCOther,
	kNCQE, kNCRES, kNCOther } BNBLabels_t;
	
    std::string getBNBLabelName( BNBLabels_t label ) {
      switch (label) {
      case kBackground:
	return "background";
	break;
      case kNumuCCQE:
	return "numuccqe";
	break;
      case kNumuCCRES:
	return "numuccres";
	break;
      case kNumuCCOther:
	return "numuccother";
	break;
      case kNueCCQE:
	return "nueccqe";
	break;
      case kNueCCRES:
	return "nueccres";
	break;
      case kNueCCOther:
	return "nueccother";
	break;
      case kNCQE:
	return "ncqe";
	break;
      case kNCRES:
	return "ncres";
	break;
      case kNCOther:
	return "ncother";
	break;
      };
    };

    BNBLabels_t getBNBLabelName( std::string name ) {    
      if ( name=="background" )
	return kBackground;
      else if ( name.substr(0,4)=="numu" ) {
	if ( name=="numuccqe" ) return kNumuCCQE;
	else if (name=="numuccres" ) return kNumuCCRES;
	else if (name=="numuccother") return kNumuCCOther;
      }
      else if ( name.substr(0,3)=="nue" ) {
	if ( name=="nueccqe" ) return kNueCCQE;
	else if ( name=="nueccres" ) return kNueCCRES;
	else if ( name=="nueccother" ) return kNueCCOther;
      }
      else if ( name.substr(0,2)=="nc" ) {
	if (name=="ncqe") return kNCQE;
	else if (name=="ncres") return kNCRES;
	else if (name=="ncother") return kNCOther;
      }
      // should never get here.
      assert(false);
    };

    BNBLabels_t labelFromRootVars( int mode, int nuscatter, int flavor ) {

      if ( nuscatter==0 ) {
	if (flavor==14 || flavor==-14) {
	  switch (mode) {
	  case 0:
	    return kNumuCCQE;
	    break;
	  case 1:
	    return kNumuCCRES;
	    break;
	  default:
	    return kNumuCCOther;
	    break;
	  };
	}
	else if ( flavor==12 || flavor==-12 ) {
	  switch (mode) {
	  case 0:
	    return kNueCCQE;
	    break;
	  case 1:
	    return kNueCCRES;
	    break;
	  default:
	    return kNueCCOther;
	    break;
	  };
	}
      }//end of nuscatter==0 (CC)
      else {
	switch (mode) {
	case 0:
	  return kNCQE;
	  break;
	case 1:
	  return kNCRES;
	  break;
	default:
	  return kNCOther;
	  break;
	};
      }
    };
    
    assert(false);
  }
}

#endif
