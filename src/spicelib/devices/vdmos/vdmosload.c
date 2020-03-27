/**********
Copyright 1990 Regents of the University of California.  All rights reserved.
Author: 1985 Thomas L. Quarles
Modified: 2000 AlansFixes
VDMOS: 2018 Holger Vogt
**********/

#include "ngspice/ngspice.h"
#include "ngspice/cktdefs.h"
#include "ngspice/devdefs.h"
#include "vdmosdefs.h"
#include "ngspice/trandefs.h"
#include "ngspice/const.h"
#include "ngspice/sperror.h"
#include "ngspice/suffix.h"

static double
cweakinv2(double sl, double shift, double vgst, double vds, double lambda, double beta, double vt, double mtr, double theta);


int
VDMOSload(GENmodel *inModel, CKTcircuit *ckt)
/* actually load the current value into the
 * sparse matrix previously provided
 */
{
    VDMOSmodel *model = (VDMOSmodel *)inModel;
    VDMOSinstance *here;
    double Beta;
    double DrainSatCur;
    double SourceSatCur;
    double arg;
    double cdhat;
    double cdrain;
    double cdreq;
    double ceq;
    double ceqgd;
    double ceqgs;
    double delvds;
    double delvgd;
    double delvgs;
    double gcgd;
    double gcgs;
    double geq;
    double sarg;
    double vds;
    double vdsat;
    double vgd1;
    double vgd;
    double vgdo;
    double vgs1;
    double vgs;
    double von;
    double vt;
#ifndef PREDICTOR
    double xfact = 0.0;
#endif
    int xnrm;
    int xrev;
    double capgs = 0.0;   /* total gate-source capacitance */
    double capgd = 0.0;   /* total gate-drain capacitance */
    int Check;
    int error;

    double CGBdummy;

    /*  loop through all the VDMOS device models */
    for (; model != NULL; model = VDMOSnextModel(model)) {
        /* VDMOS capacitance parameters */
        const double cgdmin = model->VDMOScgdmin;
        const double cgdmax = model->VDMOScgdmax;
        const double a = model->VDMOSa;
        const double cgs = model->VDMOScgs;

        /* loop through all the instances of the model */
        for (here = VDMOSinstances(model); here != NULL;
                here = VDMOSnextInstance(here)) {

            vt = CONSTKoverQ * here->VDMOStemp;
            Check = 1;

            /* first, we compute a few useful values - these could be
             * pre-computed, but for historical reasons are still done
             * here.  They may be moved at the expense of instance size
             */

            DrainSatCur = here->VDMOSm * here->VDMOStSatCur;
            SourceSatCur = here->VDMOSm * here->VDMOStSatCur;
            Beta = here->VDMOStTransconductance * here->VDMOSm *
                   here->VDMOSw / here->VDMOSl;

            /*
             * ok - now to do the start-up operations
             *
             * we must get values for vbs, vds, and vgs from somewhere
             * so we either predict them or recover them from last iteration
             * These are the two most common cases - either a prediction
             * step or the general iteration step and they
             * share some code, so we put them first - others later on
             */

            if ((ckt->CKTmode & (MODEINITFLOAT | MODEINITPRED | MODEINITSMSIG
                                 | MODEINITTRAN)) ||
                    ((ckt->CKTmode & MODEINITFIX) && (!here->VDMOSoff))) {
#ifndef PREDICTOR
                if (ckt->CKTmode & (MODEINITPRED | MODEINITTRAN)) {

                    /* predictor step */

                    xfact = ckt->CKTdelta / ckt->CKTdeltaOld[1];
                    *(ckt->CKTstate0 + here->VDMOSvgs) =
                        *(ckt->CKTstate1 + here->VDMOSvgs);
                    vgs = (1 + xfact)* (*(ckt->CKTstate1 + here->VDMOSvgs))
                          - (xfact * (*(ckt->CKTstate2 + here->VDMOSvgs)));
                    *(ckt->CKTstate0 + here->VDMOSvds) =
                        *(ckt->CKTstate1 + here->VDMOSvds);
                    vds = (1 + xfact)* (*(ckt->CKTstate1 + here->VDMOSvds))
                          - (xfact * (*(ckt->CKTstate2 + here->VDMOSvds)));
                } else {
#endif /* PREDICTOR */

                    /* general iteration */

                    vgs = model->VDMOStype * (
                              *(ckt->CKTrhsOld + here->VDMOSgNodePrime) -
                              *(ckt->CKTrhsOld + here->VDMOSsNodePrime));
                    vds = model->VDMOStype * (
                              *(ckt->CKTrhsOld + here->VDMOSdNodePrime) -
                              *(ckt->CKTrhsOld + here->VDMOSsNodePrime));
#ifndef PREDICTOR
                }
#endif /* PREDICTOR */

                /* now some common crunching for some more useful quantities */

                vgd = vgs - vds;
                vgdo = *(ckt->CKTstate0 + here->VDMOSvgs) -
                       *(ckt->CKTstate0 + here->VDMOSvds);
                delvgs = vgs - *(ckt->CKTstate0 + here->VDMOSvgs);
                delvds = vds - *(ckt->CKTstate0 + here->VDMOSvds);
                delvgd = vgd - vgdo;

                /* these are needed for convergence testing */

                if (here->VDMOSmode >= 0) {
                    cdhat =
                            here->VDMOScd
                          + here->VDMOSgm * delvgs
                          + here->VDMOSgds * delvds;
                } else {
                    cdhat =
                            here->VDMOScd
                          - here->VDMOSgm * delvgd
                          + here->VDMOSgds * delvds;
                }

#ifndef NOBYPASS
                /* now lets see if we can bypass (ugh) */
                if ((!(ckt->CKTmode &
                        (MODEINITPRED | MODEINITTRAN | MODEINITSMSIG))) &&
                        (ckt->CKTbypass) &&
                        (fabs(delvgs) < (ckt->CKTreltol *
                                         MAX(fabs(vgs),
                                             fabs(*(ckt->CKTstate0 +
                                                    here->VDMOSvgs))) +
                                         ckt->CKTvoltTol)) &&
                        (fabs(delvds) < (ckt->CKTreltol *
                                         MAX(fabs(vds),
                                             fabs(*(ckt->CKTstate0 +
                                                    here->VDMOSvds))) +
                                         ckt->CKTvoltTol)) &&
                        (fabs(cdhat - here->VDMOScd) < (ckt->CKTreltol *
                                                        MAX(fabs(cdhat),
                                                                fabs(here->VDMOScd)) +
                                                        ckt->CKTabstol))) {
                    /* bypass code */
                    /* nothing interesting has changed since last
                     * iteration on this device, so we just
                     * copy all the values computed last iteration out
                     * and keep going
                     */
                    vgs = *(ckt->CKTstate0 + here->VDMOSvgs);
                    vds = *(ckt->CKTstate0 + here->VDMOSvds);
                    vgd = vgs - vds;
                    cdrain = here->VDMOSmode * (here->VDMOScd);
                    if (ckt->CKTmode & (MODETRAN | MODETRANOP)) {
                        capgs = (*(ckt->CKTstate0 + here->VDMOScapgs) +
                                 *(ckt->CKTstate1 + here->VDMOScapgs));
                        capgd = (*(ckt->CKTstate0 + here->VDMOScapgd) +
                                 *(ckt->CKTstate1 + here->VDMOScapgd));

                    }
                    goto bypass;
                }
#endif /*NOBYPASS*/


                /* ok - bypass is out, do it the hard way */

                von = model->VDMOStype * here->VDMOSvon;

#ifndef NODELIMITING
                /*
                 * limiting
                 *  we want to keep device voltages from changing
                 * so fast that the exponentials churn out overflows
                 * and similar rudeness
                 */

                if (*(ckt->CKTstate0 + here->VDMOSvds) >= 0) {
                    vgs = DEVfetlim(vgs, *(ckt->CKTstate0 + here->VDMOSvgs)
                                    , von);
                    vds = vgs - vgd;
                    vds = DEVlimvds(vds, *(ckt->CKTstate0 + here->VDMOSvds));
                    vgd = vgs - vds;
                } else {
                    vgd = DEVfetlim(vgd, vgdo, von);
                    vds = vgs - vgd;
                    if (!(ckt->CKTfixLimit)) {
                        vds = -DEVlimvds(-vds, -(*(ckt->CKTstate0 +
                                                   here->VDMOSvds)));
                    }
                    vgs = vgd + vds;
                }
#endif /*NODELIMITING*/


            } else {

                /* ok - not one of the simple cases, so we have to
                 * look at all of the possibilities for why we were
                 * called.  We still just initialize the three voltages
                 */

                if ((ckt->CKTmode & MODEINITJCT) && !here->VDMOSoff) {
                    vds = model->VDMOStype * here->VDMOSicVDS;
                    vgs = model->VDMOStype * here->VDMOSicVGS;
                    if ((vds == 0) && (vgs == 0) &&
                            ((ckt->CKTmode &
                              (MODETRAN | MODEDCOP | MODEDCTRANCURVE)) ||
                             (!(ckt->CKTmode & MODEUIC)))) {
                        vgs = model->VDMOStype * here->VDMOStVto;
                        vds = 0;
                    }
                } else {
                    vgs = vds = 0;
                }
            }


            /*
             * now all the preliminaries are over - we can start doing the
             * real work
             */
            vgd = vgs - vds;


            /* now to determine whether the user was able to correctly
             * identify the source and drain of his device
             */
            if (vds >= 0) {
                /* normal mode */
                here->VDMOSmode = 1;
            } else {
                /* inverse mode */
                here->VDMOSmode = -1;
            }

            {
                /*
                 *     this block of code evaluates the drain current and its
                 *     derivatives using the shichman-hodges model and the
                 *     charges associated with the gate, channel and bulk for
                 *     mosfets
                 *
                 */

                /* the following 2 variables are local to this code block until
                 * it is obvious that they can be made global
                 */
                double betap;
                double vgst;

                von = (model->VDMOSvt0*model->VDMOStype);
                vgst = (here->VDMOSmode == 1 ? vgs : vgd) - von;
                vdsat = MAX(vgst, 0);
                if (model->VDMOSksubthresGiven) {
                /* Alternative simple weak inversion model, according to https://www.anasoft.co.uk/MOS1Model.htm
                 * Scale the voltage overdrive vgst logarithmically in weak inversion.
                 * Best fits LTSPICE curves with shift=0
                 * Drain current including subthreshold current */

                    double slope = model->VDMOSksubthres;
                    double lambda = model->VDMOSlambda;
                    double theta = model->VDMOStheta;
                    double shift = model->VDMOSsubshift;
                    double mtr = model->VDMOSmtr;

                    /* scale vds with mtr (except with lambda) */
                    double vdss = vds*mtr*here->VDMOSmode;
                    double t0 = 1 + lambda*vds;
                    double t1 = 1 + theta*vgs;
                    betap = Beta*t0/t1;
                    double dbetapdvgs = -Beta*theta*t0/(t1*t1);
                    double dbetapdvds = Beta*lambda/t1;

                    double t2 = exp((vgst-shift)/slope);
                    vgst = slope * log(1 + t2);
                    double dvgstdvgs = t2/(t2+1);

                    if (vgst <= vdss) {
                        /* saturation region */
                        cdrain = betap*vgst*vgst*.5;
                        here->VDMOSgm = betap*vgst*dvgstdvgs + 0.5*dbetapdvgs*vgst*vgst;
                        here->VDMOSgds = .5*dbetapdvds*vgst*vgst;
                    }
                    else {
                        /* linear region */
                        cdrain = betap * vdss * (vgst - .5 * vdss);
                        here->VDMOSgm = betap*vdss*dvgstdvgs + vdss*dbetapdvgs*(vgst-.5*vdss);
                        here->VDMOSgds = vdss*dbetapdvds*(vgst-.5*vdss) + betap*mtr*(vgst-.5*vdss) - .5*vdss*betap*mtr;
                    }
                }
                else if (model->VDMOSsubslGiven) {
                    /* numerical differentiation for gd and gm with a delta of 2 mV */
                    double vdsm = vds * here->VDMOSmode;
                    double delta = 0.001;
                    cdrain = cweakinv2(model->VDMOSsubsl, model->VDMOSsubshift, vgst, vdsm, model->VDMOSlambda,
                        Beta, vt, model->VDMOSmtr, model->VDMOStheta);
                    /* gd */
                    double vds1 = vdsm + delta;
                    double cdrp = cweakinv2(model->VDMOSsubsl, model->VDMOSsubshift, vgst, vds1, model->VDMOSlambda,
                        Beta, vt, model->VDMOSmtr, model->VDMOStheta);
                    vds1 = vdsm - delta;
                    double cdrm = cweakinv2(model->VDMOSsubsl, model->VDMOSsubshift, vgst, vds1, model->VDMOSlambda,
                        Beta, vt, model->VDMOSmtr, model->VDMOStheta);
                    here->VDMOSgds = (cdrp - cdrm) / (2. * delta);
                    /* gm */
                    double vgst1 = vgst + delta;
                    cdrp = cweakinv2(model->VDMOSsubsl, model->VDMOSsubshift, vgst1, vdsm, model->VDMOSlambda,
                        Beta, vt, model->VDMOSmtr, model->VDMOStheta);
                    vgst1 = vgst - delta;
                    cdrm = cweakinv2(model->VDMOSsubsl, model->VDMOSsubshift, vgst1, vdsm, model->VDMOSlambda,
                        Beta, vt, model->VDMOSmtr, model->VDMOStheta);
                    here->VDMOSgm = (cdrp - cdrm) / (2. * delta);
                } else {
                    double onfg, fgate, Betam, dfgdvg;
                    onfg = 1.0+model->VDMOStheta*vgst;
                    fgate = 1.0/onfg;
                    Betam = Beta * fgate;
                    dfgdvg = -model->VDMOStheta*fgate*fgate;
                    if (vgst <= 0) {
                        /*
                         *     cutoff region
                         */
                        cdrain = 0;
                        here->VDMOSgm = 0;
                        here->VDMOSgds = 0;
                    } else {
                        /* scale vds with mtr */
                        double mtr = model->VDMOSmtr;
                        betap = Betam*(1 + model->VDMOSlambda*(vds*here->VDMOSmode));
                        if (vgst <= (vds * here->VDMOSmode) * mtr) {
                            /*
                             *     saturation region
                             */
                            cdrain = betap*vgst*vgst*.5;
                            here->VDMOSgm = betap*vgst * fgate + dfgdvg * cdrain;
                            here->VDMOSgds = model->VDMOSlambda*Betam*vgst*vgst*.5;
                        } else {
                            /*
                             *     linear region
                             */
                            cdrain = betap * (vds * here->VDMOSmode) * mtr *
                                     (vgst - .5 * (vds*here->VDMOSmode) * mtr);
                            here->VDMOSgm = betap * (vds * here->VDMOSmode) * mtr  * fgate + dfgdvg * cdrain;
                            here->VDMOSgds = betap * (vgst - (vds * here->VDMOSmode) * mtr) +
                                             model->VDMOSlambda * Betam *
                                             (vds * here->VDMOSmode) * mtr *
                                             (vgst - .5 * (vds * here->VDMOSmode) * mtr);
                        }
                    }
                }
            }


            /* now deal with n vs p polarity */

            here->VDMOSvon = model->VDMOStype * von;
            here->VDMOSvdsat = model->VDMOStype * vdsat;

            /*
             *  COMPUTE EQUIVALENT DRAIN CURRENT SOURCE
             */
            here->VDMOScd = here->VDMOSmode * cdrain;

            /* save things away for next time */

            *(ckt->CKTstate0 + here->VDMOSvgs) = vgs;
            *(ckt->CKTstate0 + here->VDMOSvds) = vds;


            /*
             * vdmos capacitor model
             */
            if (ckt->CKTmode & (MODETRAN | MODETRANOP | MODEINITSMSIG)) {
                /*
                 * calculate gate - drain, gate - source capacitors
                 * drain-source capacitor is evaluated with the bulk diode below
                 */
                /*
                 * this just evaluates at the current time,
                 * expects you to remember values from previous time
                 * returns 1/2 of non-constant portion of capacitance
                 * you must add in the other half from previous time
                 * and the constant part
                 */
                DevCapVDMOS(vgd, cgdmin, cgdmax, a, cgs,
                            (ckt->CKTstate0 + here->VDMOScapgs),
                            (ckt->CKTstate0 + here->VDMOScapgd),
                             &CGBdummy);

                vgs1 = *(ckt->CKTstate1 + here->VDMOSvgs);
                vgd1 = vgs1 - *(ckt->CKTstate1 + here->VDMOSvds);
                if (ckt->CKTmode & (MODETRANOP | MODEINITSMSIG)) {
                    capgs = 2 * *(ckt->CKTstate0 + here->VDMOScapgs);
                    capgd = 2 * *(ckt->CKTstate0 + here->VDMOScapgd);
                } else {
                    capgs = (*(ckt->CKTstate0 + here->VDMOScapgs) +
                             *(ckt->CKTstate1 + here->VDMOScapgs));
                    capgd = (*(ckt->CKTstate0 + here->VDMOScapgd) +
                             *(ckt->CKTstate1 + here->VDMOScapgd));
                }
                /*

                */

#ifndef PREDICTOR
                if (ckt->CKTmode & (MODEINITPRED | MODEINITTRAN)) {
                    *(ckt->CKTstate0 + here->VDMOSqgs) =
                        (1 + xfact) * *(ckt->CKTstate1 + here->VDMOSqgs)
                        - xfact * *(ckt->CKTstate2 + here->VDMOSqgs);
                    *(ckt->CKTstate0 + here->VDMOSqgd) =
                        (1 + xfact) * *(ckt->CKTstate1 + here->VDMOSqgd)
                        - xfact * *(ckt->CKTstate2 + here->VDMOSqgd);
                } else {
#endif /*PREDICTOR*/
                    if (ckt->CKTmode & MODETRAN) {
                        *(ckt->CKTstate0 + here->VDMOSqgs) = (vgs - vgs1)*capgs +
                                                             *(ckt->CKTstate1 + here->VDMOSqgs);
                        *(ckt->CKTstate0 + here->VDMOSqgd) = (vgd - vgd1)*capgd +
                                                             *(ckt->CKTstate1 + here->VDMOSqgd);
                    } else {
                        /* TRANOP only */
                        *(ckt->CKTstate0 + here->VDMOSqgs) = vgs*capgs;
                        *(ckt->CKTstate0 + here->VDMOSqgd) = vgd*capgd;
                    }
#ifndef PREDICTOR
                }
#endif /*PREDICTOR*/
            }
#ifndef NOBYPASS
bypass :
#endif

            if ((ckt->CKTmode & (MODEINITTRAN)) ||
                    (!(ckt->CKTmode & (MODETRAN)))) {
                /*
                 *  initialize to zero charge conductances
                 *  and current
                 */
                gcgs = 0;
                ceqgs = 0;
                gcgd = 0;
                ceqgd = 0;
            } else {
                if (capgs == 0) *(ckt->CKTstate0 + here->VDMOScqgs) = 0;
                if (capgd == 0) *(ckt->CKTstate0 + here->VDMOScqgd) = 0;
                /*
                 *    calculate equivalent conductances and currents for
                 *    meyer"s capacitors
                 */
                error = NIintegrate(ckt, &gcgs, &ceqgs, capgs, here->VDMOSqgs);
                if (error) return(error);
                error = NIintegrate(ckt, &gcgd, &ceqgd, capgd, here->VDMOSqgd);
                if (error) return(error);
                ceqgs = ceqgs - gcgs*vgs + ckt->CKTag[0] *
                        *(ckt->CKTstate0 + here->VDMOSqgs);
                ceqgd = ceqgd - gcgd*vgd + ckt->CKTag[0] *
                        *(ckt->CKTstate0 + here->VDMOSqgd);
            }

            /*
             *  load current vector
             */
            if (here->VDMOSmode >= 0) {
                xnrm = 1;
                xrev = 0;
                cdreq = model->VDMOStype*(cdrain - here->VDMOSgds*vds -
                                          here->VDMOSgm*vgs);
            } else {
                xnrm = 0;
                xrev = 1;
                cdreq = -(model->VDMOStype)*(cdrain - here->VDMOSgds*(-vds) -
                                             here->VDMOSgm*vgd);
            }
            *(ckt->CKTrhs + here->VDMOSgNodePrime) -=
                (model->VDMOStype * (ceqgs + ceqgd));
            *(ckt->CKTrhs + here->VDMOSdNodePrime) +=
                (-cdreq + model->VDMOStype * ceqgd);
            *(ckt->CKTrhs + here->VDMOSsNodePrime) +=
                cdreq + model->VDMOStype * ceqgs;


            /* quasi saturation
             * according to Vincenzo d'Alessandro's Quasi-Saturation Model, simplified:
             V. D'Alessandro, F. Frisina, N. Rinaldi: A New SPICE Model of VDMOS Transistors
             Including Thermal and Quasi-saturation Effects, 9th European Conference on Power
             Electronics and applications (EPE), Graz, Austria, August 2001, pp. P.1 − P.10.
             */
            if (model->VDMOSqsGiven && (here->VDMOSmode == 1)) {
                double vdsn = model->VDMOStype * (
                    *(ckt->CKTrhsOld + here->VDMOSdNode) -
                    *(ckt->CKTrhsOld + here->VDMOSsNode));
                double rd = model->VDMOSdrainResistance + model->VDMOSqsResistance *
                    (vdsn / (vdsn + fabs(model->VDMOSqsVoltage)));
                here->VDMOSdrainConductance = 1 / rd;
            }


            /*
             *  load y matrix
             */
            *(here->VDMOSDdPtr) += (here->VDMOSdrainConductance + here->VDMOSdsConductance);
            *(here->VDMOSGgPtr) += (here->VDMOSgateConductance); //((gcgd + gcgs + gcgb));
            *(here->VDMOSSsPtr) += (here->VDMOSsourceConductance + here->VDMOSdsConductance);
            *(here->VDMOSDPdpPtr) +=
                (here->VDMOSdrainConductance + here->VDMOSgds +
                 xrev*(here->VDMOSgm) + gcgd);
            *(here->VDMOSSPspPtr) +=
                (here->VDMOSsourceConductance + here->VDMOSgds +
                 xnrm*(here->VDMOSgm) + gcgs);
            *(here->VDMOSGPgpPtr) +=
                (here->VDMOSgateConductance) + (gcgd + gcgs);
            *(here->VDMOSGgpPtr) += (-here->VDMOSgateConductance);
            *(here->VDMOSDdpPtr) += (-here->VDMOSdrainConductance);
            *(here->VDMOSGPgPtr) += (-here->VDMOSgateConductance);
            *(here->VDMOSGPdpPtr) -= gcgd;
            *(here->VDMOSGPspPtr) -= gcgs;
            *(here->VDMOSSspPtr) += (-here->VDMOSsourceConductance);
            *(here->VDMOSDPdPtr) += (-here->VDMOSdrainConductance);
            *(here->VDMOSDPgpPtr) += ((xnrm - xrev)*here->VDMOSgm - gcgd);
            *(here->VDMOSDPspPtr) += (-here->VDMOSgds - xnrm*
                                      (here->VDMOSgm));
            *(here->VDMOSSPgpPtr) += (-(xnrm - xrev)*here->VDMOSgm - gcgs);
            *(here->VDMOSSPsPtr) += (-here->VDMOSsourceConductance);
            *(here->VDMOSSPdpPtr) += (-here->VDMOSgds - xrev*
                                      (here->VDMOSgm));

            *(here->VDMOSDsPtr) += (-here->VDMOSdsConductance);
            *(here->VDMOSSdPtr) += (-here->VDMOSdsConductance);


            /* bulk diode model
             * Delivers reverse conduction and forward breakdown
             * of VDMOS transistor
             */

            double vd;      /* current diode voltage */
            double vdtemp;
            double vte;
            double vtebrk, vbrknp;
            double cd, cdb, csat, cdeq;
            double czero;
            double czof2;
            double capd;
            double gd, gdb, gspr;
            double delvd;   /* change in diode voltage temporary */
            double diffcharge, deplcharge, diffcap, deplcap;
            double evd, evrev;
#ifndef NOBYPASS
            double tol;     /* temporary for tolerence calculations */
#endif

            cd = 0.0;
            cdb = 0.0;
            gd = 0.0;
            gdb = 0.0;
            csat = here->VDIOtSatCur;
            gspr = here->VDIOtConductance;
            vte = model->VDMOSDn * vt;
            vtebrk = model->VDIObrkdEmissionCoeff * vt;
            vbrknp = here->VDIOtBrkdwnV;

            Check = 1;
            if (ckt->CKTmode & MODEINITSMSIG) {
                vd = *(ckt->CKTstate0 + here->VDIOvoltage);
            } else if (ckt->CKTmode & MODEINITTRAN) {
                vd = *(ckt->CKTstate1 + here->VDIOvoltage);
            } else if ((ckt->CKTmode & MODEINITJCT) &&
                       (ckt->CKTmode & MODETRANOP) && (ckt->CKTmode & MODEUIC)) {
                vd = here->VDIOinitCond;
            } else if (ckt->CKTmode & MODEINITJCT) {
                vd = here->VDIOtVcrit;
            } else {
#ifndef PREDICTOR
                if (ckt->CKTmode & MODEINITPRED) {
                    *(ckt->CKTstate0 + here->VDIOvoltage) =
                        *(ckt->CKTstate1 + here->VDIOvoltage);
                    vd = DEVpred(ckt, here->VDIOvoltage);
                    *(ckt->CKTstate0 + here->VDIOcurrent) =
                        *(ckt->CKTstate1 + here->VDIOcurrent);
                    *(ckt->CKTstate0 + here->VDIOconduct) =
                        *(ckt->CKTstate1 + here->VDIOconduct);
                } else {
#endif /* PREDICTOR */
                    vd = model->VDMOStype * (*(ckt->CKTrhsOld + here->VDIOposPrimeNode) -
                                             *(ckt->CKTrhsOld + here->VDMOSdNode));
#ifndef PREDICTOR
                }
#endif /* PREDICTOR */
                delvd = vd - *(ckt->CKTstate0 + here->VDIOvoltage);
                cdhat = *(ckt->CKTstate0 + here->VDIOcurrent) +
                        *(ckt->CKTstate0 + here->VDIOconduct) * delvd;
                /*
                *   bypass if solution has not changed
                */
#ifndef NOBYPASS
                if ((!(ckt->CKTmode & MODEINITPRED)) && (ckt->CKTbypass)) {
                    tol = ckt->CKTvoltTol + ckt->CKTreltol*
                          MAX(fabs(vd), fabs(*(ckt->CKTstate0 + here->VDIOvoltage)));
                    if (fabs(delvd) < tol) {
                        tol = ckt->CKTreltol* MAX(fabs(cdhat),
                                                  fabs(*(ckt->CKTstate0 + here->VDIOcurrent))) +
                              ckt->CKTabstol;
                        if (fabs(cdhat - *(ckt->CKTstate0 + here->VDIOcurrent))
                                < tol) {
                            vd = *(ckt->CKTstate0 + here->VDIOvoltage);
                            cd = *(ckt->CKTstate0 + here->VDIOcurrent);
                            gd = *(ckt->CKTstate0 + here->VDIOconduct);
                            goto load;
                        }
                    }
                }
#endif /* NOBYPASS */
                /*
                *   limit new junction voltage
                */
                if ((model->VDMOSDbvGiven) &&
                        (vd < MIN(0, -vbrknp + 10 * vtebrk))) {
                    vdtemp = -(vd + vbrknp);
                    vdtemp = DEVpnjlim(vdtemp,
                                       -(*(ckt->CKTstate0 + here->VDIOvoltage) +
                                         vbrknp), vtebrk,
                                       here->VDIOtVcrit, &Check);
                    vd = -(vdtemp + vbrknp);
                } else {
                    vd = DEVpnjlim(vd, *(ckt->CKTstate0 + here->VDIOvoltage),
                                   vte, here->VDIOtVcrit, &Check);
                }
            }
            /*
            *   compute dc current and derivatives
            */
            if (vd >= -3 * vte) {                 /* bottom current forward */

                evd = exp(vd / vte);
                cdb = csat*(evd - 1);
                gdb = csat*evd / vte;

            } else if ((!(model->VDMOSDbvGiven)) ||
                       vd >= -vbrknp) { /* reverse */

                arg = 3 * vte / (vd*CONSTe);
                arg = arg * arg * arg;
                cdb = -csat*(1 + arg);
                gdb = csat * 3 * arg / vd;

            } else {                          /* breakdown */

                evrev = exp(-(vbrknp + vd) / vtebrk);
                cdb = -csat*evrev;
                gdb = csat*evrev / vtebrk;

            }


            cd = cdb;
            gd = gdb;

            gd = gd + ckt->CKTgmin;
            cd = cd + ckt->CKTgmin*vd;

            if ((ckt->CKTmode & (MODEDCTRANCURVE | MODETRAN | MODEAC | MODEINITSMSIG)) ||
                    ((ckt->CKTmode & MODETRANOP) && (ckt->CKTmode & MODEUIC))) {
                /*
                *   charge storage elements
                */
                czero = here->VDIOtJctCap;
                if (vd < here->VDIOtDepCap) {
                    arg = 1 - vd / here->VDIOtJctPot;
                    sarg = exp(-here->VDIOtGradingCoeff*log(arg));
                    deplcharge = here->VDIOtJctPot*czero*(1 - arg*sarg) / (1 - here->VDIOtGradingCoeff);
                    deplcap = czero*sarg;
                } else {
                    czof2 = czero / here->VDIOtF2;
                    deplcharge = czero*here->VDIOtF1 + czof2*(here->VDIOtF3*(vd - here->VDIOtDepCap) +
                                 (here->VDIOtGradingCoeff / (here->VDIOtJctPot + here->VDIOtJctPot))*(vd*vd - here->VDIOtDepCap*here->VDIOtDepCap));
                    deplcap = czof2*(here->VDIOtF3 + here->VDIOtGradingCoeff*vd / here->VDIOtJctPot);
                }
                diffcharge = here->VDIOtTransitTime*cdb;
                *(ckt->CKTstate0 + here->VDIOcapCharge) =
                    diffcharge +  deplcharge;

                diffcap = here->VDIOtTransitTime*gdb;
                capd = diffcap + deplcap;

                here->VDIOcap = capd;

                /*
                *   store small-signal parameters
                */
                if ((!(ckt->CKTmode & MODETRANOP)) ||
                        (!(ckt->CKTmode & MODEUIC))) {
                    if (ckt->CKTmode & MODEINITSMSIG) {
                        *(ckt->CKTstate0 + here->VDIOcapCurrent) = capd;

                        continue;
                    }

                    /*
                    *   transient analysis
                    */

                    if (ckt->CKTmode & MODEINITTRAN) {
                        *(ckt->CKTstate1 + here->VDIOcapCharge) =
                            *(ckt->CKTstate0 + here->VDIOcapCharge);
                    }
                    error = NIintegrate(ckt, &geq, &ceq, capd, here->VDIOcapCharge);
                    if (error) return(error);
                    gd = gd + geq;
                    cd = cd + *(ckt->CKTstate0 + here->VDIOcapCurrent);
                    if (ckt->CKTmode & MODEINITTRAN) {
                        *(ckt->CKTstate1 + here->VDIOcapCurrent) =
                            *(ckt->CKTstate0 + here->VDIOcapCurrent);
                    }
                }
            }

            /*
            *   check convergence
            */

            if (Check == 1) {
                ckt->CKTnoncon++;
                ckt->CKTtroubleElt = (GENinstance *)here;
            }

            *(ckt->CKTstate0 + here->VDIOvoltage) = vd;
            *(ckt->CKTstate0 + here->VDIOcurrent) = cd;
            *(ckt->CKTstate0 + here->VDIOconduct) = gd;

#ifndef NOBYPASS
load :
#endif
            /*
            *   load current vector
            */
            cdeq = cd - gd*vd;
            if (model->VDMOStype == 1) {
                *(ckt->CKTrhs + here->VDMOSdNode) += cdeq;
                *(ckt->CKTrhs + here->VDIOposPrimeNode) -= cdeq;
            } else {
                *(ckt->CKTrhs + here->VDMOSdNode) -= cdeq;
                *(ckt->CKTrhs + here->VDIOposPrimeNode) += cdeq;
            }

            /*
            *   load matrix
            */
            *(here->VDMOSSsPtr) += gspr;
            *(here->VDMOSDdPtr) += gd;
            *(here->VDIORPrpPtr) += (gd + gspr);
            *(here->VDIOSrpPtr) -= gspr;
            *(here->VDIODrpPtr) -= gd;
            *(here->VDIORPsPtr) -= gspr;
            *(here->VDIORPdPtr) -= gd;
        }
    }
    return(OK);
}


