/**********
Copyright 1990 Regents of the University of California.  All rights reserved.
Author: 1985 Thomas L. Quarles
Modified: 2000 AlansFixes
**********/

#ifndef VDMOS
#define VDMOS

#include "ngspice/ifsim.h"
#include "ngspice/cktdefs.h"
#include "ngspice/gendefs.h"
#include "ngspice/complex.h"
#include "ngspice/noisedef.h"

/* declarations for VDMOSFETs */

/* indices to the array of MOSFET(1) noise sources */

enum {
    VDMOSRDNOIZ = 0,
    VDMOSRSNOIZ,
    VDMOSIDNOIZ,
    VDMOSFLNOIZ,
    VDMOSTOTNOIZ,
    /* finally, the number of noise sources */
    VDMOSNSRCS
};

/* information needed for each instance */

typedef struct sVDMOSinstance {

    struct GENinstance gen;

#define VDMOSmodPtr(inst) ((struct sVDMOSmodel *)((inst)->gen.GENmodPtr))
#define VDMOSnextInstance(inst) ((struct sVDMOSinstance *)((inst)->gen.GENnextInstance))
#define VDMOSname gen.GENname
#define VDMOSstates gen.GENstate

    const int VDMOSdNode;  /* number of the gate node of the mosfet */
    const int VDMOSgNode;  /* number of the gate node of the mosfet */
    const int VDMOSsNode;  /* number of the source node of the mosfet */
    int VDMOSdNodePrime; /* number of the internal drain node of the mosfet */
    int VDMOSsNodePrime; /* number of the internal source node of the mosfet */
    int VDMOSgNodePrime; /* number of the internal gate node of the mosfet */
    int VDIOposPrimeNode; /* number of the internal node of the bulk diode */

    double VDMOSm;   /* parallel device multiplier */

    double VDMOSl;   /* the length of the channel region */
    double VDMOSw;   /* the width of the channel region */
    double VDMOSsourceConductance;   /*conductance of source(or 0):set in setup*/
    double VDMOSdrainConductance;    /*conductance of drain(or 0):set in setup*/
    double VDMOSgateConductance;    /*conductance of gate(or 0):set in setup*/
    double VDMOSdsConductance;    /*conductance of drain to source:set in setup*/
    double VDMOStemp;    /* operating temperature of this instance */
    double VDMOSdtemp;   /* operating temperature of the instance relative to circuit temperature*/

    double VDMOStTransconductance;   /* temperature corrected transconductance*/
    double VDMOStPhi;                /* temperature corrected Phi */
    double VDMOStVto;                /* temperature corrected Vto */
    double VDMOStSatCur;             /* temperature corrected saturation Cur. */

    double VDMOSicVDS;   /* initial condition D-S voltage */
    double VDMOSicVGS;   /* initial condition G-S voltage */
    double VDMOSvon;
    double VDMOSvdsat;
    double VDMOSsourceVcrit; /* Vcrit for pos. vds */
    double VDMOSdrainVcrit;  /* Vcrit for pos. vds */
    double VDMOScd;
    double VDMOSgm;
    double VDMOSgds;
    double VDMOSf2d;
    double VDMOSf3d;
    double VDMOSf4d;
    double VDMOSf2s;
    double VDMOSf3s;
    double VDMOSf4s;

    double VDIOcap;
    double VDIOtSatCur; /* temperature corrected saturation Cur. density*/
    double VDIOinitCond;
    double VDIOtVcrit;
    double VDIOtConductance;
    double VDIOtBrkdwnV;
    double VDIOtJctCap;
    double VDIOtDepCap;     /* temperature adjusted transition point in */
                             /* the cureve matching Fc * Vj */
    double VDIOtJctPot;    /* temperature corrected Bulk potential */
    double VDIOtGradingCoeff;

    double VDIOtTransitTime;
    double VDIOtF1;
    double VDIOtF2;
    double VDIOtF3;

/*
 * naming convention:
 * x = vgs
 * y = vbs
 * z = vds
 * cdr = cdrain
 */

#define VDMOSNDCOEFFS   11

#ifndef NODISTO
    double VDMOSdCoeffs[VDMOSNDCOEFFS];
#else /* NODISTO */
    double *VDMOSdCoeffs;
#endif /* NODISTO */

#ifndef CONFIG

#define cdr_x2      VDMOSdCoeffs[0]
#define cdr_y2      VDMOSdCoeffs[1]
#define cdr_xy      VDMOSdCoeffs[2]
#define cdr_x3      VDMOSdCoeffs[3]
#define cdr_y3      VDMOSdCoeffs[4]
#define cdr_x2y     VDMOSdCoeffs[5]
#define cdr_xy2     VDMOSdCoeffs[6]
#define capgs2      VDMOSdCoeffs[7]
#define capgs3      VDMOSdCoeffs[8]
#define capgd2      VDMOSdCoeffs[9]
#define capgd3      VDMOSdCoeffs[10]

#endif

#ifndef NONOISE
    double VDMOSnVar[NSTATVARS][VDMOSNSRCS];
#else /* NONOISE */
    double **VDMOSnVar;
#endif /* NONOISE */

    int VDMOSmode;       /* device mode : 1 = normal, -1 = inverse */


    unsigned VDMOSoff:1;  /* non-zero to indicate device is off for dc analysis*/
    unsigned VDMOStempGiven :1;  /* instance temperature specified */
    unsigned VDMOSdtempGiven :1;  /* instance delta temperature specified */
    unsigned VDMOSmGiven :1;
    unsigned VDMOSlGiven :1;
    unsigned VDMOSwGiven :1;
    unsigned VDMOSdNodePrimeSet  :1;
    unsigned VDMOSsNodePrimeSet  :1;
    unsigned VDMOSicVDSGiven :1;
    unsigned VDMOSicVGSGiven :1;
    unsigned VDMOSvonGiven   :1;
    unsigned VDMOSvdsatGiven :1;
    unsigned VDMOSmodeGiven  :1;


    double *VDMOSDdPtr;      /* pointer to sparse matrix element at
                                     * (Drain node,drain node) */
    double *VDMOSGgPtr;      /* pointer to sparse matrix element at
                                     * (gate node,gate node) */
    double *VDMOSSsPtr;      /* pointer to sparse matrix element at
                                     * (source node,source node) */
    double *VDMOSDPdpPtr;    /* pointer to sparse matrix element at
                                     * (drain prime node,drain prime node) */
    double *VDMOSSPspPtr;    /* pointer to sparse matrix element at
                                     * (source prime node,source prime node) */
    double *VDMOSDdpPtr;     /* pointer to sparse matrix element at
                                     * (drain node,drain prime node) */
    double *VDMOSGdpPtr;     /* pointer to sparse matrix element at
                                     * (gate node,drain prime node) */
    double *VDMOSGspPtr;     /* pointer to sparse matrix element at
                                     * (gate node,source prime node) */
    double *VDMOSSspPtr;     /* pointer to sparse matrix element at
                                     * (source node,source prime node) */
    double *VDMOSDPspPtr;    /* pointer to sparse matrix element at
                                     * (drain prime node,source prime node) */
    double *VDMOSDPdPtr;     /* pointer to sparse matrix element at
                                     * (drain prime node,drain node) */
    double *VDMOSDPgPtr;     /* pointer to sparse matrix element at
                                     * (drain prime node,gate node) */

    double *VDMOSSPgPtr;     /* pointer to sparse matrix element at
                                     * (source prime node,gate node) */
    double *VDMOSSPsPtr;     /* pointer to sparse matrix element at
                                     * (source prime node,source node) */
    double *VDMOSSPdpPtr;    /* pointer to sparse matrix element at
                                     * (source prime node,drain prime node) */
    /* added for VDMOS */
    double *VDMOSGPgpPtr;    /* pointer to sparse matrix element at
                             * (gate prime node, gate prime node) */
    double *VDMOSGPdpPtr;    /* pointer to sparse matrix element at
                             * (gate prime node, drain prime node) */
    double *VDMOSGPspPtr;    /* pointer to sparse matrix element at
                             * (gate prime node, source prime node) */
    double *VDMOSDPgpPtr;    /* pointer to sparse matrix element at
                             * (drain prime node, gate prime node) */
    double *VDMOSSPgpPtr;    /* pointer to sparse matrix element at
                             * (source prime node, gate prime node) */
    double *VDMOSGgpPtr;    /* pointer to sparse matrix element at
                             * (gate node, gate prime node) */
    double *VDMOSGPgPtr;    /* pointer to sparse matrix element at
                             * (gate prime node, gate node) */
    double *VDMOSDsPtr;    /* pointer to sparse matrix element at
                             * (source node, drain node) */
    double *VDMOSSdPtr;    /* pointer to sparse matrix element at
                             * (drain node, source node) */
    /* bulk diode */
    double *VDIORPdPtr;    /* pointer to sparse matrix element at
                             * (diode prime node, drain node) */
    double *VDIODrpPtr;    /* pointer to sparse matrix element at
                             * (drain node, diode prime node) */
    double *VDIORPrpPtr;    /* pointer to sparse matrix element at
                             * (diode prime node, diode prime node) */
    double *VDIOSrpPtr;    /* pointer to sparse matrix element at
                            * (source node, diode prime node) */
    double *VDIORPsPtr;    /* pointer to sparse matrix element at
                            * (diode prime node, source node) */

} VDMOSinstance ;

