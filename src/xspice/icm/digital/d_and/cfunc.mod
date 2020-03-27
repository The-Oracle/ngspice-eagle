/*.......1.........2.........3.........4.........5.........6.........7.........8
================================================================================

FILE d_and/cfunc.mod

Copyright 1991
Georgia Tech Research Corporation, Atlanta, Ga. 30332
All Rights Reserved

PROJECT A-8503-405
               

AUTHORS                      

    14 June 1991     Jeffrey P. Murray


MODIFICATIONS   

    27 Sept 1991    Jeffrey P. Murray
                                   

SUMMARY

    This file contains the functional description of the d_and
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

FUNCTION cm_d_and()

AUTHORS                      

    14 Jun 1991     Jeffrey P. Murray

MODIFICATIONS   

    27 Sep 1991     Jeffrey P. Murray

SUMMARY

    This function implements the d_and code model.

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

/*=== CM_D_AND ROUTINE ===*/

/************************************************
*      The following is the model for the       *
*   digital AND gate for the                    *
*   ATESSE Version 2.0 system.                  *
*                                               *
*   Created 6/14/91               J.P.Murray    *
************************************************/


void cm_d_and(ARGS) 

{
    int                    i,   /* generic loop counter index */
	                    size;   /* number of input & output ports */
         
                        

    Digital_State_t     *out,   /* temporary output for buffers */
                    *out_old,   /* previous output for buffers  */                               
                       input;   /* temp storage for input bits  */    


    /** Retrieve size value... **/
    size = PORT_SIZE(in);

                                          

    /*** Setup required state variables ***/

    if(INIT) {  /* initial pass */ 

        /* allocate storage for the outputs */
        cm_event_alloc(0,sizeof(Digital_State_t));
                  
        /* set loading for inputs */
        for (i=0; i<size; i++) LOAD(in[i]) = PARAM(input_load);

        /* retrieve storage for the outputs */
        out = out_old = (Digital_State_t *) cm_event_get_ptr(0,0);

    }
    else {      /* Retrieve previous values */
                                              
        /* retrieve storage for the outputs */
        out = (Digital_State_t *) cm_event_get_ptr(0,0);
        out_old = (Digital_State_t *) cm_event_get_ptr(0,1);
    }


                                     

    /*** Calculate new output value based on inputs ***/

    *out = ONE;
    for (i=0; i<size; i++) {

        /* make sure this input isn't floating... */
        if ( FALSE == PORT_NULL(in) ) {

            /* if a 0, set *out low */
            if ( ZERO == (input = INPUT_STATE(in[i])) ) {
                *out = ZERO;
                break;
            }
            else {                    
                /* if an unknown input, set *out to unknown & break */
                if ( UNKNOWN == input ) {
                    *out = UNKNOWN;
                }
            }
        }
        else {
            /* at least one port is floating...output is unknown */
            *out = UNKNOWN;
            break;
        }
    }       



    /*** Determine analysis type and output appropriate values ***/

    if (ANALYSIS == DC) {   /** DC analysis...output w/o delays **/
                                  
        OUTPUT_STATE(out) = *out;

    }

    else {      /** Transient Analysis **/


        if ( *out != *out_old ) { /* output value is changing */

            switch ( *out ) {
                                                 
            /* fall to zero value */
            case 0: OUTPUT_STATE(out) = ZERO;
                    OUTPUT_DELAY(out) = PARAM(fall_delay);
                    break;
    
            /* rise to one value */
            case 1: OUTPUT_STATE(out) = ONE;
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
        else {                    /* output value not changing */
            OUTPUT_CHANGED(out) = FALSE;
        }
    }

    OUTPUT_STRENGTH(out) = STRONG;

} 

      



