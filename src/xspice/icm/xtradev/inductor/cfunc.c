#line 1 "./xtradev/inductor/cfunc.mod"
#include "ngspice/cm.h"
extern void cm_inductor(Mif_Private_t *);
#line 1 "./xtradev/inductor/cfunc.mod"
/* ===========================================================================
FILE    cfunc.mod

MEMBER OF process XSPICE

Copyright 1991
Georgia Tech Research Corporation
Atlanta, Georgia 30332
All Rights Reserved

PROJECT A-8503

AUTHORS

    9/12/91  Bill Kuhn

MODIFICATIONS

    <date> <person name> <nature of modifications>

SUMMARY

    This file contains the definition of an inductor code model
    with current initial conditions.

INTERFACES

    cm_inductor()

REFERENCED FILES

    None.

NON-STANDARD FEATURES

    None.

=========================================================================== */


#define LI  0


void cm_inductor (Mif_Private_t *mif_private)
{
    Complex_t   ac_gain;
    double      partial;
    double      ramp_factor;
    double      *li;

    /* Get the ramp factor from the .option ramptime */
    ramp_factor = cm_analog_ramp_factor();

    /* Initialize/access instance specific storage for capacitor voltage */
    if(mif_private->circuit.init) {
        cm_analog_alloc(LI, sizeof(double));
        li = (double *) cm_analog_get_ptr(LI, 0);
        *li = mif_private->param[1]->element[0].rvalue * ramp_factor;
    }
    else {
        li = (double *) cm_analog_get_ptr(LI, 0);
    }

    /* Compute the output */
    if(mif_private->circuit.anal_type == DC) {
        mif_private->conn[0]->port[0]->output.rvalue = mif_private->param[1]->element[0].rvalue * ramp_factor;
         mif_private->conn[0]->port[0]->partial[0].port[0] = 0.0;
    }
    else if(mif_private->circuit.anal_type == AC) {
        ac_gain.real = 0.0;
        ac_gain.imag = 1.0 * mif_private->circuit.frequency * mif_private->param[0]->element[0].rvalue;
         mif_private->conn[0]->port[0]->ac_gain[0].port[0] = ac_gain;
    }
    else if(mif_private->circuit.anal_type == TRANSIENT) {
        if(ramp_factor < 1.0) {
            *li = mif_private->param[1]->element[0].rvalue * ramp_factor;
            mif_private->conn[0]->port[0]->output.rvalue = *li;
             mif_private->conn[0]->port[0]->partial[0].port[0] = 0.0;
        }
        else {
            cm_analog_integrate(mif_private->conn[0]->port[0]->input.rvalue / mif_private->param[0]->element[0].rvalue, li, &partial);
            partial /= mif_private->param[0]->element[0].rvalue;
            mif_private->conn[0]->port[0]->output.rvalue = *li;
             mif_private->conn[0]->port[0]->partial[0].port[0] = partial;
        }
    }
}