#define VDMOSvgs VDMOSstates+ 0   /* gate-source voltage */
#define VDMOSvds VDMOSstates+ 1   /* drain-source voltage */

#define VDMOScapgs VDMOSstates+2  /* gate-source capacitor value */
#define VDMOSqgs VDMOSstates+ 3   /* gate-source capacitor charge */
#define VDMOScqgs VDMOSstates+ 4  /* gate-source capacitor current */

#define VDMOScapgd VDMOSstates+ 5 /* gate-drain capacitor value */
#define VDMOSqgd VDMOSstates+ 6   /* gate-drain capacitor charge */
#define VDMOScqgd VDMOSstates+ 7  /* gate-drain capacitor current */

#define VDIOvoltage VDMOSstates+ 8
#define VDIOcurrent VDMOSstates+ 9
#define VDIOconduct VDMOSstates+ 10
#define VDIOcapCharge VDMOSstates+ 11
#define VDIOcapCurrent VDMOSstates+ 12

#define VDMOSnumStates 13


/* per model data */

    /* NOTE:  parameters marked 'input - use xxxx' are paramters for
     * which a temperature correction is applied in VDMOStemp, thus
     * the VDMOSxxxx value in the per-instance structure should be used
     * instead in all calculations 
     */


typedef struct sVDMOSmodel {       /* model structure for a resistor */

    struct GENmodel gen;

#define VDMOSmodType gen.GENmodType
#define VDMOSnextModel(inst) ((struct sVDMOSmodel *)((inst)->gen.GENnextModel))
#define VDMOSinstances(inst) ((VDMOSinstance *)((inst)->gen.GENinstances))
#define VDMOSmodName gen.GENmodName

    int VDMOStype;       /* device type : 1 = nmos,  -1 = pmos */
    double VDMOStnom;        /* temperature at which parameters measured */
    double VDMOSdrainResistance;
    double VDMOSsourceResistance;
    double VDMOSgateResistance;
    double VDMOSqsResistance;
    double VDMOSqsVoltage;
    double VDMOStransconductance;    /* input - use tTransconductance */
    double VDMOSoxideCapFactor;
    double VDMOSvt0; /* input - use tVto */
    double VDMOSphi; /* input - use tPhi */
    double VDMOSlambda;
    double VDMOStheta;
    double VDMOSfNcoef;
    double VDMOSfNexp;

    double VDMOScgdmin;
    double VDMOScgdmax;
    double VDMOSa;
    double VDMOScgs;
    double VDMOSsubsl;
    double VDMOSsubshift;
    double VDMOSksubthres;
    double VDMOSmtr;

    /* bulk diode */
    double VDIOjunctionCap;   /* input - use tCj */
    double VDIOjunctionPot;    /* input - use tBulkPot */
    double VDIOdepletionCapCoeff;
    double VDIOjctSatCur;   /* input - use tSatCur */
    double VDMOSDbv;
    double VDMOSDibv;
    double VDIObrkdEmissionCoeff;
    double VDIOresistance;
    double VDIOresistTemp1;
    double VDIOresistTemp2;
    double VDIOconductance;
    double VDMOSrds;
    double VDMOSDn;
    double VDIOtransitTime;
    double VDIOtranTimeTemp1;
    double VDIOtranTimeTemp2;
    double VDMOSDeg;
    double VDMOSDxti;
    double VDIOgradCoeff;
    double VDIOgradCoeffTemp1;
    double VDIOgradCoeffTemp2;

    unsigned VDMOStypeGiven  :1;
    unsigned VDIOjctSatCurGiven :1;
    unsigned VDMOSdrainResistanceGiven   :1;
    unsigned VDMOSsourceResistanceGiven  :1;
    unsigned VDMOSgateResistanceGiven    :1;
    unsigned VDMOSqsResistanceGiven    :1;
    unsigned VDMOSqsVoltageGiven    :1;
    unsigned VDMOSqsGiven    :1;
    unsigned VDMOStransconductanceGiven  :1;
    unsigned VDMOSvt0Given   :1;
    unsigned VDIOgradCoeffGiven    :1;
    unsigned VDIOdepletionCapCoeffGiven :1;
    unsigned VDMOSphiGiven   :1;
    unsigned VDMOSlambdaGiven    :1;
    unsigned VDMOSthetaGiven    :1;
    unsigned VDMOStnomGiven  :1;
    unsigned VDMOSfNcoefGiven  :1;
    unsigned VDMOSfNexpGiven   :1;

    unsigned VDMOScgdminGiven   :1;
    unsigned VDMOScgdmaxGiven   :1;
    unsigned VDMOScgsGiven   :1;
    unsigned VDMOSaGiven   :1;
    unsigned VDMOSsubslGiven   :1;
    unsigned VDMOSsubshiftGiven   :1;
    unsigned VDMOSksubthresGiven :1;
    unsigned VDMOSmtrGiven   :1;

    unsigned VDMOSDbvGiven   :1;
    unsigned VDMOSDibvGiven   :1;
    unsigned VDIOjunctionCapGiven :1;
    unsigned VDIOjunctionPotGiven :1;
    unsigned VDIObrkdEmissionCoeffGiven :1;
    unsigned VDIOresistanceGiven :1;
    unsigned VDMOSrdsGiven   :1;
    unsigned VDMOSDnGiven   :1;
    unsigned VDIOtransitTimeGiven :1;
    unsigned VDMOSDegGiven   :1;
    unsigned VDMOSDxtiGiven   :1;

} VDMOSmodel;