/* scaling function, sine function interpolating between 0 and 1
 * nf2: empirical setting of sine 'speed'
 */

static double
scalef(double nf2, double vgst)
{
    double vgstsin = vgst / nf2;
    if (vgstsin > 1)
        return 1;
    else if (vgstsin < -1)
        return 0;
    else
        return 0.5 * sin(vgstsin * M_PI / 2) + 0.5;
}


/* Calculate D/S current including weak inversion.
 * Uses a single function covering weak-moderate-stong inversion, as well
 * as linear and saturation regions, with an interpolation method according to
 * Tvividis, McAndrew: "Operation and Modeling of the MOS Transistor", Oxford, 2011, p. 209.
 * A single parameter n sets the slope of the weak inversion current. The weak inversion
 * current is independent from vds, as in long channel devices.
 * The following modification has been added for VDMOS compatibility:
 * n and lambda are depending on vgst with a sine function interpolating between 0 and 1.
 */

static double
cweakinv2(double slope, double shift, double vgst, double vds, double lambda, double beta, double vt, double mtr, double theta)
{
    double betam = beta / (1.0+theta*vgst);
    vgst += shift * (1 - scalef(0.5, vgst));
    double n = slope / 2.3 / 0.0256; /* Tsividis, p. 208 */
    double n1 = n + (1 - n) * scalef(0.7, vgst); /* n < n1 < 1 */
    double first = log(1 + exp(vgst / (2 * n1 * vt)));
    double second = log(1 + exp((vgst - vds * mtr * n1) / (2 * n1 * vt)));
    double cds =
        betam * n1 * 2 * vt * vt * (1 + scalef(1, vgst) * lambda * vds) *
        (first * first - second * second);
    return cds;
}

