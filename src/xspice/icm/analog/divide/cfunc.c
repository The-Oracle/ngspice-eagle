#line 1 "./analog/divide/cfunc.mod"
#include "ngspice/cm.h"
extern void cm_divide(Mif_Private_t *);
#line 1 "./analog/divide/cfunc.mod"
/*.......1.........2.........3.........4.........5.........6.........7.........8
================================================================================

FILE divide/cfunc.mod

Copyright 1991
Georgia Tech Research Corporation, Atlanta, Ga. 30332
All Rights Reserved

PROJECT A-8503-405
               

AUTHORS                      

    6 Jun 1991     Jeffrey P. Murray


MODIFICATIONS   

     2 Oct 1991    Jeffrey P. Murray
                                   

SUMMARY

    This file contains the model-specific routines used to
    functionally describe the divide code model.


INTERFACES       

    FILE                 ROUTINE CALLED     

    CMutil.c             void cm_smooth_corner(); 


REFERENCED FILES

    Inputs from and outputs to ARGS structure.
                     

NON-STANDARD FEATURES

    NONE

===============================================================================*/

/*=== INCLUDE FILES ====================*/

#include <math.h>

                                      

/*=== CONSTANTS ========================*/




/*=== MACROS ===========================*/



  
/*=== LOCAL VARIABLES & TYPEDEFS =======*/                         


    
           
/*=== FUNCTION PROTOTYPE DEFINITIONS ===*/




                   
/*==============================================================================

FUNCTION void cm_divide()

AUTHORS                      

     2 Oct 1991     Jeffrey P. Murray

MODIFICATIONS   

    NONE

SUMMARY

    This function implements the divide code model.

INTERFACES       

    FILE                 ROUTINE CALLED     

    CMutil.c             void cm_smooth_corner(); 

RETURNED VALUE
    
    Returns inputs and outputs via ARGS structure.

GLOBAL VARIABLES
    
    NONE

NON-STANDARD FEATURES

    NONE

==============================================================================*/


/*=== CM_DIVIDE ROUTINE ===*/


void cm_divide(Mif_Private_t *mif_private)  

{
    double den_lower_limit;  /* denominator lower limit */
	double den_domain;       /* smoothing range for the lower limit */
	double threshold_upper;  /* value above which smoothing occurs */
	double threshold_lower;  /* value below which smoothing occurs */
    double numerator;        /* numerator input */
	double denominator;      /* denominator input */
	double limited_den;      /* denominator value if limiting is needed */   
	double den_partial;      /* partial of the output wrt denominator */
	double out_gain;         /* output gain */
    double num_gain;         /* numerator gain */
	double den_gain;         /* denominator gain */
    
    Mif_Complex_t ac_gain;



    /* Retrieve frequently used parameters... */

    den_lower_limit = mif_private->param[4]->element[0].rvalue;
    den_domain = mif_private->param[5]->element[0].rvalue;
    out_gain = mif_private->param[7]->element[0].rvalue;
    num_gain = mif_private->param[1]->element[0].rvalue;
    den_gain = mif_private->param[3]->element[0].rvalue;
                                                    

    if (mif_private->param[6]->element[0].bvalue == MIF_TRUE)    /* Set domain to absolute value */
        den_domain = den_domain * den_lower_limit;

    threshold_upper = den_lower_limit +   /* Set Upper Threshold */
                         den_domain;

    threshold_lower = den_lower_limit -   /* Set Lower Threshold */
                         den_domain;

    numerator = (mif_private->conn[0]->port[0]->input.rvalue + mif_private->param[0]->element[0].rvalue) * num_gain; 
    denominator = (mif_private->conn[1]->port[0]->input.rvalue + mif_private->param[2]->element[0].rvalue) * den_gain; 

    if ((denominator < threshold_upper) && (denominator >= 0)) {  /* Need to limit den...*/

        if (denominator > threshold_lower)   /* Parabolic Region */
            cm_smooth_corner(denominator,den_lower_limit,
                        den_lower_limit,den_domain,0.0,1.0,
                        &limited_den,&den_partial);

        else {                            /* Hard-Limited Region */
            limited_den = den_lower_limit;
            den_partial = 0.0;        
        }
    }
	else
    if ((denominator > -threshold_upper) && (denominator < 0)) {  /* Need to limit den...*/
        if (denominator < -threshold_lower)   /* Parabolic Region */
            cm_smooth_corner(denominator,-den_lower_limit,
                        -den_lower_limit,den_domain,0.0,1.0,
                        &limited_den,&den_partial);

        else {                            /* Hard-Limited Region */
            limited_den = -den_lower_limit;
            den_partial = 0.0;        
        }
    }
    else {                         /* No limiting needed */
        limited_den = denominator;
        den_partial = 1.0;
    }

    if (mif_private->circuit.anal_type != MIF_AC) {

        mif_private->conn[2]->port[0]->output.rvalue = mif_private->param[8]->element[0].rvalue + out_gain * 
                         ( numerator/limited_den );
        mif_private->conn[2]->port[0]->partial[0].port[0] = out_gain * num_gain / limited_den;
        mif_private->conn[2]->port[0]->partial[1].port[0] = -out_gain * numerator * den_gain *
                            den_partial / (limited_den * limited_den);

    }
    else {
        ac_gain.real = out_gain * num_gain / limited_den;
        ac_gain.imag= 0.0;
        mif_private->conn[2]->port[0]->ac_gain[0].port[0] = ac_gain;

        ac_gain.real = -out_gain * numerator * den_gain *
                            den_partial / (limited_den * limited_den);
        ac_gain.imag= 0.0;
        mif_private->conn[2]->port[0]->ac_gain[1].port[0] = ac_gain;
    }

}
