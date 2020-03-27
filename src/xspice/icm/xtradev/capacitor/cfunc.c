#line 1 "./xtradev/capacitor/cfunc.mod"
#include "ngspice/cm.h"
extern void cm_capacitor(Mif_Private_t *);
#line 1 "./xtradev/capacitor/cfunc.mod"
/* ===========================================================================
FILE    capacitor/cfunc.mod

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

    This file contains the definition of a capacitor code model
    with voltage type initial conditions.

INTERFACES

    cm_capacitor()

REFERENCED FILES

    None.

NON-STANDARD FEATURES

    None.

=========================================================================== */


#define VC  0


void cm_capacitor (Mif_Private_t *mif_private)
{
    Complex_t   ac_gain;
    double      partial;
    double      ramp_factor;
    double      *vc;


    /* Get the ramp factor from the .option ramptime */
    ramp_factor = cm_analog_ramp_factor();

    /* Initialize/access instance specific storage for capacitor voltage */
    if(mif_private->circuit.init) {
        cm_analog_alloc(VC, sizeof(double));
        vc = (double *) cm_analog_get_ptr(VC, 0);
        *vc = mif_private->param[1]->element[0].rvalue * cm_analog_ramp_factor();
    }
    else {
        vc = (double *) cm_analog_get_ptr(VC, 0);
    }

    /* Compute the output */
    if(mif_private->circuit.anal_type == DC) {
        mif_private->conn[0]->port[0]->output.rvalue = mif_private->param[1]->element[0].rvalue * ramp_factor;
         mif_private->conn[0]->port[0]->partial[0].port[0] = 0.0;
    }
    else if(mif_private->circuit.anal_type == AC) {
        ac_gain.real = 0.0;
        ac_gain.imag = -1.0 / mif_private->circuit.frequency / mif_private->param[0]->element[0].rvalue;
         mif_private->conn[0]->port[0]->ac_gain[0].port[0] = ac_gain;
    }
    else if(mif_private->circuit.anal_type == TRANSIENT) {
        if(ramp_factor < 1.0) {
            *vc = mif_private->param[1]->element[0].rvalue * ramp_factor;
            mif_private->conn[0]->port[0]->output.rvalue = *vc;
             mif_private->conn[0]->port[0]->partial[0].port[0] = 0.0;
        }
        else {
            cm_analog_integrate(mif_private->conn[0]->port[0]->input.rvalue / mif_private->param[0]->element[0].rvalue, vc, &partial);
            partial /= mif_private->param[0]->element[0].rvalue;
            mif_private->conn[0]->port[0]->output.rvalue = *vc;
             mif_private->conn[0]->port[0]->partial[0].port[0] = partial;
        }
    }
}

