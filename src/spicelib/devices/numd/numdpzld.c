/**********
Copyright 1992 Regents of the University of California.  All rights reserved.
Author:	1987 Kartikeya Mayaram, U. C. Berkeley CAD Group
**********/

#include "ngspice/ngspice.h"
#include "ngspice/cktdefs.h"
#include "ngspice/complex.h"
#include "ngspice/sperror.h"
#include "numddefs.h"
#include "../../../ciderlib/oned/onedext.h"
#include "ngspice/cidersupt.h"
#include "ngspice/suffix.h"

/* External Declarations */
extern int ONEacDebug;

int
NUMDpzLoad(GENmodel *inModel, CKTcircuit *ckt, SPcomplex *s)
{
  register NUMDmodel *model = (NUMDmodel *) inModel;
  register NUMDinstance *inst;
  SPcomplex y;
  double startTime;

  NG_IGNORE(ckt);

  /* loop through all the diode models */
  for (; model != NULL; model = NUMDnextModel(model)) {
    FieldDepMobility = model->NUMDmodels->MODLfieldDepMobility;
    Srh = model->NUMDmodels->MODLsrh;
    Auger = model->NUMDmodels->MODLauger;
    AvalancheGen = model->NUMDmodels->MODLavalancheGen;
    AcAnalysisMethod = model->NUMDmethods->METHacAnalysisMethod;
    MobDeriv = model->NUMDmethods->METHmobDeriv;
    ONEacDebug = model->NUMDoutputs->OUTPacDebug;

    for (inst = NUMDinstances(model); inst != NULL;
         inst = NUMDnextInstance(inst)) {

      startTime = SPfrontEnd->IFseconds();
      /* Get Temp.-Dep. Global Parameters */
      GLOBgetGlobals(&(inst->NUMDglobals));

      NUMDys(inst->NUMDpDevice, s, &y);

      *(inst->NUMDposPosPtr) += y.real;
      *(inst->NUMDposPosPtr + 1) += y.imag;
      *(inst->NUMDnegNegPtr) += y.real;
      *(inst->NUMDnegNegPtr + 1) += y.imag;
      *(inst->NUMDnegPosPtr) -= y.real;
      *(inst->NUMDnegPosPtr + 1) -= y.imag;
      *(inst->NUMDposNegPtr) -= y.real;
      *(inst->NUMDposNegPtr + 1) -= y.imag;

      inst->NUMDpDevice->pStats->totalTime[STAT_AC] +=
	  SPfrontEnd->IFseconds() - startTime;
    }
  }
  return (OK);
}
