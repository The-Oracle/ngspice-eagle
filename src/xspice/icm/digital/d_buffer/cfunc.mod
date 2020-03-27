/*.......1.........2.........3.........4.........5.........6.........7.........8
================================================================================

FILE d_buffer/cfunc.mod

Copyright 1991
Georgia Tech Research Corporation, Atlanta, Ga. 30332
All Rights Reserved

PROJECT A-8503-405
               

AUTHORS                      

    14 June 1991     Jeffrey P. Murray


MODIFICATIONS   

    27 Sept 1991    Jeffrey P. Murray
                                   

SUMMARY

    This file contains the functional description of the d_buffer
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

FUNCTION cm_d_buffer()

AUTHORS                      

    14 Jun 1991     Jeffrey P. Murray

MODIFICATIONS   

    27 Sep 1991     Jeffrey P. Murray

SUMMARY

    This function implements the d_buffer code model.

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

/*=== CM_D_BUFFER ROUTINE ===*/

/************************************************
*      The following is the model for the       *
*   digital buffer for the                      *
*   ATESSE Version 2.0 system.                  *
*                                               *
*   Created 6/14/91               J.P,Murray    *
************************************************/


void cm_d_buffer(ARGS) 

{
    /*int                    i;*/   /* generic loop counter index */
         
                        

    Digital_State_t     *out,   /* temporary output for buffers */
                    *out_old;   /* previous output for buffers  */                               


    /** Setup required state variables **/

    if(INIT) {  /* initial pass */ 

        /* allocate storage for the outputs */
        cm_event_alloc(0,sizeof(Digital_State_t));

        /* define input loading... */
        LOAD(in) = PARAM(input_load);

        /* retrieve storage for the outputs */
        out = out_old = (Digital_State_t *) cm_event_get_ptr(0,0);

    }
    else {      /* Retrieve previous values */
                                              
        /* retrieve storage for the outputs */
        out = (Digital_State_t *) cm_event_get_ptr(0,0);
        out_old = (Digital_State_t *) cm_event_get_ptr(0,1);
    }

                                      
    /** Check on analysis type **/

    if (ANALYSIS == DC) {   /* DC analysis...output w/o delays */
                                  
        OUTPUT_STATE(out) = *out = INPUT_STATE(in);

    }
    else {      /* Transient Analysis */

        switch ( INPUT_STATE(in) ) {
                                                 
        /* fall to zero value */
        case 0: OUTPUT_STATE(out) = *out = ZERO;
                OUTPUT_DELAY(out) = PARAM(fall_delay);
                break;

        /* rise to one value */
        case 1: OUTPUT_STATE(out) = *out = ONE;
                OUTPUT_DELAY(out) = PARAM(rise_delay);
                break;
                                
        /* unknown output */
        default:
                OUTPUT_STATE(out) = *out = UNKNOWN;

                /* based on old value, add rise or fall delay */
                if (0 == *out_old) {  /* add rising delay */
                    OUTPUT_DELAY(out) = PARAM(rise_delay);
                }
                else {                /* add falling delay */
                    OUTPUT_DELAY(out) = PARAM(fall_delay);
                }   
                break;
        }
    }

    OUTPUT_STRENGTH(out) = STRONG;
}       



