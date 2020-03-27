/**********
Copyright 1990 Regents of the University of California.  All rights reserved.
Author: 1985 Thomas L. Quarles
Modified: 2000 AlansFixes
**********/

/* load the VDMOS device structure with those pointers needed later
 * for fast matrix loading
 */

#include "ngspice/ngspice.h"
#include "ngspice/smpdefs.h"
#include "ngspice/cktdefs.h"
#include "vdmosdefs.h"
#include "ngspice/sperror.h"
#include "ngspice/suffix.h"

int
VDMOSsetup(SMPmatrix *matrix, GENmodel *inModel, CKTcircuit *ckt,
    int *states)
{
    VDMOSmodel *model = (VDMOSmodel *)inModel;
    VDMOSinstance *here;
    int error;
    CKTnode *tmp;

    /*  loop through all the VDMOS device models */
    for (; model != NULL; model = VDMOSnextModel(model)) {

        if (!model->VDMOStypeGiven) {
            model->VDMOStype = NMOS;
        }
        if (!model->VDIOjctSatCurGiven) {
            model->VDIOjctSatCur = 1e-14;
        }
        if (!model->VDMOStransconductanceGiven) {
            model->VDMOStransconductance = 1;
        }
        if (!model->VDMOSvt0Given) {
            model->VDMOSvt0 = 0;
        }
        if (!model->VDIOjunctionPotGiven) {
            model->VDIOjunctionPot = .8;
        }
        if (!model->VDIOgradCoeffGiven) {
            model->VDIOgradCoeff = .5;
        }
        if (!model->VDIOdepletionCapCoeffGiven) {
            model->VDIOdepletionCapCoeff = .5;
        }
        if (!model->VDMOSphiGiven) {
            model->VDMOSphi = .6;
        }
        if (!model->VDMOSlambdaGiven) {
            model->VDMOSlambda = 0;
        }
        if (!model->VDMOSthetaGiven) {
            model->VDMOStheta = 0;
        }
        if (!model->VDMOSfNcoefGiven) {
            model->VDMOSfNcoef = 0;
        }
        if (!model->VDMOSfNexpGiven) {
            model->VDMOSfNexp = 1;
        }
        if (!model->VDMOScgdminGiven) {
            model->VDMOScgdmin = 0;
        }
        if (!model->VDMOScgdmaxGiven) {
            model->VDMOScgdmax = 0;
        }
        if (!model->VDMOScgsGiven) {
            model->VDMOScgs = 0;
        }
        if (!model->VDMOSaGiven) {
            model->VDMOSa = 1.;
        }
        if (!model->VDMOSsubslGiven) {
            model->VDMOSsubsl = 0;
        }
        if (!model->VDMOSsubshiftGiven) {
            model->VDMOSsubshift = 0;
        }
        if (!model->VDMOSksubthresGiven) {
            model->VDMOSksubthres = 0;
        }
        if (!model->VDMOSmtrGiven) {
            model->VDMOSmtr = 1.;
        }
        if (!model->VDMOSDbvGiven) {
            model->VDMOSDbv = 1.0e30;
        }
        if (!model->VDMOSDibvGiven) {
            model->VDMOSDibv = 1.0e-10;
        }
        if (!model->VDIObrkdEmissionCoeffGiven) {
            model->VDIObrkdEmissionCoeff = 1.;
        }
        if (!model->VDMOSrdsGiven) {
            model->VDMOSrds = 1.0e30;
        }
        if (!model->VDMOSDnGiven) {
            model->VDMOSDn = 1.;
        }
        if (!model->VDIOtransitTimeGiven) {
            model->VDIOtransitTime = 0.;
        }
        if (!model->VDMOSDegGiven) {
            model->VDMOSDeg = 1.11;
        }

        /* loop through all the instances of the model */
        for (here = VDMOSinstances(model); here != NULL;
            here = VDMOSnextInstance(here)) {

            /* allocate a chunk of the state vector */
            here->VDMOSstates = *states;
            *states += VDMOSnumStates;

            if (!here->VDMOSicVDSGiven) {
                here->VDMOSicVDS = 0;
            }
            if (!here->VDMOSicVGSGiven) {
                here->VDMOSicVGS = 0;
            }
            if (!here->VDMOSvdsatGiven) {
                here->VDMOSvdsat = 0;
            }
            if (!here->VDMOSvonGiven) {
                here->VDMOSvon = 0;
            }
            if (model->VDMOSdrainResistance != 0) {
                if (here->VDMOSdNodePrime == 0) {
                    error = CKTmkVolt(ckt, &tmp, here->VDMOSname, "drain");
                    if (error) return(error);
                    here->VDMOSdNodePrime = tmp->number;

                    if (ckt->CKTcopyNodesets) {
                        CKTnode *tmpNode;
                        IFuid tmpName;

                        if (CKTinst2Node(ckt, here, 1, &tmpNode, &tmpName) == OK) {
                            if (tmpNode->nsGiven) {
                                tmp->nodeset = tmpNode->nodeset;
                                tmp->nsGiven = tmpNode->nsGiven;
                            }
                        }
                    }
                }

            }
            else {
                here->VDMOSdNodePrime = here->VDMOSdNode;
            }

            if (model->VDMOSsourceResistance != 0) {
                if (here->VDMOSsNodePrime == 0) {
                    error = CKTmkVolt(ckt, &tmp, here->VDMOSname, "source");
                    if (error) return(error);
                    here->VDMOSsNodePrime = tmp->number;

                    if (ckt->CKTcopyNodesets) {
                        CKTnode *tmpNode;
                        IFuid tmpName;

                        if (CKTinst2Node(ckt, here, 3, &tmpNode, &tmpName) == OK) {
                            if (tmpNode->nsGiven) {
                                tmp->nodeset = tmpNode->nodeset;
                                tmp->nsGiven = tmpNode->nsGiven;
                            }
                        }
                    }

                }
            }
            else {
                here->VDMOSsNodePrime = here->VDMOSsNode;
            }

            if (model->VDMOSgateResistance != 0 ) {
                if (here->VDMOSgNodePrime == 0) {
                    error = CKTmkVolt(ckt, &tmp, here->VDMOSname, "gate");
                    if (error) return(error);
                    here->VDMOSgNodePrime = tmp->number;

                    if (ckt->CKTcopyNodesets) {
                        CKTnode *tmpNode;
                        IFuid tmpName;

                        if (CKTinst2Node(ckt, here, 3, &tmpNode, &tmpName) == OK) {
                            if (tmpNode->nsGiven) {
                                tmp->nodeset = tmpNode->nodeset;
                                tmp->nsGiven = tmpNode->nsGiven;
                            }
                        }
                    }
                }
            }
            else {
                here->VDMOSgNodePrime = here->VDMOSgNode;
            }

            if (model->VDIOresistance != 0 ) {
                if (here->VDIOposPrimeNode == 0) {
                    error = CKTmkVolt(ckt, &tmp, here->VDMOSname, "bulk diode");
                    if (error) return(error);
                    here->VDIOposPrimeNode = tmp->number;

                    if (ckt->CKTcopyNodesets) {
                        CKTnode *tmpNode;
                        IFuid tmpName;

                        if (CKTinst2Node(ckt, here, 3, &tmpNode, &tmpName) == OK) {
                            if (tmpNode->nsGiven) {
                                tmp->nodeset = tmpNode->nodeset;
                                tmp->nsGiven = tmpNode->nsGiven;
                            }
                        }
                    }
                }
            }
            else {
                here->VDIOposPrimeNode = here->VDMOSsNode;
            }


            /* macro to make elements with built in test for out of memory */
#define TSTALLOC(ptr,first,second) \
do { if((here->ptr = SMPmakeElt(matrix, here->first, here->second)) == NULL){\
    return(E_NOMEM);\
} } while(0)

            TSTALLOC(VDMOSDdPtr, VDMOSdNode, VDMOSdNode);
            TSTALLOC(VDMOSGgPtr, VDMOSgNode, VDMOSgNode);
            TSTALLOC(VDMOSSsPtr, VDMOSsNode, VDMOSsNode);
            TSTALLOC(VDMOSDPdpPtr, VDMOSdNodePrime, VDMOSdNodePrime);
            TSTALLOC(VDMOSSPspPtr, VDMOSsNodePrime, VDMOSsNodePrime);
            TSTALLOC(VDMOSGPgpPtr, VDMOSgNodePrime, VDMOSgNodePrime);
            TSTALLOC(VDMOSDdpPtr, VDMOSdNode, VDMOSdNodePrime);
            TSTALLOC(VDMOSGPdpPtr, VDMOSgNodePrime, VDMOSdNodePrime);
            TSTALLOC(VDMOSGPspPtr, VDMOSgNodePrime, VDMOSsNodePrime);
            TSTALLOC(VDMOSSspPtr, VDMOSsNode, VDMOSsNodePrime);
            TSTALLOC(VDMOSDPspPtr, VDMOSdNodePrime, VDMOSsNodePrime);
            TSTALLOC(VDMOSDPdPtr, VDMOSdNodePrime, VDMOSdNode);
            TSTALLOC(VDMOSDPgpPtr, VDMOSdNodePrime, VDMOSgNodePrime);
            TSTALLOC(VDMOSSPgpPtr, VDMOSsNodePrime, VDMOSgNodePrime);
            TSTALLOC(VDMOSSPsPtr, VDMOSsNodePrime, VDMOSsNode);
            TSTALLOC(VDMOSSPdpPtr, VDMOSsNodePrime, VDMOSdNodePrime);

            TSTALLOC(VDMOSGgpPtr, VDMOSgNode, VDMOSgNodePrime);
            TSTALLOC(VDMOSGPgPtr, VDMOSgNodePrime, VDMOSgNode);

            TSTALLOC(VDMOSDsPtr, VDMOSdNode, VDMOSsNode);
            TSTALLOC(VDMOSSdPtr, VDMOSsNode, VDMOSdNode);

            TSTALLOC(VDIORPdPtr, VDIOposPrimeNode, VDMOSdNode);
            TSTALLOC(VDIODrpPtr, VDMOSdNode, VDIOposPrimeNode);
            TSTALLOC(VDIOSrpPtr, VDMOSsNode, VDIOposPrimeNode);
            TSTALLOC(VDIORPsPtr, VDIOposPrimeNode, VDMOSsNode);
            TSTALLOC(VDIORPrpPtr, VDIOposPrimeNode, VDIOposPrimeNode);
        }
    }
    return(OK);
}

