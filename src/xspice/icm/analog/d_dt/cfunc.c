#line 1 "./analog/d_dt/cfunc.mod"
#include "ngspice/cm.h"
extern void cm_d_dt(Mif_Private_t *);
#line 1 "./analog/d_dt/cfunc.mod"
/*.......1.........2.........3.........4.........5.........6.........7.........8
================================================================================

FILE d_dt/cfunc.mod

Copyright 1991
Georgia Tech Research Corporation, Atlanta, Ga. 30332
All Rights Reserved

PROJECT A-8503-405
               

AUTHORS                      

    6 June 1991     Jeffrey P. Murray


MODIFICATIONS   

    30 Sept 1991    Jeffrey P. Murray
                                   

SUMMARY

    This file contains the functional description of the d_dt
    (differentiator) code model.


INTERFACES       

    FILE                 ROUTINE CALLED     

    CMutil.c             void cm_smooth_corner(); 

    CM.c                 void *cm_analog_alloc()
                         void *cm_analog_get_ptr()
                         int  cm_analog_integrate()
       


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

FUNCTION cm_d_dt()

AUTHORS                      

     6 Jun 1991     Jeffrey P. Murray

MODIFICATIONS   

    30 Sep 1991     Jeffrey P. Murray

SUMMARY

    This function implements the d_dt code model.

INTERFACES       

    FILE                 ROUTINE CALLED     

    CMutil.c             void cm_smooth_corner(); 

    CM.c                 void *cm_analog_alloc()
                         void *cm_analog_get_ptr()
                         int  cm_analog_integrate()

RETURNED VALUE
    
    Returns inputs and outputs via ARGS structure.

GLOBAL VARIABLES
    
    NONE

NON-STANDARD FEATURES

    NONE

==============================================================================*/

/*=== CM_D_DT ROUTINE ===*/
                                                   

void cm_d_dt(Mif_Private_t *mif_private)   

{

    double        *in, /* current input value   */
              *in_old, /* previous input value  */
                  out, /* output    */
                  dum, /* fake input value...used for truncation
                          error checking */
                 gain, /* gain parameter    */
           out_offset, /* output offset parameter   */
      out_lower_limit, /* output mower limit    */
      out_upper_limit, /* output upper limit    */
          limit_range, /* range of output below out_upper_limit
                          or above out_lower_limit to which
                          smoothing will be applied */
             pout_pin, /* partial derivative of output
                          w.r.t. input  */
          dumpout_pin, /* fake partial derivative of output
                          w.r.t. input (for use with integration */
                delta, /* delta time value = TIME - T(1) */
            pout_gain; /* temporary storage for partial
                          returned by smoothing function 
                          (subsequently multiplied w/pout_pin)  */

    Mif_Complex_t ac_gain;  /* AC gain  */
                                                   


    /** Retrieve frequently used parameters (used by all analyses)... **/

    gain = mif_private->param[1]->element[0].rvalue;
                                     


    if (mif_private->circuit.anal_type != MIF_AC) {     /**** DC & Transient Analyses ****/

        /** Retrieve frequently used parameters... **/

        out_offset = mif_private->param[0]->element[0].rvalue;
        out_lower_limit = mif_private->param[2]->element[0].rvalue;
        out_upper_limit = mif_private->param[3]->element[0].rvalue;                         
        limit_range = mif_private->param[4]->element[0].rvalue;


        /** Test for INIT; if so, allocate storage, otherwise, retrieve
                                   previous timepoint input value...     **/

        if (mif_private->circuit.init==1) {  /* First pass...allocate storage for previous state.   */
                        /* Also, calculate roughly where the current output    */
                        /* will be and use this value to define current state. */
    
            cm_analog_alloc(TRUE,sizeof(double));   
    
        }
        /* retrieve previous values */
    
            in = (double *) cm_analog_get_ptr(TRUE,0);  /* Set out pointer to current 
                                                                time storage */    
            in_old = (double *) cm_analog_get_ptr(TRUE,1);  /* Set old-output-state pointer 
                                                       to previous time storage */    


        if ( 0.0 == mif_private->circuit.time ) {              /*** Test to see if this is the first ***/
                                          /***    timepoint calculation...if    ***/
            *in_old = *in = mif_private->conn[0]->port[0]->input.rvalue;    /***    so, return a zero d/dt value. ***/
            out = 0.0;                    /***    so, return a zero d/dt value. ***/
            pout_pin = 0.0;
        }
        else {               /*** Calculate value of d_dt.... ***/
				  delta = mif_private->circuit.time - mif_private->circuit.t[1];
            *in = mif_private->conn[0]->port[0]->input.rvalue;
            out = gain * (*in - *in_old) / delta + out_offset;
            pout_pin = gain / delta;            
        }


        /*** Smooth output if it is within limit_range of 
                 out_lower_limit or out_upper_limit.          ***/
                                                                  
        if (out < (out_lower_limit - limit_range)) {  /* At lower limit. */ 
            out = out_lower_limit;
            pout_pin = 0.0;
        }
        else {
            if (out < (out_lower_limit + limit_range)) {  /* Lower smoothing range */
                cm_smooth_corner(out,out_lower_limit,out_lower_limit,limit_range,
                            0.0,1.0,&out,&pout_gain);
                pout_pin = pout_pin * pout_gain;
            }
            else {
                if (out > (out_upper_limit + limit_range))  {  /* At upper limit */
                    out = out_upper_limit;
                    pout_pin = 0.0;
                }
                else { 
                    if (out > (out_upper_limit - limit_range))  {  /* Upper smoothing region */
                        cm_smooth_corner(out,out_upper_limit,out_upper_limit,limit_range,
                                    1.0,0.0,&out,&pout_gain); 
                        pout_pin = pout_pin * pout_gain;
                    }
                }   
            }
        }

        /** Output values for DC & Transient **/

        mif_private->conn[1]->port[0]->output.rvalue = out;          
        mif_private->conn[1]->port[0]->partial[0].port[0] = pout_pin; 
		/* this cm_analog_integrate call is required in order to force
		   truncation error to be evaluated */
		cm_analog_integrate(out,&dum,&dumpout_pin);

    }

    else {                    /**** AC Analysis...output (0.0,s*gain) ****/
        ac_gain.real = 0.0;
        ac_gain.imag= gain * mif_private->circuit.frequency;
        mif_private->conn[1]->port[0]->ac_gain[0].port[0] = ac_gain;
    }
}





