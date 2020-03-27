#line 1 "./digital/adc_bridge/cfunc.mod"
#include "ngspice/cm.h"
extern void cm_adc_bridge(Mif_Private_t *);
#line 1 "./digital/adc_bridge/cfunc.mod"
/*.......1.........2.........3.........4.........5.........6.........7.........8
================================================================================

FILE adc_bridge/cfunc.mod

Copyright 1991
Georgia Tech Research Corporation, Atlanta, Ga. 30332
All Rights Reserved

PROJECT A-8503-405
               

AUTHORS                      

    6 June 1991     Jeffrey P. Murray


MODIFICATIONS   

    26 Sept 1991    Jeffrey P. Murray
                                   

SUMMARY

    This file contains the functional description of the adc_bridge
    code model.


INTERFACES       

    FILE                 ROUTINE CALLED     
                                    
    CM.c                 void *cm_analog_alloc()
                         void *cm_analog_get_ptr()

    CMevt.c              void *cm_event_alloc()
                         void *cm_event_get_ptr()
                         int  cm_event_queue()




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

FUNCTION cm_adc_bridge()

AUTHORS                      

    6 June 1991     Jeffrey P. Murray

MODIFICATIONS   

    26 Sept 1991    Jeffrey P. Murray

SUMMARY

    This function implements the adc_bridge code model.

INTERFACES       

    FILE                 ROUTINE CALLED     

    CM.c                 void *cm_analog_alloc()
                         void *cm_analog_get_ptr()

    CMevt.c              void *cm_event_alloc()
                         void *cm_event_get_ptr()
                         int  cm_event_queue()

RETURNED VALUE
    
    Returns inputs and outputs via ARGS structure.

GLOBAL VARIABLES
    
    NONE

NON-STANDARD FEATURES

    NONE

==============================================================================*/

/*=== CM_ADC_BRIDGE ROUTINE ===*/

/************************************************
*      The following is the model for the       *
*   analog-to-digital nodebridge for the        *
*   ATESSE Version 2.0 system.                  *
*                                               *
*   Created 6/6/91                              *
*   Last Modified 7/26/91         J.P.Murray    *
************************************************/


void cm_adc_bridge(Mif_Private_t *mif_private) 

