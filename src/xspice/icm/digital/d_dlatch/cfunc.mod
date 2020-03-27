/*.......1.........2.........3.........4.........5.........6.........7.........8
================================================================================

FILE d_dlatch/cfunc.mod

Copyright 1991
Georgia Tech Research Corporation, Atlanta, Ga. 30332
All Rights Reserved

PROJECT A-8503-405
               

AUTHORS                      

    25 Jun 1991     Jeffrey P. Murray


MODIFICATIONS   

    13 Aug 1991    Jeffrey P. Murray
    30 Sep 1991    Jeffrey P. Murray
    29 Jan 1992    Jeffrey P. Murray
                                   

SUMMARY

    This file contains the functional description of the d_latch
    code model.


INTERFACES       

    FILE                 ROUTINE CALLED     

    CMutil.c             void cm_toggle_bit(); 
       
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

FUNCTION cm_toggle_bit()


AUTHORS                      

    30 Sept 1991     Jeffrey P. Murray


MODIFICATIONS   

    NONE


SUMMARY

    Alters the state of a passed digital variable to its
    complement. Thus, a ONE changes to a ZERO. A ZERO changes
    to a ONE, and an UNKNOWN remains unchanged.


INTERFACES       

    FILE                 ROUTINE CALLED     

    N/A                  N/A


RETURNED VALUE
    
    No returned value. Passed pointer to variable is used 
    to redefine the variable value.
           

GLOBAL VARIABLES
    
    NONE


NON-STANDARD FEATURES

    NONE
                                       
===============================================================================*/

/*=== CM_TOGGLE_BIT ROUTINE ===*/

static void cm_toggle_bit(Digital_State_t *bit) 

{
    /* Toggle bit from ONE to ZERO or vice versa, unless the
       bit value is UNKNOWN. In the latter case, return 
       without changing the bit value.                      */

    if ( UNKNOWN != *bit ) {
        if ( ONE == *bit ) {
            *bit = ZERO;
        }
        else { 
            *bit = ONE;
        }
    }

}


                   
/*==============================================================================

FUNCTION cm_dlatch()

AUTHORS                      

    25 Jun 1991     Jeffrey P. Murray

MODIFICATIONS   

    13 Aug 1991    Jeffrey P. Murray
    30 Sep 1991    Jeffrey P. Murray
    29 Jan 1992    Jeffrey P. Murray

SUMMARY

    This function implements the d_dlatch code model.

INTERFACES       

    FILE                 ROUTINE CALLED     

    CMutil.c             void cm_toggle_bit(); 
       
    CMevt.c              void *cm_event_alloc()
                         void *cm_event_get_ptr()

RETURNED VALUE
    
    Returns inputs and outputs via ARGS structure.

GLOBAL VARIABLES
    
    NONE

NON-STANDARD FEATURES

    NONE

==============================================================================*/

/*=== CM_D_DLATCH ROUTINE ===*/

/************************************************
*      The following is the model for the       *
*   digital d-type latch for the                *
*   ATESSE Version 2.0 system.                  *
*                                               *
*   Created 6/25/91               J.P.Murray    *
************************************************/


void cm_d_dlatch(ARGS) 

