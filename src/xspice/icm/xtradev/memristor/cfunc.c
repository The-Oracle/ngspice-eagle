#line 1 "./xtradev/memristor/cfunc.mod"
#include "ngspice/cm.h"
extern void cm_memristor(Mif_Private_t *);
#line 1 "./xtradev/memristor/cfunc.mod"
/* ===========================================================================
FILE    memristor/cfunc.mod

MEMBER OF process XSPICE

Copyright 2012
Holger Vogt
Mülheim, Germany
All Rights Reserved



AUTHORS

    6/08/2012  Holger Vogt

MODIFICATIONS

    <date> <person name> <nature of modifications>

SUMMARY

    This file contains the definition of a memristor code model
    with threshold according to
	Y. V. Pershin, M. Di Ventra: "SPICE model of memristive devices with threshold", 
    arXiv:1204.2600v1 [physics.comp-ph] 12 Apr 2012, 
    http://arxiv.org/pdf/1204.2600.pdf.
	
	** Experimental, still to be tested in circuits !! **

	dc and ac simulation just return rinit.

INTERFACES

    cm_memristor()

REFERENCED FILES

    None.

NON-STANDARD FEATURES

    None.

=========================================================================== */

/*=== INCLUDE FILES ====================*/

#include <math.h>


#define RV  0

/* model parameters */
double alpha, beta, vt;
/* forward of window function */
double f1(double y); 

void cm_memristor (Mif_Private_t *mif_private)
{
    Complex_t   ac_gain;
    double      partial;
    double      int_value;
    double      *rval;
	double      inpdiff;
	
    /* get the parameters */
	alpha = mif_private->param[3]->element[0].rvalue;
    beta = mif_private->param[4]->element[0].rvalue;
    vt = mif_private->param[5]->element[0].rvalue;	

    /* Initialize/access instance specific storage for resistance value */
    if(mif_private->circuit.init) {
        cm_analog_alloc(RV, sizeof(double));
        rval = (double *) cm_analog_get_ptr(RV, 0);
        *rval = mif_private->param[2]->element[0].rvalue;
    }
    else {
        rval = (double *) cm_analog_get_ptr(RV, 0);
    }

    /* Compute the output */
    if(mif_private->circuit.anal_type == TRANSIENT) {
	    /* input the voltage across the terminals */
	    inpdiff = f1(mif_private->conn[0]->port[0]->input.rvalue);
	    if ((inpdiff > 0) && (*rval < mif_private->param[1]->element[0].rvalue))
		    int_value = inpdiff;
		else if  ((inpdiff < 0) && (*rval > mif_private->param[0]->element[0].rvalue))
		    int_value = inpdiff;
        else
		    int_value = 0.0;
		/* integrate the new resistance */	
        cm_analog_integrate(int_value, rval, &partial);
		/* output the current */
        mif_private->conn[0]->port[0]->output.rvalue = mif_private->conn[0]->port[0]->input.rvalue / *rval;
        /* This does work, but is questionable */
         mif_private->conn[0]->port[0]->partial[0].port[0] = partial;
        /* This may be a (safe?) replacement, but in fact is not
        so good	at high voltage	(at strong non-linearity)
		cm_analog_auto_partial();*/
    }
    else if(mif_private->circuit.anal_type == AC) {
        ac_gain.real = 1/ *rval;
        ac_gain.imag = 0.0;
         mif_private->conn[0]->port[0]->ac_gain[0].port[0] = ac_gain;
    }
	else
	    mif_private->conn[0]->port[0]->output.rvalue = mif_private->conn[0]->port[0]->input.rvalue / *rval;
}

/* the window function */
double f1(double y) {
    return (beta*y+0.5*(alpha-beta)*(fabs(y+vt)-fabs(y-vt)));
}	


