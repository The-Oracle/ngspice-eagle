#line 1 "./digital/d_inverter/cfunc.mod"
#include "ngspice/cm.h"
extern void cm_d_inverter(Mif_Private_t *);
#line 1 "./digital/d_inverter/cfunc.mod"
/*.......1.........2.........3.........4.........5.........6.........7.........8
================================================================================

FILE d_inverter/cfunc.mod

Copyright 1991
Georgia Tech Research Corporation, Atlanta, Ga. 30332
All Rights Reserved

PROJECT A-8503-405
               

AUTHORS                      

    14 Jun 1991     Jeffrey P. Murray


MODIFICATIONS   

    30 Sep 1991    Jeffrey P. Murray
                                   

SUMMARY

    This file contains the functional description of the <model_name>
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

#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <string.h>

                                      

/*=== CONSTANTS ========================*/




/*=== MACROS ===========================*/



  
/*=== LOCAL VARIABLES & TYPEDEFS =======*/                         


    
           
/*=== FUNCTION PROTOTYPE DEFINITIONS ===*/




                   
/*==============================================================================

FUNCTION cm_d_inverter()

AUTHORS                      

    14 Jun 1991     Jeffrey P. Murray

MODIFICATIONS   

    30 Sep 1991     Jeffrey P. Murray

SUMMARY

    This function implements the d_inverter code model.

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

/*=== CM_D_INVERTER ROUTINE ===*/

/************************************************
*      The following is the model for the       *
*   digital inverter gate for the               *
*   ATESSE Version 2.0 system.                  *
*                                               *
*   Created 6/14/91               J.P.Murray    *
************************************************/


void cm_d_inverter(Mif_Private_t *mif_private) 

{
    /*int                    i;*/   /* generic loop counter index */
         
                        

    Digital_State_t     *out,   /* temporary output for inverter */
                    *out_old;   /* previous output for inverter */                               


    /** Setup required state variables **/

    if(mif_private->circuit.init) {  /* initial pass */ 

        /* allocate storage for the outputs */
        cm_event_alloc(0,sizeof(Digital_State_t));

        /* define load value on inputs */
        mif_private->conn[0]->port[0]->load = mif_private->param[2]->element[0].rvalue;

        /* retrieve storage for the outputs */
        out = out_old = (Digital_State_t *) cm_event_get_ptr(0,0);

    }
    else {      /* Retrieve previous values */
                                              
        /* retrieve storage for the outputs */
        out = (Digital_State_t *) cm_event_get_ptr(0,0);
        out_old = (Digital_State_t *) cm_event_get_ptr(0,1);
    }

                                      
    /** Check on analysis type **/

    if (mif_private->circuit.anal_type == DC) {   /* DC analysis...output w/o delays */
                                        
        switch ( ((Digital_t*)(mif_private->conn[0]->port[0]->input.pvalue))->state ) {

        case ZERO: 
            ((Digital_t*)(mif_private->conn[1]->port[0]->output.pvalue))->state = *out = *out_old = ONE;
            break;

        case ONE:
            ((Digital_t*)(mif_private->conn[1]->port[0]->output.pvalue))->state = *out = *out_old = ZERO;
            break;
        
        default:
            ((Digital_t*)(mif_private->conn[1]->port[0]->output.pvalue))->state = *out = *out_old = UNKNOWN;
            break;
        }

    }
    else {      /* Transient Analysis */

        switch ( ((Digital_t*)(mif_private->conn[0]->port[0]->input.pvalue))->state ) {
                                                 
        /* fall to zero value */
        case 1: ((Digital_t*)(mif_private->conn[1]->port[0]->output.pvalue))->state = *out = ZERO;
                mif_private->conn[1]->port[0]->delay = mif_private->param[1]->element[0].rvalue;
                break;

        /* rise to one value */
        case 0: ((Digital_t*)(mif_private->conn[1]->port[0]->output.pvalue))->state = *out = ONE;
                mif_private->conn[1]->port[0]->delay = mif_private->param[0]->element[0].rvalue;
                break;
                                
        /* unknown output */
        default:
                ((Digital_t*)(mif_private->conn[1]->port[0]->output.pvalue))->state = *out = UNKNOWN;

                /* based on old value, add rise or fall delay */
                if (0 == *out_old) {  /* add rising delay */
                    mif_private->conn[1]->port[0]->delay = mif_private->param[0]->element[0].rvalue;
                }
                else {                /* add falling delay */
                    mif_private->conn[1]->port[0]->delay = mif_private->param[1]->element[0].rvalue;
                }   
                break;
        }
    }

    ((Digital_t*)(mif_private->conn[1]->port[0]->output.pvalue))->strength = STRONG;
}       




