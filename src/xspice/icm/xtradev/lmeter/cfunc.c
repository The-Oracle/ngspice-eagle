#line 1 "./xtradev/lmeter/cfunc.mod"
#include "ngspice/cm.h"
extern void cm_lmeter(Mif_Private_t *);
#line 1 "./xtradev/lmeter/cfunc.mod"
/*.......1.........2.........3.........4.........5.........6.........7.........8
================================================================================

FILE lmeter/cfunc.mod

Copyright 1991
Georgia Tech Research Corporation, Atlanta, Ga. 30332
All Rights Reserved

PROJECT A-8503-405
               

AUTHORS                      

    30 Jul 1991     Bill Kuhn


MODIFICATIONS   

     2 Oct 1991    Jeffrey P. Murray
                                   

SUMMARY

    This file contains the model-specific routines used to
    functionally describe the lmeter code model.


INTERFACES       

    FILE                 ROUTINE CALLED     

    CMmeters.c           double cm_netlist_get_l()
                         


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

FUNCTION void cm_lmeter()

AUTHORS                      

    30 Jul 1991     Bill Kuhn


MODIFICATIONS   

     2 Oct 1991    Jeffrey P. Murray

SUMMARY

    This function implements the lmeter code model.

INTERFACES       

    FILE                 ROUTINE CALLED     

    CMmeters.c           double cm_netlist_get_l()


RETURNED VALUE
    
    Returns inputs and outputs via ARGS structure.

GLOBAL VARIABLES
    
    NONE

NON-STANDARD FEATURES

    NONE

==============================================================================*/

/*=== CM_LMETER ROUTINE ===*/
                                                   

void cm_lmeter (Mif_Private_t *mif_private)
{

    double      leq;

    if(mif_private->circuit.init) {
        leq = cm_netlist_get_l();
        mif_private->inst_var[0]->element[0].rvalue = leq;
    }
    else
        leq = mif_private->inst_var[0]->element[0].rvalue;

    mif_private->conn[1]->port[0]->output.rvalue = mif_private->param[0]->element[0].rvalue * leq;
}




