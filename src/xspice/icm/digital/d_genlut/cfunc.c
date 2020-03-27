#line 1 "./digital/d_genlut/cfunc.mod"
#include "ngspice/cm.h"
extern void cm_d_genlut(Mif_Private_t *);
#line 1 "./digital/d_genlut/cfunc.mod"
/*.......1.........2.........3.........4.........5.........6.........7.........8
================================================================================

FILE d_genlut/cfunc.mod

AUTHORS

    25 Aug 2016     Tim Edwards         efabless inc., San Jose, CA

SUMMARY

    This file contains the functional description of the d_genlut
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

FUNCTION cm_d_genlut()

AUTHORS

    25 Aug 2016     Tim Edwards

SUMMARY

    This function implements the d_genlut code model.

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


void cm_d_genlut(Mif_Private_t *mif_private)
{
    int i,          /* generic loop counter index */
        j,          /* lookup index bit value */
        k,          /* generic loop counter index */
        idx,        /* lookup index */
        ivalid,     /* check for valid input */
        isize,      /* number of input ports */
        osize,      /* number of output ports */
        dsize,      /* number of input delay params */
        rsize,      /* number of output rise delay params */
        fsize,      /* number of output fall delay params */
        lsize,      /* number of input load params */
        entrylen,   /* length of table per output (2^isize) */
        tablelen;   /* length of table (osize * (2^isize)) */

    char        *table_string;
    double      maxdelay,  /* maximum input-to-output delay */
        testdelay;

    Digital_State_t *in,       /* temp storage for input bits  */
        *in_old;   /* previous input for buffers  */
    Digital_t       *out,      /* temporary output for buffers */
        *out_old,  /* previous output for buffers  */
        *lookup_table; /* lookup table */

    /** Retrieve size values and compute table length... **/
    isize = mif_private->conn[0]->size;
    osize = mif_private->conn[1]->size;

    if (mif_private->param[2]->is_null)
        lsize = 0;
    else
        lsize = mif_private->param[2]->size;

    if (mif_private->param[3]->is_null)
        dsize = 0;
    else
        dsize = mif_private->param[3]->size;

    if (mif_private->param[0]->is_null)
        rsize = 0;
    else
        rsize = mif_private->param[0]->size;

    if (mif_private->param[1]->is_null)
        fsize = 0;
    else
        fsize = mif_private->param[1]->size;

    entrylen = (1 << isize);
    tablelen = osize * entrylen;

    /*** Setup required state variables ***/

    if (mif_private->circuit.init) {  /* initial pass */

        /* allocate storage for the lookup table */
         mif_private->inst_var[0]->element[0].pvalue = calloc((size_t) tablelen, sizeof(Digital_t));
        lookup_table =  mif_private->inst_var[0]->element[0].pvalue;

        /* allocate storage for the outputs */
        cm_event_alloc(0, osize * (int) sizeof(Digital_t));
        cm_event_alloc(1, isize * (int) sizeof(Digital_State_t));

        /* set loading for inputs */
        for (i = 0; i < isize; i++)
            if (i < lsize)
                mif_private->conn[0]->port[i]->load = mif_private->param[2]->element[i].rvalue;
            else if (lsize > 0)
                mif_private->conn[0]->port[i]->load =   mif_private->param[2]->element[lsize-1].rvalue;
            else
                mif_private->conn[0]->port[i]->load = 1.0e-12;

        /* retrieve storage for the outputs */
        out = out_old = (Digital_t *) cm_event_get_ptr(0, 0);
        in =  in_old = (Digital_State_t *) cm_event_get_ptr(1, 0);

        /* read parameter string into lookup table */
        table_string = mif_private->param[4]->element[0].svalue;
        for (idx = 0; idx < (int)strlen(table_string); idx++) {
            if (idx == tablelen)
                // If string is longer than 2^num_inputs, ignore
                // the extra values at the end
                break;
            if (table_string[idx] == '1') {
                lookup_table[idx].state = ONE;
                lookup_table[idx].strength = STRONG;
            } else if (table_string[idx] == '0') {
                lookup_table[idx].state = ZERO;
                lookup_table[idx].strength = STRONG;
            } else if (table_string[idx] == 'z') {
                lookup_table[idx].state = UNKNOWN;
                lookup_table[idx].strength = HI_IMPEDANCE;
            } else {
                lookup_table[idx].state = UNKNOWN;
                lookup_table[idx].strength = UNDETERMINED;
            }
        }
        for (; idx < tablelen; idx++) {
            // If string is shorter than 2^num_inputs, fill
            // the remainder of the lookup table with UNKNOWN
            // values.
            lookup_table[idx].state = UNKNOWN;
            lookup_table[idx].strength = UNDETERMINED;
        }
    } else {    /* Retrieve previous values */

        /* retrieve lookup table */
        lookup_table =  mif_private->inst_var[0]->element[0].pvalue;

        /* retrieve storage for the inputs and outputs */
        out = (Digital_t *) cm_event_get_ptr(0, 0);
        out_old = (Digital_t *) cm_event_get_ptr(0, 1);
        in = (Digital_State_t *) cm_event_get_ptr(1, 0);
        in_old = (Digital_State_t *) cm_event_get_ptr(1, 1);
    }

    /*** Calculate new output value based on inputs and table ***/

    j = 1;
    idx = 0;
    ivalid = 1;
    for (k = 0; k < osize; k++) {
        out[k].state = UNKNOWN;
        out[k].strength = UNDETERMINED;
    }
    for (i = 0; i < isize; i++) {

        /* make sure this input isn't floating... */
        if (mif_private->conn[0]->is_null == FALSE) {

            /* use inputs to find index into lookup table */
            if ((in[i] = ((Digital_t*)(mif_private->conn[0]->port[i]->input.pvalue))->state) == UNKNOWN) {
                ivalid = 0;
                break;
            } else if (in[i] == ONE) {
                idx += j;
            }
            j <<= 1;
        } else {
            /* at least one port is floating...output is unknown */
            ivalid = 0;
            break;
        }
    }

    if (ivalid)
        for (k = 0; k < osize; k++)
            out[k] = lookup_table[idx + (k * entrylen)];

    /*** Determine analysis type and output appropriate values ***/

    if (mif_private->circuit.anal_type == DC) {   /** DC analysis...output w/o delays **/

        for (i = 0; i < osize; i++) {
            ((Digital_t*)(mif_private->conn[1]->port[i]->output.pvalue))->state = out[i].state;
            ((Digital_t*)(mif_private->conn[1]->port[i]->output.pvalue))->strength = out[i].strength;
        }
    }
    else {      /** Transient Analysis **/

        /* Determine maximum input-to-output delay */
        maxdelay = 0.0;
        for (i = 0; i < isize; i++)
            if (in[i] != in_old[i]) {
                if (i < dsize)
                    testdelay = mif_private->param[3]->element[i].rvalue;
                else if (dsize > 0)
                    testdelay =   mif_private->param[3]->element[dsize-1].rvalue;
                else
                    testdelay = 0.0;
                if (maxdelay < testdelay)
                    maxdelay = testdelay;
            }

        for (i = 0; i < osize; i++) {
            if (out[i].state != out_old[i].state) { /* output value is changing */

                mif_private->conn[1]->port[i]->delay = maxdelay;
                switch (out[i].state) {

                    /* fall to zero value */
                case ZERO:
                    ((Digital_t*)(mif_private->conn[1]->port[i]->output.pvalue))->state = ZERO;
                    if (i < fsize)
                        mif_private->conn[1]->port[i]->delay += mif_private->param[1]->element[i].rvalue;
                    else if (fsize > 0)
                        mif_private->conn[1]->port[i]->delay +=   mif_private->param[1]->element[fsize-1].rvalue;
                    else
                        mif_private->conn[1]->port[i]->delay += 1.0e-9;
                    ((Digital_t*)(mif_private->conn[1]->port[i]->output.pvalue))->strength = out[i].strength;
                    break;

                    /* rise to one value */
                case ONE:
                    ((Digital_t*)(mif_private->conn[1]->port[i]->output.pvalue))->state = ONE;
                    if (i < rsize)
                        mif_private->conn[1]->port[i]->delay += mif_private->param[0]->element[i].rvalue;
                    else if (rsize > 0)
                        mif_private->conn[1]->port[i]->delay +=   mif_private->param[0]->element[rsize-1].rvalue;
                    else
                        mif_private->conn[1]->port[i]->delay += 1.0e-9;
                    ((Digital_t*)(mif_private->conn[1]->port[i]->output.pvalue))->strength = out[i].strength;
                    break;

                    /* unknown output */
                default:
                    ((Digital_t*)(mif_private->conn[1]->port[i]->output.pvalue))->state = out[i].state = UNKNOWN;
                    ((Digital_t*)(mif_private->conn[1]->port[i]->output.pvalue))->strength = out[i].strength;

                    /* based on old value, add rise or fall delay */
                    if (out_old[i].state == 0) {  /* add rising delay */
                        if (i < rsize)
                            mif_private->conn[1]->port[i]->delay += mif_private->param[0]->element[i].rvalue;
                        else if (rsize > 0)
                            mif_private->conn[1]->port[i]->delay +=   mif_private->param[0]->element[rsize-1].rvalue;
                        else
                            mif_private->conn[1]->port[i]->delay += 1.0e-9;
                    } else {              /* add falling delay */
                        if (i < fsize)
                            mif_private->conn[1]->port[i]->delay += mif_private->param[1]->element[i].rvalue;
                        else if (fsize > 0)
                            mif_private->conn[1]->port[i]->delay +=   mif_private->param[1]->element[fsize-1].rvalue;
                        else
                            mif_private->conn[1]->port[i]->delay += 1.0e-9;
                    }
                    break;
                }
            } else if (out[i].strength != out_old[i].strength) {
                /* output strength is changing */
                ((Digital_t*)(mif_private->conn[1]->port[i]->output.pvalue))->strength = out[i].strength;
                switch (out[i].strength) {
                case STRONG:
                    if (out_old[i].state == 0) {        /* add falling delay */
                        if (i < fsize)
                            mif_private->conn[1]->port[i]->delay += mif_private->param[1]->element[i].rvalue;
                        else if (fsize > 0)
                            mif_private->conn[1]->port[i]->delay +=   mif_private->param[1]->element[fsize-1].rvalue;
                        else
                            mif_private->conn[1]->port[i]->delay += 1.0e-9;
                    } else {                    /* add rising delay */
                        if (i < rsize)
                            mif_private->conn[1]->port[i]->delay += mif_private->param[0]->element[i].rvalue;
                        else if (rsize > 0)
                            mif_private->conn[1]->port[i]->delay +=   mif_private->param[0]->element[rsize-1].rvalue;
                        else
                            mif_private->conn[1]->port[i]->delay += 1.0e-9;
                    }
                    break;
                default:
                    if (out_old[i].state == 0) {        /* add rising delay */
                        if (i < rsize)
                            mif_private->conn[1]->port[i]->delay += mif_private->param[0]->element[i].rvalue;
                        else if (rsize > 0)
                            mif_private->conn[1]->port[i]->delay +=   mif_private->param[0]->element[rsize-1].rvalue;
                        else
                            mif_private->conn[1]->port[i]->delay += 1.0e-9;
                    } else {                    /* add falling delay */
                        if (i < fsize)
                            mif_private->conn[1]->port[i]->delay += mif_private->param[1]->element[i].rvalue;
                        else if (fsize > 0)
                            mif_private->conn[1]->port[i]->delay +=   mif_private->param[1]->element[fsize-1].rvalue;
                        else
                            mif_private->conn[1]->port[i]->delay += 1.0e-9;
                    }
                    break;
                }
            } else {                  /* output value not changing */
                mif_private->conn[1]->port[i]->changed = FALSE;
            }
        }
    }
}