{
    /*int                    i;*/   /* generic loop counter index */
                        

    Digital_State_t    *data,   /* current data value    */
                   *data_old,   /* previous data value   */
                     *enable,   /* current enable value    */
                 *enable_old,   /* previous enable value   */
                        *set,   /* current set value for dlatch    */
                    *set_old,   /* previous set value for dlatch   */
                      *reset,   /* current reset value for dlatch      */
                  *reset_old,   /* previous reset value for dlatch     */
                        *out,   /* current output for dlatch   */
                    *out_old,   /* previous output for dlatch  */                            

                        temp;   /* temp storage for state values    */                            



    /*** Setup required state variables ***/

    if(INIT) {  /* initial pass */ 

        /* allocate storage */
        cm_event_alloc(0,sizeof(Digital_State_t));
        cm_event_alloc(1,sizeof(Digital_State_t));
        cm_event_alloc(2,sizeof(Digital_State_t));
        cm_event_alloc(3,sizeof(Digital_State_t));
        cm_event_alloc(4,sizeof(Digital_State_t));

        /* declare load values */
        LOAD(data) = PARAM(data_load);
        LOAD(enable) = PARAM(enable_load);
        if ( !PORT_NULL(set) ) {
            LOAD(set) = PARAM(set_load);
        }
        if ( !PORT_NULL(reset) ) {
        LOAD(reset) = PARAM(reset_load);
        }

        /* retrieve storage for the outputs */
        data = data_old = (Digital_State_t *) cm_event_get_ptr(0,0);
        enable = enable_old = (Digital_State_t *) cm_event_get_ptr(1,0);
        set = set_old = (Digital_State_t *) cm_event_get_ptr(2,0);
        reset = reset_old =(Digital_State_t *) cm_event_get_ptr(3,0);
        out = out_old = (Digital_State_t *) cm_event_get_ptr(4,0);

    }
    else {      /* Retrieve previous values */
                                              
        /* retrieve storage for the outputs */
        data = (Digital_State_t *) cm_event_get_ptr(0,0);
        data_old = (Digital_State_t *) cm_event_get_ptr(0,1);
        enable = (Digital_State_t *) cm_event_get_ptr(1,0);
        enable_old = (Digital_State_t *) cm_event_get_ptr(1,1);
        set = (Digital_State_t *) cm_event_get_ptr(2,0);
        set_old = (Digital_State_t *) cm_event_get_ptr(2,1);
        reset = (Digital_State_t *) cm_event_get_ptr(3,0);
        reset_old = (Digital_State_t *) cm_event_get_ptr(3,1);
        out = (Digital_State_t *) cm_event_get_ptr(4,0);
        out_old = (Digital_State_t *) cm_event_get_ptr(4,1);
    }


         
    /******* load current input values if set or reset 
             are not connected, set to zero... *******/
    *data = INPUT_STATE(data);
    *enable = INPUT_STATE(enable);
    if ( PORT_NULL(set) ) {
        *set = *set_old = ZERO;
    }
    else {
        *set = INPUT_STATE(set);
    }
    if ( PORT_NULL(reset) ) {
        *reset = *reset_old = ZERO;
    }
    else {
        *reset = INPUT_STATE(reset);
    }

                            



    /******* Determine analysis type and output appropriate values *******/

    if (0.0 == TIME) {   /****** DC analysis...output w/o delays ******/

        temp = (Digital_State_t) PARAM(ic);

        /** Modify output if set or reset lines are active **/
        if (*enable==ONE) temp = *data; 
        if ( (*set==ONE) && (*reset==ZERO) ) temp = ONE;
        if ( (*set==ZERO) && (*reset==ONE) ) temp = ZERO;
        if ( (*set==ONE) && (*reset==ONE) ) temp = UNKNOWN;

        *out = *out_old = temp;

                                  
        if ( !PORT_NULL(out) ) {
            OUTPUT_STATE(out) = temp;
        }
        if ( !PORT_NULL(Nout) ) {
            cm_toggle_bit(&temp);
            OUTPUT_STATE(Nout) = temp;
        }
    }

    else {      /****** Transient Analysis ******/


        /***** Find input that has changed... *****/

        /**** Test set value for change ****/
        if ( *set != *set_old ) { /* either set or set release */
            switch ( *set ) {

            case ONE:
                if ( ONE != *reset) {
                    if (*out_old != ONE) {  /* set will change output */
                        /* output goes to ONE */
                        *out = ONE;

                        if ( !PORT_NULL(out) ) {
                            OUTPUT_STATE(out) = ONE;
                            OUTPUT_DELAY(out) = PARAM(set_delay);
                        }
                        if ( !PORT_NULL(Nout) ) {
                            OUTPUT_STATE(Nout) = ZERO;
                            OUTPUT_DELAY(Nout) = PARAM(set_delay);
                        }
                    }
                    else {
                        *out = *out_old;     /* output already set */
                        if ( !PORT_NULL(out) ) {
                            OUTPUT_CHANGED(out) = FALSE;
                        }
                        if ( !PORT_NULL(Nout) ) {
                            OUTPUT_CHANGED(Nout) = FALSE;
                        }
                    }
                }
                else {
                    if (*out_old != UNKNOWN) {  /* set will change output */
                        /* output goes to UNKNOWN */
                        *out = UNKNOWN;

                        if ( !PORT_NULL(out) ) {
                            OUTPUT_STATE(out) = UNKNOWN;
                            OUTPUT_DELAY(out) = PARAM(set_delay);
                        }
                        if ( !PORT_NULL(Nout) ) {
                            OUTPUT_STATE(Nout) = UNKNOWN;
                            OUTPUT_DELAY(Nout) = PARAM(set_delay);
                        }
                    }
                    else {
                        *out = *out_old;     /* output already unknown */
                        if ( !PORT_NULL(out) ) {
                            OUTPUT_CHANGED(out) = FALSE;
                        }
                        if ( !PORT_NULL(Nout) ) {
                            OUTPUT_CHANGED(Nout) = FALSE;
                        }
                    }
                }
                break;
        

            case ZERO:
            case UNKNOWN:
                if ( ONE != *reset) {
                    if ( ONE == *enable ) {
                        /* active level...save & output current data value */
                        temp = *data;

                        if (*out_old != temp) {  /* enable will change output */

                            *out = temp;

                            if ( !PORT_NULL(out) ) {
                                OUTPUT_STATE(out) = temp;
                                OUTPUT_DELAY(out) = PARAM(set_delay);
                            }

                            cm_toggle_bit(&temp);

                            if ( !PORT_NULL(Nout) ) {
                                OUTPUT_STATE(Nout) = temp;
                                OUTPUT_DELAY(Nout) = PARAM(set_delay);
                            }
                        }
                        else {
                            *out = *out_old;     /* output same as before */
                            if ( !PORT_NULL(out) ) {
                                OUTPUT_CHANGED(out) = FALSE;
                            }
                            if ( !PORT_NULL(Nout) ) {
                                OUTPUT_CHANGED(Nout) = FALSE;
                            }
                        }
                    }
                    else {
                        /* output remains at current value */
                        *out = *out_old;
                        if ( !PORT_NULL(out) ) {
                            OUTPUT_CHANGED(out) = FALSE;
                        }
                        if ( !PORT_NULL(Nout) ) {
                            OUTPUT_CHANGED(Nout) = FALSE;
                        }
                    }
                }
                else {
                    if (*out_old != ZERO) {  /* set will change output */
                        /* output returns to reset condition */
                        *out = ZERO;
                        if ( !PORT_NULL(out) ) {
                            OUTPUT_STATE(out) = ZERO;
                            OUTPUT_DELAY(out) = PARAM(set_delay);
                        }
                        if ( !PORT_NULL(Nout) ) {
                            OUTPUT_STATE(Nout) = ONE;
                            OUTPUT_DELAY(Nout) = PARAM(set_delay);
                        }
                    }
                    else {
                        *out = *out_old;     /* output already reset */
                        if ( !PORT_NULL(out) ) {
                            OUTPUT_CHANGED(out) = FALSE;
                        }
                        if ( !PORT_NULL(Nout) ) {
                            OUTPUT_CHANGED(Nout) = FALSE;
                        }
                    }
                }
                break;
            }
        }
        else {

            /**** Test reset value for change ****/
            if ( *reset != *reset_old ) { /* either reset or reset release */
                switch ( *reset ) {
    
                case ONE:
                    if ( ONE != *set) {
                        if (*out_old != ZERO) {  /* reset will change output */
                            /* output goes to ZERO */
                            *out = ZERO;

                            if ( !PORT_NULL(out) ) {
                                OUTPUT_STATE(out) = ZERO;
                                OUTPUT_DELAY(out) = PARAM(reset_delay);
                            }
                            if ( !PORT_NULL(Nout) ) {
                                OUTPUT_STATE(Nout) = ONE;
                                OUTPUT_DELAY(Nout) = PARAM(reset_delay);
                            }
                        }
                        else {
                            *out = *out_old;     /* output already reset */
                            if ( !PORT_NULL(out) ) {
                                OUTPUT_CHANGED(out) = FALSE;
                            }
                            if ( !PORT_NULL(Nout) ) {
                                OUTPUT_CHANGED(Nout) = FALSE;
                            }
                        }
                    }
                    else {
                        if (*out_old != UNKNOWN) {  /* reset will change output */
                            /* output goes to UNKNOWN */
                            *out = UNKNOWN;

                            if ( !PORT_NULL(out) ) {
                                OUTPUT_STATE(out) = UNKNOWN;
                                OUTPUT_DELAY(out) = PARAM(reset_delay);
                            }
                            if ( !PORT_NULL(Nout) ) {
                                OUTPUT_STATE(Nout) = UNKNOWN;
                                OUTPUT_DELAY(Nout) = PARAM(reset_delay);
                            }
                        }
                        else {
                            *out = *out_old;     /* output already unknown */
                            if ( !PORT_NULL(out) ) {
                                OUTPUT_CHANGED(out) = FALSE;
                            }
                            if ( !PORT_NULL(Nout) ) {
                                OUTPUT_CHANGED(Nout) = FALSE;
                            }
                        }
                    }
                    break;
                          

                case ZERO:
                case UNKNOWN:
                    if ( ONE != *set) {
                        if ( ONE == *enable ) {
                            /* active level...save & output current data value */
                            temp = *data;
    
                            if (*out_old != temp) {  /* enable will change output */

                                *out = temp;

                                if ( !PORT_NULL(out) ) {
                                    OUTPUT_STATE(out) = temp;
                                    OUTPUT_DELAY(out) = PARAM(reset_delay);
                                }

                                cm_toggle_bit(&temp);

                                if ( !PORT_NULL(Nout) ) {
                                    OUTPUT_STATE(Nout) = temp;
                                    OUTPUT_DELAY(Nout) = PARAM(reset_delay);
                                }
                            }
                            else {
                                *out = *out_old;     /* output same as before */
                                if ( !PORT_NULL(out) ) {
                                    OUTPUT_CHANGED(out) = FALSE;
                                }
                                if ( !PORT_NULL(Nout) ) {
                                    OUTPUT_CHANGED(Nout) = FALSE;
                                }
                            }
                        }
                        else {
                            /* output remains at current value */
                            *out = *out_old;
                            if ( !PORT_NULL(out) ) {
                                OUTPUT_CHANGED(out) = FALSE;
                            }
                            if ( !PORT_NULL(Nout) ) {
                                OUTPUT_CHANGED(Nout) = FALSE;
                            }
                        }
                    }
                    else {
                        if (*out_old != ONE) {  /* reset will change output */
                            /* output returns to set condition */
                            *out = ONE;

                            if ( !PORT_NULL(out) ) {
                                OUTPUT_STATE(out) = ONE;
                                OUTPUT_DELAY(out) = PARAM(reset_delay);
                            }
                            if ( !PORT_NULL(Nout) ) {
                                OUTPUT_STATE(Nout) = ZERO;
                                OUTPUT_DELAY(Nout) = PARAM(reset_delay);
                            }
                        }
                        else {
                            *out = *out_old;     /* output already reset */
                            if ( !PORT_NULL(out) ) {
                                OUTPUT_CHANGED(out) = FALSE;
                            }
                            if ( !PORT_NULL(Nout) ) {
                                OUTPUT_CHANGED(Nout) = FALSE;
                            }
                        }
                    }
                    break;
                }
            }
            else {
                                                            
                /**** Test for enable change... ****/
                if ( (*enable != *enable_old) && (*reset != ONE) &&
                     (*set != ONE) ) { /* enable or enable release */
                    switch ( *enable ) {          

                    case ONE:
                        /* active edge...save & output current data value */
                        temp = *data;

                        if (*out_old != temp) {  /* enable will change output */

                            *out = temp;

                            if ( !PORT_NULL(out) ) {
                                OUTPUT_STATE(out) = temp;
                                OUTPUT_DELAY(out) = PARAM(enable_delay);
                            }

                            cm_toggle_bit(&temp);

                            if ( !PORT_NULL(Nout) ) {
                                OUTPUT_STATE(Nout) = temp;
                                OUTPUT_DELAY(Nout) = PARAM(enable_delay);
                            }
                        }
                        else {
                            *out = *out_old;     /* output same as before */
                            if ( !PORT_NULL(out) ) {
                                OUTPUT_CHANGED(out) = FALSE;
                            }
                            if ( !PORT_NULL(Nout) ) {
                                OUTPUT_CHANGED(Nout) = FALSE;
                            }
                        }
                        break;

                    case ZERO:
                    case UNKNOWN:
                        /* inactive edge...return previous values */
                        *out = *out_old;
                        if ( !PORT_NULL(out) ) {
                            OUTPUT_CHANGED(out) = FALSE;
                        }
                        if ( !PORT_NULL(Nout) ) {
                            OUTPUT_CHANGED(Nout) = FALSE;
                        }
                        break;
                    }
                }

                else {   /* test data value for change... */

                    if ( (*data != *data_old) && (*reset != ONE) &&
                         (*set != ONE) ) {
                         /* data value has changed...
                            test enable, and if active, update
                            the output...else return w/o change. */
                        switch ( *enable ) {          

                        case ONE:
                            /* active level...save & output current data value */
                            temp = *data;
    
                            if (*out_old != temp) {  /* enable will change output */
                                *out = temp;

                                if ( !PORT_NULL(out) ) {
                                    OUTPUT_STATE(out) = temp;
                                    OUTPUT_DELAY(out) = PARAM(data_delay);
                                }
                                if ( !PORT_NULL(Nout) ) {
                                    cm_toggle_bit(&temp);
                                    OUTPUT_STATE(Nout) = temp;
                                    OUTPUT_DELAY(Nout) = PARAM(data_delay);
                                }
                            }
                            else {
                                *out = *out_old;     /* output same as before */
                                if ( !PORT_NULL(out) ) {
                                    OUTPUT_CHANGED(out) = FALSE;
                                }
                                if ( !PORT_NULL(Nout) ) {
                                    OUTPUT_CHANGED(Nout) = FALSE;
                                }
                            }
                            break;
    
                        case ZERO:
                        case UNKNOWN:
                            /* inactive level...return previous values */
                            *out = *out_old;
                            if ( !PORT_NULL(out) ) {
                                OUTPUT_CHANGED(out) = FALSE;
                            }
                            if ( !PORT_NULL(Nout) ) {
                                OUTPUT_CHANGED(Nout) = FALSE;
                            }
                            break;
                        }
                    }
                    else { /* nothing has changed!!! This shouldn't happen! */
                        *out = *out_old;
                        if ( !PORT_NULL(out) ) {
                            OUTPUT_CHANGED(out) = FALSE;
                        }
                        if ( !PORT_NULL(Nout) ) {
                            OUTPUT_CHANGED(Nout) = FALSE;
                        }
                    }
                }
            }
        }

        /***** Add additional rise or fall delays, if appropriate *****/

        if ( *out != *out_old ) { /*** output value is changing ***/

            switch ( *out ) {
                                                 
            /** fall to zero value **/
            case 0: 
                    if ( !PORT_NULL(out) ) {
                        OUTPUT_DELAY(out) += PARAM(fall_delay);
                    }
                    if ( !PORT_NULL(Nout) ) {
                        OUTPUT_DELAY(Nout) += PARAM(rise_delay);
                    }
                    break;
    
            /** rise to one value **/
            case 1: 
                    if ( !PORT_NULL(out) ) {
                        OUTPUT_DELAY(out) += PARAM(rise_delay);
                    }
                    if ( !PORT_NULL(Nout) ) {
                        OUTPUT_DELAY(Nout) += PARAM(fall_delay);
                    }
                    break;
                                    
            /** unknown output **/
            default:
                    /* based on old value, add rise or fall delay */
                    if (0 == *out_old) {  /* add rising delay */
                        if ( !PORT_NULL(out) ) {
                            OUTPUT_DELAY(out) += PARAM(rise_delay);
                        }
                        if ( !PORT_NULL(Nout) ) {
                            OUTPUT_DELAY(Nout) += PARAM(fall_delay);
                        }
                    }
                    else {                /* add falling delay */
                        if ( !PORT_NULL(out) ) {
                            OUTPUT_DELAY(out) += PARAM(fall_delay);
                        }
                        if ( !PORT_NULL(Nout) ) {
                            OUTPUT_DELAY(Nout) += PARAM(rise_delay);
                        }
                    }   
                    break;
            }
        }
    }
              
    /*** output strength values ***/
    if ( !PORT_NULL(out) ) {
        OUTPUT_STRENGTH(out) = STRONG;
    }
    if ( !PORT_NULL(Nout) ) {
        OUTPUT_STRENGTH(Nout) = STRONG;
    }
} 

      



