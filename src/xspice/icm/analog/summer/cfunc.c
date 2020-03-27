#line 1 "./analog/summer/cfunc.mod"
#include "ngspice/cm.h"
extern void cm_summer(Mif_Private_t *);
#line 1 "./analog/summer/cfunc.mod"
/*.......1.........2.........3.........4.........5.........6.........7.........8
================================================================================

FILE summer/cfunc.mod

Copyright 1991
Georgia Tech Research Corporation, Atlanta, Ga. 30332
All Rights Reserved

PROJECT A-8503-405
               

AUTHORS                      

     9 Apr 1991     Jeffrey P. Murray


MODIFICATIONS   

     2 Oct 1991    Jeffrey P. Murray
                                   

SUMMARY

    This file contains the model-specific routines used to
    functionally describe the summer code model.


INTERFACES       

    FILE                 ROUTINE CALLED     

    N/A                  N/A


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

FUNCTION void cm_summer()

AUTHORS                      

     9 Apr 1991     Jeffrey P. Murray

MODIFICATIONS   

     2 Oct 1991    Jeffrey P. Murray

SUMMARY

    This function implements the summer code model.

INTERFACES       

    FILE                 ROUTINE CALLED     

    N/A                  N/A


RETURNED VALUE
    
    Returns inputs and outputs via ARGS structure.

GLOBAL VARIABLES
    
    NONE

NON-STANDARD FEATURES

    NONE

==============================================================================*/

/*=== CM_SUMMER ROUTINE ===*/
                                                   

void cm_summer(Mif_Private_t *mif_private) 

{
    int i;                 /* generic loop counter index */
	int size;              /* number of inputs */

    double accumulate;     /* sum of all the (inputs times their
							  respective gains plus their offset). */
	double final_gain;     /* output gain stage */
	double in_gain_temp;   /* temporary variable used to calculate
							  accumulate */

    Mif_Complex_t ac_gain;


    size = mif_private->conn[0]->size;               /* Note that port size */
    final_gain = mif_private->param[2]->element[0].rvalue;        /* and out_gain are read only */
                                        /* once...saves access time. */
    if(mif_private->circuit.anal_type != MIF_AC) {       /* DC & Transient */         
        accumulate = 0.0;
        for (i=0; i<size; i++) {
            in_gain_temp = mif_private->param[1]->element[i].rvalue;  /* Ditto for in_gain[i] */
            accumulate = accumulate + in_gain_temp * 
                            (mif_private->conn[0]->port[i]->input.rvalue + mif_private->param[0]->element[i].rvalue);
            mif_private->conn[1]->port[0]->partial[0].port[i] = in_gain_temp * final_gain;
        }
        mif_private->conn[1]->port[0]->output.rvalue = accumulate * final_gain + mif_private->param[3]->element[0].rvalue;
    }

    else {                     /* AC Analysis */
        for (i=0; i<size; i++) {
            ac_gain.real = mif_private->param[1]->element[i].rvalue * final_gain; 
            ac_gain.imag = 0.0;
            mif_private->conn[1]->port[0]->ac_gain[0].port[i] = ac_gain;
        }
    }
}
