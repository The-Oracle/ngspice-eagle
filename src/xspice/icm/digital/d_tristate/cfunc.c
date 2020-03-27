#line 1 "./digital/d_tristate/cfunc.mod"
#include "ngspice/cm.h"
extern void cm_d_tristate(Mif_Private_t *);
#line 1 "./digital/d_tristate/cfunc.mod"
/*.......1.........2.........3.........4.........5.........6.........7.........8
================================================================================

FILE d_tristate/cfunc.mod

Copyright 1991
Georgia Tech Research Corporation, Atlanta, Ga. 30332
All Rights Reserved

PROJECT A-8503-405
               

AUTHORS                      

    18 Nov 1991     Jeffrey P. Murray


MODIFICATIONS   

    26 Nov 1991    Jeffrey P. Murray
                                   

SUMMARY

    This file contains the functional description of the d_tristate
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

FUNCTION cm_d_tristate()

AUTHORS                      

    18 Nov 1991     Jeffrey P. Murray

MODIFICATIONS   

    26 Nov 1991     Jeffrey P. Murray

SUMMARY

    This function implements the d_tristate code model.

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

/*=== CM_D_TRISTATE ROUTINE ===*/

/************************************************
*      The following is a model for a simple    *
*   digital tristate for the ATESSE Version     *
*   2.0 system. Note that this version has      *
*   a single delay for both input and enable... *
*   a more realistic model is anticipated in    *
*   the not-so-distant future.                  *
*                                               *
*   Created 11/18/91              J.P,Murray    *
*   Last Modified 11/26/91                      *
************************************************/


void cm_d_tristate(Mif_Private_t *mif_private) 
{
    int   enable;    /* holding variable for enable input */



    /* Retrieve input values and static variables */
    enable = ((Digital_t*)(mif_private->conn[1]->port[0]->input.pvalue))->state;

    ((Digital_t*)(mif_private->conn[2]->port[0]->output.pvalue))->state = ((Digital_t*)(mif_private->conn[0]->port[0]->input.pvalue))->state;
    mif_private->conn[2]->port[0]->delay = mif_private->param[0]->element[0].rvalue;


    /* define input loading... */
    mif_private->conn[0]->port[0]->load = mif_private->param[1]->element[0].rvalue;
    mif_private->conn[1]->port[0]->load = mif_private->param[2]->element[0].rvalue;




    if (ZERO == enable) {

        ((Digital_t*)(mif_private->conn[2]->port[0]->output.pvalue))->strength = HI_IMPEDANCE;

    }
    else 
    if (UNKNOWN == enable) {

        ((Digital_t*)(mif_private->conn[2]->port[0]->output.pvalue))->strength = UNDETERMINED;

    }
    else {
    
        ((Digital_t*)(mif_private->conn[2]->port[0]->output.pvalue))->strength = STRONG;

    }
}
 
    