#ifndef NMOS
#define NMOS 1
#define PMOS -1
#endif /*NMOS*/

/* device parameters */
enum {
    VDMOS_W = 1,
    VDMOS_L,
    VDMOS_OFF,
    VDMOS_IC,
    VDMOS_IC_VDS,
    VDMOS_IC_VGS,
    VDMOS_CG,
    VDMOS_CS,
    VDMOS_POWER,
    VDMOS_TEMP,
    VDMOS_M,
    VDMOS_DTEMP,
};

/* model paramerers */
enum {
    VDMOS_MOD_VTO = 101,
    VDMOS_MOD_KP,
    VDMOS_MOD_PHI,
    VDMOS_MOD_LAMBDA,
    VDMOS_MOD_THETA,
    VDMOS_MOD_RD,
    VDMOS_MOD_RS,
    VDMOS_MOD_RG,
    VDMOS_MOD_RQ,
    VDMOS_MOD_VQ,
    VDMOS_MOD_IS,
    VDMOS_MOD_VJ,
    VDMOS_MOD_CJ,
    VDMOS_MOD_MJ,
    VDMOS_MOD_FC,
    VDMOS_MOD_NMOS,
    VDMOS_MOD_PMOS,
    VDMOS_MOD_TNOM,
    VDMOS_MOD_KF,
    VDMOS_MOD_AF,
    VDMOS_MOD_TYPE,
    VDMOS_MOD_DMOS,
    VDMOS_MOD_CGDMIN,
    VDMOS_MOD_CGDMAX,
    VDMOS_MOD_A,
    VDMOS_MOD_CGS,
    VDMOS_MOD_RB,
    VDMOS_MOD_MTRIODE,
    VDMOS_MOD_SUBSLOPE,
    VDMOS_MOD_SUBSHIFT,
    VDMOS_MOD_KSUBTHRES,
    VDMOS_MOD_BV,
    VDMOS_MOD_IBV,
    VDMOS_MOD_NBV,
    VDMOS_MOD_RDS,
    VDMOS_MOD_N,
    VDMOS_MOD_TT,
    VDMOS_MOD_EG,
    VDMOS_MOD_XTI,
};

/* device questions */
enum {
    VDMOS_CAPGS = 201,
    VDMOS_CAPGD,
    VDMOS_CAPDS,
    VDMOS_DNODE,
    VDMOS_GNODE,
    VDMOS_SNODE,
    VDMOS_DNODEPRIME,
    VDMOS_SNODEPRIME,
    VDMOS_SOURCECONDUCT,
    VDMOS_DRAINCONDUCT,
    VDMOS_VON,
    VDMOS_VDSAT,
    VDMOS_SOURCEVCRIT,
    VDMOS_DRAINVCRIT,
    VDMOS_CD,
    VDMOS_GM,
    VDMOS_GDS,
    VDMOS_VGS,
    VDMOS_VDS,
    VDMOS_QGS,
    VDMOS_CQGS,
    VDMOS_QGD,
    VDMOS_CQGD,
    VDMOS_SOURCERESIST,
    VDMOS_DRAINRESIST,
};

/* model questions */

#include "vdmosext.h"

#endif /*VDMOS*/

