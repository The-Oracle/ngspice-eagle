#line 1 "./analog/pwl/cfunc.mod"
#include "ngspice/cm.h"
extern void cm_pwl(Mif_Private_t *);
#line 1 "./analog/pwl/cfunc.mod"
/*.......1.........2.........3.........4.........5.........6.........7.........8
================================================================================

FILE pwl/cfunc.mod

Copyright 1991
Georgia Tech Research Corporation, Atlanta, Ga. 30332
All Rights Reserved

PROJECT A-8503-405
               

AUTHORS                      

    19 Apr 1991     Jeffrey P. Murray


MODIFICATIONS   

    25 Sep 1991    Jeffrey P. Murray
     2 Oct 1991    Jeffrey P. Murray
                                   

SUMMARY

    This file contains the model-specific routines used to
    functionally describe the pwl (piece-wise linear) code model.


INTERFACES       

    FILE                 ROUTINE CALLED     

    CMutil.c             void cm_smooth_corner(); 

    CMmacros.h           cm_message_send();                   
                             
    CM.c                 void cm_analog_not_converged()


REFERENCED FILES

    Inputs from and outputs to ARGS structure.
                     

NON-STANDARD FEATURES

    NONE

===============================================================================*/

/*=== INCLUDE FILES ====================*/

#include <math.h>

                                      

/*=== CONSTANTS ========================*/

#define FRACTION 0.30
#define EPSILON 1.0e-9



/*=== MACROS ===========================*/



  
/*=== LOCAL VARIABLES & TYPEDEFS =======*/                         


    
           
/*=== FUNCTION PROTOTYPE DEFINITIONS ===*/




                   
/*==============================================================================

FUNCTION double limit_x_value()

AUTHORS                      

    25 Sep 1991     Jeffrey P. Murray


MODIFICATIONS   

     2 Oct 1991    Jeffrey P. Murray

SUMMARY

    Limits a passed input value to some fraction       
    of the segment length defined by                   
    (x_upper - x_lower). The fractional value in       
    question is passed as a value to the routine       
    (fraction).                                        
                                                       

INTERFACES       

    FILE                 ROUTINE CALLED     

    CM.c                 void cm_analog_not_converged()


RETURNED VALUE
    
    Returns a double.

GLOBAL VARIABLES
    
    NONE

NON-STANDARD FEATURES

    NONE

==============================================================================*/

#include <stdlib.h>

/*=== Static LIMIT_X_VALUE ROUTINE ================*/
                                         
/** limit_x_value ******************************************/
/**                                                       **/
/**   Limits a passed input value to some fraction        **/
/**   of the segment length defined by                    **/
/**   (x_upper - x_lower). The fractional value in        **/
/**   question is passed as a value to the routine        **/
/**   (fraction).                                         **/
/**                                                       **/
/**   9/25/91                                  JPM        **/
/***********************************************************/

static double limit_x_value(double x_lower,double x_upper, 
                            double x_input,double fraction, 
                            double *last_x_value)
{
    double max_x_delta,   /* maximum delta value permissible for 
                           this segment domain. */
                  hold;   /* Holding variable for previous x_input value */     


    /** Limit effective change of input to fraction of value of lowest **/
    /** x-segment length...                                            **/ 

    /* calculate maximum delta value for this region */
    max_x_delta = fraction * (x_upper - x_lower);

    /* Test new input */
    if ( max_x_delta < fabs(x_input - *last_x_value) ) {
                        
        hold = x_input;

        /* Assign new x_input based of direction of movement */
        /* since last iteration call                         */
        if ( 0.0 <= (x_input - *last_x_value) ) {
            x_input = *last_x_value = *last_x_value + max_x_delta;
        }
        else {
            x_input = *last_x_value = *last_x_value - max_x_delta;
        }

        /* Alert the simulator to non-convergence */
        cm_analog_not_converged();

        /*** Debugging printf statement ***/
        /* printf("Assigning new x_input...\nPrevious value=%e, New value=%e\n\n",
                hold,x_input);
        */   

    }
    else { /* No limiting of x_input required */
        *last_x_value = x_input;
    }
                          
    return x_input;

}

/*==============================================================================

FUNCTION void cm_pwl(>

AUTHORS                      

    19 Apr 1991     Jeffrey P. Murray

MODIFICATIONS   

    25 Sep 1991    Jeffrey P. Murray
     2 Oct 1991    Jeffrey P. Murray

SUMMARY

    This function implements the pwl code model.

INTERFACES       

    FILE                 ROUTINE CALLED     

    CMutil.c             void cm_smooth_corner(); 

    CMmacros.h           cm_message_send();                   
                             
    CM.c                 void cm_analog_not_converged()


RETURNED VALUE
    
    Returns inputs and outputs via ARGS structure.

GLOBAL VARIABLES
    
    NONE

NON-STANDARD FEATURES

    NONE

==============================================================================*/

/*=== CM_PWL ROUTINE ================*/

