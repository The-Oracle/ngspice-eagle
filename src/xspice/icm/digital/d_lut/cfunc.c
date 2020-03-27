#line 1 "./digital/d_lut/cfunc.mod"
#include "ngspice/cm.h"
extern void cm_d_lut(Mif_Private_t *);
#line 1 "./digital/d_lut/cfunc.mod"
/*.......1.........2.........3.........4.........5.........6.........7.........8
================================================================================

FILE d_lut/cfunc.mod

AUTHORS

    25 Aug 2016     Tim Edwards         efabless inc., San Jose, CA

SUMMARY

    This file contains the functional description of the d_lut
    code model.

LICENSE

    This software is in the public domain.

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

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <string.h>



/*=== CONSTANTS ========================*/




/*=== MACROS ===========================*/




/*=== LOCAL VARIABLES & TYPEDEFS =======*/




/*=== FUNCTION PROTOTYPE DEFINITIONS ===*/





/*==============================================================================

FUNCTION cm_d_lut()

AUTHORS

    25 Aug 2016     Tim Edwards

SUMMARY

    This function implements the d_lut code model.

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

/*=== CM_D_LUT ROUTINE ===*/

/************************************************
*      The following is the model for the       *
*   digital n-input LUT gate                    *
*                                               *
*   Created 8/25/16               Tim Edwards   *
************************************************/


void cm_d_lut(Mif_Private_t *mif_private)
{
    int         i,      /* generic loop counter index */
                j,      /* lookup index bit value */
                idx,    /* lookup index */
                size,   /* number of input & output ports */
                tablelen; /* length of table (2^size) */

    char        *table_string;

    Digital_State_t *out,      /* temporary output for buffers */
                    *out_old,  /* previous output for buffers  */
                    input,     /* temp storage for input bits  */
                    *lookup_table; /* lookup table */

    /** Retrieve size value and compute table length... **/
    size = mif_private->conn[0]->size;
    tablelen = 1 << size;

    /*** Setup required state variables ***/

    if (mif_private->circuit.init) {  /* initial pass */

        /* allocate storage for the lookup table */
         mif_private->inst_var[0]->element[0].pvalue = calloc((size_t) tablelen, sizeof(Digital_State_t));
        lookup_table =  mif_private->inst_var[0]->element[0].pvalue;

        /* allocate storage for the outputs */
        cm_event_alloc(0, sizeof(Digital_State_t));
        cm_event_alloc(1, size * (int) sizeof(Digital_State_t));

        /* set loading for inputs */
        for (i = 0; i < size; i++)
            mif_private->conn[0]->port[i]->load = mif_private->param[2]->element[0].rvalue;

        /* retrieve storage for the outputs */
        out = out_old = (Digital_State_t *) cm_event_get_ptr(0, 0);

        /* read parameter string into lookup table */
        table_string = mif_private->param[3]->element[0].svalue;
        for (idx = 0; idx < (int) strlen(table_string); idx++) {
            if (idx == tablelen)
                // If string is longer than 2^num_inputs, ignore
                // the extra values at the end
                break;
            if (table_string[idx] == '1')
                lookup_table[idx] = ONE;
            else if (table_string[idx] == '0')
                lookup_table[idx] = ZERO;
            else
                lookup_table[idx] = UNKNOWN;
        }
        // If string is shorter than 2^num_inputs, fill
        // the remainder of the lookup table with UNKNOWN values.
        for (; idx < tablelen; idx++)
            lookup_table[idx] = UNKNOWN;
    }
    else {      /* Retrieve previous values */

        /* retrieve lookup table */
        lookup_table =  mif_private->inst_var[0]->element[0].pvalue;

        /* retrieve storage for the outputs */
        out = (Digital_State_t *) cm_event_get_ptr(0, 0);
        out_old = (Digital_State_t *) cm_event_get_ptr(0, 1);
    }

    /*** Calculate new output value based on inputs and table ***/

    *out = ZERO;
    j = 1;
    idx = 0;
    for (i = 0; i < size; i++) {

        /* make sure this input isn't floating... */
        if (mif_private->conn[0]->is_null == FALSE) {

            /* use inputs to find index into lookup table */
            if ((input = ((Digital_t*)(mif_private->conn[0]->port[i]->input.pvalue))->state) == UNKNOWN) {
                *out = UNKNOWN;
                break;
            }
            else if (input == ONE) {
                idx += j;
            }
            j <<= 1;
        }
        else {
            /* at least one port is floating...output is unknown */
            *out = UNKNOWN;
            break;
        }
    }

    if (*out != UNKNOWN)
       *out = lookup_table[idx];

    /*** Determine analysis type and output appropriate values ***/

    if (mif_private->circuit.anal_type == DC) {   /** DC analysis...output w/o delays **/

        ((Digital_t*)(mif_private->conn[1]->port[0]->output.pvalue))->state = *out;

    }

    else {      /** Transient Analysis **/

        if (*out != *out_old) { /* output value is changing */

            switch (*out) {

            /* fall to zero value */
            case 0:
                ((Digital_t*)(mif_private->conn[1]->port[0]->output.pvalue))->state = ZERO;
                mif_private->conn[1]->port[0]->delay = mif_private->param[1]->element[0].rvalue;
                break;

            /* rise to one value */
            case 1:
                ((Digital_t*)(mif_private->conn[1]->port[0]->output.pvalue))->state = ONE;
                mif_private->conn[1]->port[0]->delay = mif_private->param[0]->element[0].rvalue;
                break;

            /* unknown output */
            default:
                ((Digital_t*)(mif_private->conn[1]->port[0]->output.pvalue))->state = *out = UNKNOWN;

                /* based on old value, add rise or fall delay */
                if (0 == *out_old)
                    mif_private->conn[1]->port[0]->delay = mif_private->param[0]->element[0].rvalue;
                else
                    mif_private->conn[1]->port[0]->delay = mif_private->param[1]->element[0].rvalue;
                break;
            }
        }
        else {                    /* output value not changing */
            mif_private->conn[1]->port[0]->changed = FALSE;
        }
    }

    ((Digital_t*)(mif_private->conn[1]->port[0]->output.pvalue))->strength = STRONG;
}