{
    double  in_low,        /* analog output value corresponding to '0' 
                               digital input    */
           in_high,        /* analog output value corresponding to '1' 
                               digital input    */
      current_time,        /* the current time value    */
               *in,        /* base address of array holding all digital output 
                               values plus their previous values    */            
           *in_old;        /* base address of array holding previous
                               output values    */            


    int           i,        /* generic loop counter index */
	           size;        /* number of input & output ports */
         
                        

   Digital_State_t   *out,  /* base address of array holding all input 
                               values plus their previous values */
                 *out_old,  /* base address of array holding previous
                               input values */
                     test;  /* temp holding variable for digital states */





    /* determine "width" of the node bridge... */

    size = mif_private->conn[0]->size;               
    in_high = mif_private->param[1]->element[0].rvalue;
    in_low = mif_private->param[0]->element[0].rvalue;

            


    if (mif_private->circuit.init) {  /*** Test for INIT == TRUE. If so, allocate storage, etc. ***/


        /* Allocate storage for inputs */
        cm_analog_alloc(0, size * (int) sizeof(double));
                      
        /* Allocate storage for outputs */
        cm_event_alloc(1, size * (int) sizeof(Digital_State_t));

        /* Get analog addresses */        
        in = in_old = (double *) cm_analog_get_ptr(0,0);

        /* Get discrete addresses */
        out = out_old = (Digital_State_t *) cm_event_get_ptr(1,0);
    }

    else {    /*** This is not an initialization pass...retrieve storage
                   addresses and calculate new outputs, if required. ***/


        /** Retrieve previous values... **/

        /* assign discrete addresses */
        in = (double *) cm_analog_get_ptr(0,0);
        in_old = (double *) cm_analog_get_ptr(0,1);

        /* assign analog addresses */
        out = (Digital_State_t *) cm_event_get_ptr(1,0);
        out_old = (Digital_State_t *) cm_event_get_ptr(1,1);

    }
                            

    /* read current input values */
    for (i=0; i<size; i++) {
        in[i] = mif_private->conn[0]->port[i]->input.rvalue;
    }


    /*** If TIME == 0.0, bypass calculations ***/
    if (0.0 != mif_private->circuit.time) {                                            
    
        switch (mif_private->circuit.call_type) {
    
        case ANALOG:    /** analog call...check for breakpoint calls. **/
    
            /* loop through all inputs... */
            for (i=0; i<size; i++) {
            
                if (in[i] <= in_low) {    /* low output required */
    
                    test = ZERO;
    
                    if ( test != out_old[i] ) {   
                        /* call for event breakpoint... */
                        current_time = mif_private->circuit.time;
                        cm_event_queue(current_time);
                    }                               
                    else {
                        /* no change since last time */
                    }
    
                }
                else {
                    if (in[i] >= in_high) {    /* high output required */
    
                        test = ONE;        
    
                        if ( test != out_old[i] ) {   
                            /* call for event breakpoint... */
                            current_time = mif_private->circuit.time;
                            cm_event_queue(current_time);
                        }                               
                        else {
                            /* no change since last time */
                        }
    
                    }
                    else {    /* unknown output required */
    
                        if ( UNKNOWN != out_old[i] ) {   
                            
                            /* call for event breakpoint... */
                            current_time = mif_private->circuit.time;
                            cm_event_queue(current_time);
                        }
                        else {
                            /* no change since last time */
                        }
                    }
                }
            }          
            
            break;
               
    
    
        case EVENT:    /** discrete call...lots to do **/
    
            /* loop through all inputs... */
            for (i=0; i<size; i++) {
            
                if (in[i] <= in_low) {    /* low output required */
    
                    out[i] = ZERO;
    
                    if ( out[i] != out_old[i] ) {   
                        /* post changed value */
                        ((Digital_t*)(mif_private->conn[1]->port[i]->output.pvalue))->state = ZERO;        
                        mif_private->conn[1]->port[i]->delay = mif_private->param[3]->element[0].rvalue;        
                    }                               
                    else {
                        /* no change since last time */
                        mif_private->conn[1]->port[i]->changed = FALSE;
                    }
    
                }
                else {
                    if (in[i] >= in_high) {    /* high output required */
    
                        out[i] = ONE;        
    
                        if ( out[i] != out_old[i] ) {   
                            /* post changed value */
                            ((Digital_t*)(mif_private->conn[1]->port[i]->output.pvalue))->state = ONE;        
                            mif_private->conn[1]->port[i]->delay = mif_private->param[2]->element[0].rvalue;        
                        }                               
                        else {
                            /* no change since last time */
                            mif_private->conn[1]->port[i]->changed = FALSE;
                        }
    
                    }
                    else {    /* unknown output required */
    
                        out[i] = UNKNOWN;        
    
                        if ( UNKNOWN != out_old[i] ) {   
                            
                            /* post changed value */
                            ((Digital_t*)(mif_private->conn[1]->port[i]->output.pvalue))->state = UNKNOWN;        
    
                            switch (out_old[i]) {
                            case ONE:
                                mif_private->conn[1]->port[i]->delay = mif_private->param[3]->element[0].rvalue;        
                                break;
    
                            case ZERO:
                                mif_private->conn[1]->port[i]->delay = mif_private->param[2]->element[0].rvalue;        
                                break;
                            case UNKNOWN: /* should never get here! */
                                break;
                            }
                        }
                        else {
                            /* no change since last time */
                            mif_private->conn[1]->port[i]->changed = FALSE;
                        }
                    }
                }
                /* regardless, output the strength */
                ((Digital_t*)(mif_private->conn[1]->port[i]->output.pvalue))->strength = STRONG;        
            }          
            break;
        }
    }
    else {  /*** TIME == 0.0 => set outputs to input value... ***/
        /* loop through all inputs... */
        for (i=0; i<size; i++) {

            if (in[i] <= in_low) {    /* low output required */
                ((Digital_t*)(mif_private->conn[1]->port[i]->output.pvalue))->state = out[i] = ZERO;
            }
            else 
            if (in[i] >= in_high) {   /* high output required */
                ((Digital_t*)(mif_private->conn[1]->port[i]->output.pvalue))->state = out[i] = ONE;
            }
            else {
                ((Digital_t*)(mif_private->conn[1]->port[i]->output.pvalue))->state = out[i] = UNKNOWN;
            }
            ((Digital_t*)(mif_private->conn[1]->port[i]->output.pvalue))->strength = STRONG;        
        }
    }
}



