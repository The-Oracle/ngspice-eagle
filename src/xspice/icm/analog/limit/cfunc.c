#line 1 "./analog/limit/cfunc.mod"
#include "ngspice/cm.h"
extern void cm_limit(Mif_Private_t *);
#line 1 "./analog/limit/cfunc.mod"
/*.......1.........2.........3.........4.........5.........6.........7.........8
================================================================================

FILE limit/cfunc.mod

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
    functionally describe the limit code model.


INTERFACES       

    FILE                 ROUTINE CALLED     

    CMutil.c             void cm_smooth_corner(); 


REFERENCED FILES

    Inputs from and outputs to ARGS structure.
                     

NON-STANDARD FEATURES

    NONE

===============================================================================*/

/*=== INCLUDE FILES ====================*/


                                      

/*=== CONSTANTS ========================*/




/*=== MACROS ===========================*/



  
/*=== LOCAL VARIABLES & TYPEDEFS =======*/                         


    
           
/*=== FUNCTION PROTOTYPE DEFINITIONS ===*/




                   
/*==============================================================================

FUNCTION void cm_limit()

AUTHORS                      

     2 Oct 1991     Jeffrey P. Murray

MODIFICATIONS   

    NONE

SUMMARY

    This function implements the limit code model.

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

/*=== CM_LIMIT ROUTINE ===*/

void cm_limit(Mif_Private_t *mif_private)  /* structure holding parms, 
                                       inputs, outputs, etc.     */
{
    double out_lower_limit;   /* output lower limit */
	double out_upper_limit;   /* output upper limit */
	double limit_range;       /* upper and lower limit smoothing range */
	double gain;              /* gain */
    double threshold_upper;   /* value above which smoothing takes place */
	double threshold_lower;   /* value below which smoothing takes place */
	double out;               /* output */
	double limited_out;       /* limited output value */
    double out_partial;       /* partial of the output wrt input */
    
    Mif_Complex_t ac_gain;



    /* Retrieve frequently used parameters... */

    out_lower_limit = mif_private->param[2]->element[0].rvalue;
    out_upper_limit = mif_private->param[3]->element[0].rvalue;
    limit_range = mif_private->param[4]->element[0].rvalue;
    gain = mif_private->param[1]->element[0].rvalue;


    if (mif_private->param[5]->element[0].bvalue == MIF_TRUE)     /* Set range to absolute value */
        limit_range = limit_range * 
              (out_upper_limit - out_lower_limit);



    threshold_upper = out_upper_limit -   /* Set Upper Threshold */
                         limit_range;

    threshold_lower = out_lower_limit +   /* Set Lower Threshold */
                         limit_range;
                              


    /* Compute Un-Limited Output */

    out = gain * (mif_private->param[0]->element[0].rvalue + mif_private->conn[0]->port[0]->input.rvalue); 



    if (out < threshold_lower) {       /* Limit Out @ Lower Bound */

        if (out > (out_lower_limit - limit_range)) { /* Parabolic */
            cm_smooth_corner(out,out_lower_limit,out_lower_limit,
                        limit_range,0.0,1.0,&limited_out,
                        &out_partial);               
            out_partial = gain * out_partial;   
        }
        else {                             /* Hard-Limited Region */
            limited_out = out_lower_limit;
            out_partial = 0.0;
        }    
    }
    else {
        if (out > threshold_upper) {       /* Limit Out @ Upper Bound */

            if (out < (out_upper_limit + limit_range)) { /* Parabolic */
                cm_smooth_corner(out,out_upper_limit,out_upper_limit,
                            limit_range,1.0,0.0,&limited_out,
                            &out_partial);               
                out_partial = gain * out_partial; 
            }
            else {                             /* Hard-Limited Region */
                limited_out = out_upper_limit;
                out_partial = 0.0;
            }
        }
        else {               /* No Limiting Needed */
            limited_out = out;
            out_partial = gain;
        }
    }

    if (mif_private->circuit.anal_type != MIF_AC) {     /* DC & Transient Analyses */

        mif_private->conn[1]->port[0]->output.rvalue = limited_out;
        mif_private->conn[1]->port[0]->partial[0].port[0] = out_partial;

    }
    else {                        /* AC Analysis */
        ac_gain.real = out_partial;
        ac_gain.imag= 0.0;
        mif_private->conn[1]->port[0]->ac_gain[0].port[0] = ac_gain;

    }
}
