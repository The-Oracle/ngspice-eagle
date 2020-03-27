#line 1 "./xtradev/cmeter/cfunc.mod"
#include "ngspice/cm.h"
extern void cm_cmeter(Mif_Private_t *);
#line 1 "./xtradev/cmeter/cfunc.mod"
/*.......1.........2.........3.........4.........5.........6.........7.........8
================================================================================

FILE cmeter/cfunc.mod

Copyright 1991
Georgia Tech Research Corporation, Atlanta, Ga. 30332
All Rights Reserved

PROJECT A-8503-405
               

AUTHORS                      

    30 July 1991     Bill Kuhn


MODIFICATIONS   

    26 Sept 1991    Jeffrey P. Murray
                                   

SUMMARY

    This file contains the functional description of the cmeter
    code model.


INTERFACES       

    FILE                 ROUTINE CALLED     

    CMmeters.c           double cm_netlist_get_c()
                         


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

FUNCTION cm_cmeter()

AUTHORS                      

    30 July 1991     Bill Kuhn

MODIFICATIONS   

    26 Sept 1991    Jeffrey P. Murray

SUMMARY

    This function implements the cmeter code model.

INTERFACES       

    FILE                 ROUTINE CALLED     

    CMmeters.c           double cm_netlist_get_c()

RETURNED VALUE
    
    Returns inputs and outputs via ARGS structure.

GLOBAL VARIABLES
    
    NONE

NON-STANDARD FEATURES

    NONE

==============================================================================*/

/*=== CM_CMETER ROUTINE ===*/

void cm_cmeter (Mif_Private_t *mif_private)
{

    double      ceq;    /* holding variable for read capacitance value */

    if(mif_private->circuit.init) {
        ceq = cm_netlist_get_c();
        mif_private->inst_var[0]->element[0].rvalue = ceq;
    }
    else
        ceq = mif_private->inst_var[0]->element[0].rvalue;

    mif_private->conn[1]->port[0]->output.rvalue = mif_private->param[0]->element[0].rvalue * ceq;

}




