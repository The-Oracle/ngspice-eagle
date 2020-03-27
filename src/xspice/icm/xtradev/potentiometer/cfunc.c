#line 1 "./xtradev/potentiometer/cfunc.mod"
#include "ngspice/cm.h"
extern void cm_potentiometer(Mif_Private_t *);
#line 1 "./xtradev/potentiometer/cfunc.mod"
/*.......1.........2.........3.........4.........5.........6.........7.........8
================================================================================

FILE potentiometer/cfunc.mod

Copyright 1991
Georgia Tech Research Corporation, Atlanta, Ga. 30332
All Rights Reserved

PROJECT A-8503-405
               

AUTHORS                      

    19 June 1992     Jeffrey P. Murray


MODIFICATIONS   

    19 June 1992     Jeffrey P. Murray
                                   

SUMMARY

    This file contains the functional description of the potentiometer
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

FUNCTION cm_potentiometer()

AUTHORS                      

    19 June 1992     Jeffrey P. Murray

MODIFICATIONS   

    19 June 1992     Jeffrey P. Murray

SUMMARY

    This function implements the potentiometer code model.

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

/*=== CM_POTENTIOMETER ROUTINE ===*/


void cm_potentiometer (Mif_Private_t *mif_private)
{
    double position;     /* position of wiper contact */
    double resistance;   /* total resistance */
    double r_lower;      /* resistance from r0 to wiper */
    double r_upper;      /* resistance from wiper to r1 */
    double vr0;          /* voltage at r0 */
    double vr1;          /* voltage at r1 */
    double vwiper;       /* voltage at wiper */



    Mif_Complex_t ac_gain;
                   
                       

    /* Retrieve frequently used parameters... */

    position = mif_private->param[0]->element[0].rvalue;
    resistance = mif_private->param[2]->element[0].rvalue;

    /* Retrieve input voltages... */
    vr0 = mif_private->conn[0]->port[0]->input.rvalue;
    vwiper = mif_private->conn[1]->port[0]->input.rvalue;
    vr1 = mif_private->conn[2]->port[0]->input.rvalue;


    if ( mif_private->param[1]->element[0].bvalue == FALSE ) {   

        /* Linear Variation in resistance w.r.t. position */
        r_lower = position * resistance;
        r_upper = resistance - r_lower;

    }
    else {        
        
        /* Logarithmic Variation in resistance w.r.t. position */
        r_lower = resistance / 
                  pow(10.0,(position * mif_private->param[3]->element[0].rvalue));
        r_upper = resistance - r_lower;

    }





    /* Output DC & Transient Values  */

    if(mif_private->circuit.anal_type != MIF_AC) {               
        mif_private->conn[0]->port[0]->output.rvalue = (vr0 - vwiper) / r_lower;
        mif_private->conn[2]->port[0]->output.rvalue = (vr1 - vwiper) / r_upper;
        mif_private->conn[1]->port[0]->output.rvalue = ((vwiper - vr0)/r_lower) + ((vwiper - vr1)/r_upper);

        mif_private->conn[0]->port[0]->partial[0].port[0] = 1.0 / r_lower;
        mif_private->conn[0]->port[0]->partial[2].port[0] = 0.0;
        mif_private->conn[0]->port[0]->partial[1].port[0] = -1.0 / r_lower;

        mif_private->conn[2]->port[0]->partial[0].port[0] = 0.0;
        mif_private->conn[2]->port[0]->partial[2].port[0] = 1.0 / r_upper;
        mif_private->conn[2]->port[0]->partial[1].port[0] = -1.0 / r_upper;

        mif_private->conn[1]->port[0]->partial[0].port[0] = -1.0 / r_lower;
        mif_private->conn[1]->port[0]->partial[2].port[0] = -1.0 / r_upper;
        mif_private->conn[1]->port[0]->partial[1].port[0] = (1.0/r_lower) + (1.0/r_upper);

    }
    else {                       

        /*   Output AC Gain Values      */

        ac_gain.imag= 0.0;              

        ac_gain.real = -1.0 / r_lower;
        mif_private->conn[0]->port[0]->ac_gain[0].port[0] = ac_gain;

        ac_gain.real = 0.0;             
        mif_private->conn[0]->port[0]->ac_gain[2].port[0] = ac_gain;

        ac_gain.real = 1.0 / r_lower;             
        mif_private->conn[0]->port[0]->ac_gain[1].port[0] = ac_gain;

        ac_gain.real = 0.0;
        mif_private->conn[2]->port[0]->ac_gain[0].port[0] = ac_gain;

        ac_gain.real = -1.0 / r_upper;             
        mif_private->conn[2]->port[0]->ac_gain[2].port[0] = ac_gain;

        ac_gain.real = 1.0 / r_upper;             
        mif_private->conn[2]->port[0]->ac_gain[1].port[0] = ac_gain;

        ac_gain.real = 1.0 / r_lower;
        mif_private->conn[1]->port[0]->ac_gain[0].port[0] = ac_gain;

        ac_gain.real = 1.0 / r_upper;             
        mif_private->conn[1]->port[0]->ac_gain[2].port[0] = ac_gain;

        ac_gain.real = -(1.0/r_lower) - (1.0/r_upper);             
        mif_private->conn[1]->port[0]->ac_gain[1].port[0] = ac_gain;

    }

}