void cm_pwl(Mif_Private_t *mif_private)  /* structure holding parms, 
                                   inputs, outputs, etc.     */
{
    int i;               /* generic loop counter index */
	int size;            /* size of the x_array        */

    double input_domain;    /* smoothing range */
    double *x;              /* pointer to the x-coordinate array */
    double *y;              /* pointer to the y-coordinate array */
    double lower_seg;       /* x segment below which input resides */
    double upper_seg;       /* x segment above which the input resides */
    double lower_slope;     /* slope of the lower segment */
    double upper_slope;     /* slope of the upper segment */
    double x_input;         /* input */
    double out;             /* output */
    double dout_din;        /* partial derivative of the output wrt input */
    double threshold_lower; /* value below which the output begins smoothing */
    double threshold_upper; /* value above which the output begins smoothing */
    double test1;           /* debug testing value */
    double test2;           /* debug testing value */
    double *last_x_value;   /* static variable for limiting */
    double test;            /* temp storage variable for limit testing */

    Mif_Complex_t ac_gain;
                                           
    char *allocation_error="\n***ERROR***\nPWL: Allocation calloc failed!\n";
    char *limit_error="\n***ERROR***\nPWL: Violation of 50% rule in breakpoints!\n";

    /* Retrieve frequently used parameters... */

    input_domain = mif_private->param[2]->element[0].rvalue;

    size = mif_private->param[0]->size;           


      
    if (mif_private->circuit.init==1) {  /* First pass...allocate storage for previous value... */

        /* Allocate storage for last_x_value */
        mif_private->inst_var[0]->element[0].pvalue = (double *) malloc(sizeof(double));
        last_x_value = (double *) mif_private->inst_var[0]->element[0].pvalue;

        /* Allocate storage for breakpoint domain & range values */
        mif_private->inst_var[1]->element[0].pvalue = (double *) calloc((size_t) size, sizeof(double));
        x = (double *) mif_private->inst_var[1]->element[0].pvalue;
        if (!x) {
            cm_message_send(allocation_error); 
        }

        mif_private->inst_var[2]->element[0].pvalue = (double *) calloc((size_t) size, sizeof(double));
        y = (double *) mif_private->inst_var[2]->element[0].pvalue;
        if (!y) {
            cm_message_send(allocation_error);  
        }

        /* Retrieve x and y values. */       
        for (i=0; i<size; i++) {
            x[i] = mif_private->param[0]->element[i].rvalue;
            y[i] = mif_private->param[1]->element[i].rvalue;
        }                       

    }  
    else {

        last_x_value = (double *) mif_private->inst_var[0]->element[0].pvalue;

        x = (double *) mif_private->inst_var[1]->element[0].pvalue;

        y = (double *) mif_private->inst_var[2]->element[0].pvalue;

    }





    /* See if input_domain is absolute...if so, test against   */
    /* breakpoint segments for violation of 50% rule...        */
    if (mif_private->param[3]->element[0].bvalue == MIF_FALSE) {
        if ( 3 < size ) {
            for (i=1; i<(size-2); i++) { 
                /* Test for overlap...0.999999999 factor is to      */
                /* prevent floating point problems with comparison. */
                if ( (test1 = x[i+1] - x[i]) < 
                     (test2 = 0.999999999 * (2.0 * input_domain)) ) {
                    cm_message_send(limit_error);          
                }    
            }
        }

    }


    /* Retrieve x_input value. */       
    x_input = mif_private->conn[0]->port[0]->input.rvalue;
                                      

    /* If this is the first call, set *last_x_value to x_input */
    if (mif_private->circuit.init == 1) 
        *last_x_value=x_input;


    /*** Add debugging printf statement ***/
    /* printf("Last x_input=%e, Current x_input=%e,\n",
            *last_x_value,x_input);
    */
                
                                                              
    /**** Add internal limiting to input value ****/

    /* Determine region of input, and limit accordingly */
    if ( *last_x_value < x[0] ) { /** Non-limited input less than x[0] **/

        /* Obtain the test value of the input, if it has changed excessively */
        if ( (x[0] - x_input) > (x[1] - x[0]) ) {
            test = limit_x_value(x_input,x[0],x_input,FRACTION,last_x_value);
        }
        else {
            test = limit_x_value(x[0],x[1],x_input,FRACTION,last_x_value);
        }
        
        /* If the test value is greater than x[0], force to x[0]  */
        if ( test >= x[0] ) {
            x_input = *last_x_value = x[0];

            /* Alert the simulator to non-convergence */
            cm_analog_not_converged();
        }                                               
        else {
            x_input = *last_x_value = test;
        }
    }
    else 
    if ( *last_x_value >= x[size-1] ) { /** Non-Limited input greater than x[size-1] **/

        /* Obtain the test value of the input, if it has changed excessively */
        if ( (x_input - x[size-1]) > (x[size-1] - x[size-2]) ) {
            test = limit_x_value(x[size-1],x_input,x_input,FRACTION,last_x_value);
        }
        else {
            test = limit_x_value(x[size-2],x[size-1],x_input,FRACTION,last_x_value);
        }
        
        /* If the test value is less than x[size-1], force to x[size-1]  */
        /* minus some epsilon value.                                     */
        if ( test < x[size-1] ) {
            x_input = *last_x_value = x[size-1] - EPSILON;

            /* Alert the simulator to non-convergence */
            cm_analog_not_converged();
        }                                               
        else {
            x_input = *last_x_value = test;
        }
    }
    else {
        for (i=1; i<size; i++) {
            if ( *last_x_value < x[i] ) {
                              
                /* Obtain the test value of the input */
                test = limit_x_value(x[i-1],x[i],x_input,FRACTION,last_x_value);
                
                /* If the test value is greater than x[i], force to x[i]  */
                if ( test > x[i] ) {
                    x_input = *last_x_value = x[i];

                    /* Alert the simulator to non-convergence */
                    cm_analog_not_converged();

                    break;
                }                                               
                else 
                /* If the test value is less than x[i-1], force to x[i-1]  */
                /* minus some epsilon value...                             */
                if ( test < x[i-1] ) {
                    x_input = *last_x_value = x[i-1] - EPSILON;

                    /* Alert the simulator to non-convergence */
                    cm_analog_not_converged();

                    break;
                }
                else { /* Use returned value for next input */
                    x_input = *last_x_value = test; 
                    break;
                }
            }
        }
    }

    /* Assign new limited value back to the input for */
    /* use in the matrix calculations....             */       
    mif_private->conn[0]->port[0]->input.rvalue = x_input; 
    

    /*** Add debugging printf statement ***/
    /* printf("Limited x_input=%e\n\n",
            x_input);
    */


    /**** End internal limiting ****/                                               



    /* Determine segment boundaries within which x_input resides */

    if (x_input <= (x[0] + x[1])/2.0) {/*** x_input below lowest midpoint ***/
        dout_din = (y[1] - y[0])/(x[1] - x[0]);

                    
        /* Compute new output */
        out = y[0] + (x_input - x[0]) * dout_din;
       
    }
    else {
        if (x_input >= (x[size-2] + x[size-1])/2.0) { 
                                   /*** x_input above highest midpoint ***/
            dout_din = (y[size-1] - y[size-2]) /
                          (x[size-1] - x[size-2]);

            out = y[size-1] + (x_input - x[size-1]) * dout_din;
        }
        else { /*** x_input within bounds of end midpoints...     ***/
               /*** must determine position progressively & then  ***/
               /*** calculate required output.                    ***/

            for (i=1; i<size; i++) {
                    
                if (x_input < (x[i] + x[i+1])/2.0) { 
                                   /* approximate position known...          */
             
                    lower_seg = (x[i] - x[i-1]);
                    upper_seg = (x[i+1] - x[i]);


                    /* Calculate input_domain about this region's breakpoint.*/

                    if (mif_private->param[3]->element[0].bvalue == MIF_TRUE) {  /* Translate input_domain */
                                                  /* into an absolute....   */
                        if ( lower_seg <= upper_seg )          /* Use lower  */
                                                               /* segment    */
                                                               /* for % calc.*/
                            input_domain = input_domain * lower_seg;
                        else                                   /* Use upper  */
                                                               /* segment    */
                                                               /* for % calc.*/
                            input_domain = input_domain * upper_seg;
                    } 
    
                    /* Set up threshold values about breakpoint... */
                    threshold_lower = x[i] - input_domain;
                    threshold_upper = x[i] + input_domain;

                    /* Determine where x_input is within region & determine */
                    /* output and partial values....                        */
                    if (x_input < threshold_lower) { /* Lower linear region */
                        dout_din = (y[i] - y[i-1])/lower_seg;

                        out = y[i] + (x_input - x[i]) * dout_din;
                    }
                    else {        
                        if (x_input < threshold_upper) { /* Parabolic region */
                            lower_slope = (y[i] - y[i-1])/lower_seg;
                            upper_slope = (y[i+1] - y[i])/upper_seg;

                            cm_smooth_corner(x_input,x[i],y[i],input_domain,
                                        lower_slope,upper_slope,&out,&dout_din);
                        }
                        else {        /* Upper linear region */
                            dout_din = (y[i+1] - y[i])/upper_seg;
                            out = y[i] + (x_input - x[i]) * dout_din;
                        }
                    }
                    break;  /* Break search loop...x_input has been found, */
                            /* and out and dout_din have been assigned.    */
                }
            }
        }
    }


    if(mif_private->circuit.anal_type != MIF_AC) {        /* Output DC & Transient Values */
        mif_private->conn[1]->port[0]->output.rvalue = out;
        mif_private->conn[1]->port[0]->partial[0].port[0] = dout_din;
    }
    else {                      /* Output AC Gain */
        ac_gain.real = dout_din;
        ac_gain.imag= 0.0;
        mif_private->conn[1]->port[0]->ac_gain[0].port[0] = ac_gain;
    }
} 

