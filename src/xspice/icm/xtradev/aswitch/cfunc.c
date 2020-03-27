#line 1 "./xtradev/aswitch/cfunc.mod"
#include "ngspice/cm.h"
extern void cm_aswitch(Mif_Private_t *);
#line 1 "./xtradev/aswitch/cfunc.mod"
/*.......1.........2.........3.........4.........5.........6.........7.........8
================================================================================

FILE aswitch/cfunc.mod

Copyright 1991
Georgia Tech Research Corporation, Atlanta, Ga. 30332
All Rights Reserved

PROJECT A-8503-405
               

AUTHORS                      

    6 June 1991     Jeffrey P. Murray


MODIFICATIONS   

    26 Sept 1991    Jeffrey P. Murray
                                   

SUMMARY

    This file contains the functional description of the aswitch
    code model.


INTERFACES       

    FILE                 ROUTINE CALLED     

    CMmacros.h           cm_message_send();                   


REFERENCED FILES

    Inputs from and outputs to ARGS structure.
                     

NON-STANDARD FEATURES

    NONE

===============================================================================*/

/*=== INCLUDE FILES ====================*/

#include <math.h>

                                      

/*=== CONSTANTS ========================*/




/*=== MACROS ===========================*/



  
/*=== LOCAL VARIABLES & TYPEDEFS =======*/                         


    
           
/*=== FUNCTION PROTOTYPE DEFINITIONS ===*/






                   
/*==============================================================================

FUNCTION cm_aswitch()

AUTHORS                      

    6 June 1991     Jeffrey P. Murray

MODIFICATIONS   

    26 Sept 1991    Jeffrey P. Murray

SUMMARY

    This function implements the aswitch code model.

INTERFACES       

    FILE                 ROUTINE CALLED     

    CMmacros.h           cm_message_send();                   

RETURNED VALUE
    
    Returns inputs and outputs via ARGS structure.

GLOBAL VARIABLES
    
    NONE

NON-STANDARD FEATURES

    NONE

==============================================================================*/

/*=== CM_ASWITCH ROUTINE ===*/



void cm_aswitch(Mif_Private_t *mif_private)  /* structure holding parms, 
                                          inputs, outputs, etc.     */
{
    double cntl_on;      /* voltage above which switch come on */
	double cntl_off;     /* voltage below the switch has resistance roff */ 
	double r_on;         /* on resistance */
	double r_off;        /* off resistance */
	double intermediate; /* intermediate value used to calculate
				the resistance of the switch when the
				controlling voltage is between cntl_on
				and cntl_of */
	double r;            /* value of the resistance of the switch */
    double pi_pvout;     /* partial of the output wrt input       */
	double pi_pcntl;     /* partial of the output wrt control input */

   Mif_Complex_t ac_gain;
                   
	  char *cntl_error = "\n*****ERROR*****\nASWITCH: CONTROL voltage delta less than 1.0e-12\n";
                       

    /* Retrieve frequently used parameters... */

    cntl_on = mif_private->param[1]->element[0].rvalue;
    cntl_off = mif_private->param[0]->element[0].rvalue;
    r_on = mif_private->param[4]->element[0].rvalue;
    r_off = mif_private->param[3]->element[0].rvalue;

    if( r_on < 1.0e-3 ) r_on = 1.0e-3;  /* Set minimum 'ON' resistance */  

    if( (fabs(cntl_on - cntl_off) < 1.0e-12) ) {
        cm_message_send(cntl_error);          
        return;
    }

    if ( mif_private->param[2]->element[0].bvalue == MIF_TRUE ) {   /* Logarithmic Variation in 'R' */
        intermediate = log(r_off / r_on) / (cntl_on - cntl_off);
        r = r_on * exp(intermediate * (cntl_on - mif_private->conn[0]->port[0]->input.rvalue));
        if(r<=1.0e-9) r=1.0e-9;/* minimum resistance limiter */
        pi_pvout = 1.0 / r;
        pi_pcntl = intermediate * mif_private->conn[1]->port[0]->input.rvalue / r;
    }
    else {                      /* Linear Variation in 'R' */
        intermediate = (r_on - r_off) / (cntl_on - cntl_off);
        r = mif_private->conn[0]->port[0]->input.rvalue * intermediate + ((r_off*cntl_on - 
                r_on*cntl_off) / (cntl_on - cntl_off));
        if(r<=1.0e-9) r=1.0e-9;/* minimum resistance limiter */
        pi_pvout = 1.0 / r;
        pi_pcntl = -intermediate * mif_private->conn[1]->port[0]->input.rvalue / (r*r);
    }                                 

    /*pi_pvout = 1.0 / r;*/


    if(mif_private->circuit.anal_type != MIF_AC) {            /* Output DC & Transient Values  */
        mif_private->conn[1]->port[0]->output.rvalue = mif_private->conn[1]->port[0]->input.rvalue / r;      /* Note that the minus   */
        mif_private->conn[1]->port[0]->partial[1].port[0] = pi_pvout;       /* Signs are required    */
        mif_private->conn[1]->port[0]->partial[0].port[0] = pi_pcntl;   /* because current is    */
                                            /* positive flowing INTO */
                                            /* rather than OUT OF a  */
                                            /* component node.       */
    }
    else {                       /*   Output AC Gain Values      */
        ac_gain.real = -pi_pvout;           /* See comment on minus   */
        ac_gain.imag= 0.0;                  /* signs above....        */
        mif_private->conn[1]->port[0]->ac_gain[1].port[0] = ac_gain;

        ac_gain.real = -pi_pcntl;
        ac_gain.imag= 0.0;
        mif_private->conn[1]->port[0]->ac_gain[0].port[0] = ac_gain;
    }
} 

