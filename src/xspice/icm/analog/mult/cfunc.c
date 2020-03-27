#line 1 "./analog/mult/cfunc.mod"
#include "ngspice/cm.h"
extern void cm_mult(Mif_Private_t *);
#line 1 "./analog/mult/cfunc.mod"
/*.......1.........2.........3.........4.........5.........6.........7.........8
================================================================================

FILE mult/cfunc.mod

Copyright 1991
Georgia Tech Research Corporation, Atlanta, Ga. 30332
All Rights Reserved

PROJECT A-8503-405
               

AUTHORS                      

    20 Mar 1991     Jeffrey P. Murray


MODIFICATIONS   

     2 Oct 1991    Jeffrey P. Murray
                                   

SUMMARY

    This file contains the model-specific routines used to
    functionally describe the mult code model.


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

FUNCTION void cm_mult()

AUTHORS                      

    20 Mar 1991     Jeffrey P. Murray


MODIFICATIONS   

     2 Oct 1991    Jeffrey P. Murray

SUMMARY

    This function implements the mult code model.

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

/*=== CM_MULT ROUTINE ===*/
                                                   

void cm_mult(Mif_Private_t *mif_private) 

{
    int i;     /* generic loop counter index */
	int size;  /* number of input ports */

    double accumulate_gain;  /* product of all the gains */
	double accumulate_in;    /* product of all (inputs + offsets) */
	double final_gain;       /* output gain */

    Mif_Complex_t ac_gain;


    size = mif_private->conn[0]->size;               /* Note that port size */
    final_gain = mif_private->param[2]->element[0].rvalue;        /* and out_gain are read only */
                                        /* once...saves access time. */


    /* Calculate multiplication of inputs and gains for   */
    /* all types of analyes....                           */

    accumulate_gain = 1.0;
    accumulate_in = 1.0;

    for (i=0; i<size; i++) {
        accumulate_gain = accumulate_gain *    /* Multiply all input gains */
                             mif_private->param[1]->element[i].rvalue; 
        accumulate_in = accumulate_in *        /* Multiply offset input values */
                           (mif_private->conn[0]->port[i]->input.rvalue + mif_private->param[0]->element[i].rvalue);
    }


    if(mif_private->circuit.anal_type != MIF_AC) {                /* DC & Transient */         

        for (i=0; i<size; i++) {   /* Partials are product of all gains and */
                                   /*   inputs divided by each individual   */
                                   /*      input value.                     */

            if (0.0 != accumulate_in) {  /* make sure that no division by zero
                                            will occur....                      */
                mif_private->conn[1]->port[0]->partial[0].port[i] = (accumulate_in/(mif_private->conn[0]->port[i]->input.rvalue +
                         mif_private->param[0]->element[i].rvalue)) * accumulate_gain * final_gain;
            }
            else {                       /* otherwise, set partial to zero.     */
                mif_private->conn[1]->port[0]->partial[0].port[i] = 0.0;
            }

        }

        mif_private->conn[1]->port[0]->output.rvalue = accumulate_in * accumulate_gain * final_gain + 
                                          mif_private->param[3]->element[0].rvalue;
    }

    else {                              /* AC Analysis */
 
        for (i=0; i<size; i++) {   /* Partials are product of all gains and */
                                   /*   inputs divided by each individual   */
                                   /*      input value.                     */
            ac_gain.real = (accumulate_in/(mif_private->conn[0]->port[i]->input.rvalue +
                     mif_private->param[0]->element[i].rvalue)) * accumulate_gain * final_gain;
            ac_gain.imag = 0.0;
            mif_private->conn[1]->port[0]->ac_gain[0].port[i] = ac_gain;
        }
    }
}