int
VDMOSunsetup(GENmodel *inModel, CKTcircuit *ckt)
{
    VDMOSmodel *model;
    VDMOSinstance *here;

    for (model = (VDMOSmodel *)inModel; model != NULL;
        model = VDMOSnextModel(model))
    {
        for (here = VDMOSinstances(model); here != NULL;
            here = VDMOSnextInstance(here))
        {
            if (here->VDMOSsNodePrime > 0
                && here->VDMOSsNodePrime != here->VDMOSsNode)
                CKTdltNNum(ckt, here->VDMOSsNodePrime);
            here->VDMOSsNodePrime = 0;

            if (here->VDMOSdNodePrime > 0
                && here->VDMOSdNodePrime != here->VDMOSdNode)
                CKTdltNNum(ckt, here->VDMOSdNodePrime);
            here->VDMOSdNodePrime = 0;

            if (here->VDMOSgNodePrime > 0
                && here->VDMOSgNodePrime != here->VDMOSgNode)
                CKTdltNNum(ckt, here->VDMOSgNodePrime);
            here->VDMOSgNodePrime = 0;

            if (here->VDIOposPrimeNode > 0
                && here->VDIOposPrimeNode != here->VDMOSsNode)
                CKTdltNNum(ckt, here->VDIOposPrimeNode);
            here->VDIOposPrimeNode = 0;
        }
    }
    return OK;
}
