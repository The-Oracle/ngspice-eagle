#line 1 "./digital/d_pulldown/cfunc.mod"
#include "ngspice/cm.h"
extern void cm_d_pulldown(Mif_Private_t *);
#line 1 "./digital/d_pulldown/cfunc.mod"
/*.......1.........2.........3.........4.........5.........6.........7.........8
================================================================================

FILE d_pulldown/cfunc.mod

Copyright 1991
Georgia Tech Research Corporation, Atlanta, Ga. 30332
All Rights Reserved

PROJECT A-8503-405
               

AUTHORS                      

    19 Nov 1991     Jeffrey P. Murray


MODIFICATIONS   

    19 Nov 1991    Jeffrey P. Murray
                                   

SUMMARY

    This file contains the functional description of the d_pulldown
    code model.


INTERFACES       

    FILE                 ROUTINE CALLED     

    CMevt.c              void *cm_event_alloc()
                         void *cm_event_get_ptr()
                         


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

FUNCTION cm_d_pulldown()

AUTHORS                      

    19 Nov 1991     Jeffrey P. Murray

MODIFICATIONS   

    19 Nov 1991     Jeffrey P. Murray

SUMMARY

    This function implements the d_pulldown code model.

INTERFACES       

    FILE                 ROUTINE CALLED     

    CMevt.c              void *cm_event_alloc()
                         void *cm_event_get_ptr()

RETURNED VALUE
    
    Returns inputs and outputs via ARGS structure.

GLOBAL VARIABLES
    
    NONE

NON-STANDARD FEATURES

    NONE

==============================================================================*/

/*=== CM_D_PULLDOWN ROUTINE ===*/

/************************************************
*      The following is the model for the       *
*   digital pulldown resistor for the           *
*   ATESSE Version 2.0 system.                  *
*                                               *
*   Created 11/19/91              J.P,Murray    *
************************************************/


void cm_d_pulldown(Mif_Private_t *mif_private) 

{
    mif_private->conn[0]->port[0]->load = mif_private->param[0]->element[0].rvalue;
    ((Digital_t*)(mif_private->conn[0]->port[0]->output.pvalue))->state = ZERO;
    ((Digital_t*)(mif_private->conn[0]->port[0]->output.pvalue))->strength = RESISTIVE;
}




