#line 1 "./analog/gain/cfunc.mod"
#include "ngspice/cm.h"
extern void cm_gain(Mif_Private_t *);
#line 1 "./analog/gain/cfunc.mod"
/*.......1.........2.........3.........4.........5.........6.........7.........8
================================================================================

FILE gain/cfunc.mod

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
    functionally describe the gain code model.


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

FUNCTION void cm_gain()

AUTHORS                      

     2 Oct 1991     Jeffrey P. Murray

MODIFICATIONS   

    NONE

SUMMARY

    This function implements the gain code model.

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


/*=== CM_GAIN ROUTINE ===*/
                                                   

void cm_gain(Mif_Private_t *mif_private)   /* structure holding parms, inputs, outputs, etc.     */
{
    Mif_Complex_t ac_gain;

    if(mif_private->circuit.anal_type != MIF_AC) {
        mif_private->conn[1]->port[0]->output.rvalue = mif_private->param[2]->element[0].rvalue + mif_private->param[1]->element[0].rvalue * 
                         ( mif_private->conn[0]->port[0]->input.rvalue + mif_private->param[0]->element[0].rvalue);
        mif_private->conn[1]->port[0]->partial[0].port[0] = mif_private->param[1]->element[0].rvalue;
    }
    else {
        ac_gain.real = mif_private->param[1]->element[0].rvalue;
        ac_gain.imag= 0.0;
        mif_private->conn[1]->port[0]->ac_gain[0].port[0] = ac_gain;
    }

}
