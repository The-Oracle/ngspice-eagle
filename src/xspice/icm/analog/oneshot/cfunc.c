#line 1 "./analog/oneshot/cfunc.mod"
#include "ngspice/cm.h"
extern void cm_oneshot(Mif_Private_t *);
#line 1 "./analog/oneshot/cfunc.mod"
/*.......1.........2.........3.........4.........5.........6.........7.........8
================================================================================

FILE oneshot/cfunc.mod

Copyright 1991
Georgia Tech Research Corporation, Atlanta, Ga. 30332
All Rights Reserved

PROJECT A-8503-405


AUTHORS

    20 Mar 1991     Harry Li


MODIFICATIONS

    17 Sep 1991    Jeffrey P. Murray
     2 Oct 1991    Jeffrey P. Murray
     9 Sep 2012    Holger Vogt

SUMMARY

    This file contains the model-specific routines used to
    functionally describe the oneshot code model.


INTERFACES

    FILE                 ROUTINE CALLED

    CMmacros.h           cm_message_send();

    CM.c                 void *cm_analog_alloc()
                         void *cm_analog_get_ptr()
                         int cm_analog_set_temp_bkpt()

REFERENCED FILES

    Inputs from and outputs to ARGS structure.


NON-STANDARD FEATURES

    NONE

===============================================================================*/

/*=== INCLUDE FILES ====================*/

#include "oneshot.h"



/*=== CONSTANTS ========================*/




/*=== MACROS ===========================*/




/*=== LOCAL VARIABLES & TYPEDEFS =======*/


typedef struct {

    double   *control;   /* the storage array for the
                            control vector (cntl_array)   */

    double   *pw;   /* the storage array for the
                         pulse width array (pw_array)   */

    int tran_init; /* for initialization of old_clock) */

} Local_Data_t;



/*=== FUNCTION PROTOTYPE DEFINITIONS ===*/





/*==============================================================================

FUNCTION void cm_oneshot()

AUTHORS

    20 Mar 1991     Harry Li


MODIFICATIONS

    17 Sep 1991    Jeffrey P. Murray
     2 Oct 1991    Jeffrey P. Murray
     9 Sep 2012    Holger Vogt

SUMMARY

    This function implements the oneshot code model.

INTERFACES

    FILE                 ROUTINE CALLED

    CMmacros.h           cm_message_send();

    CM.c                 void *cm_analog_alloc()
                         void *cm_analog_get_ptr()
                         int cm_analog_set_temp_bkpt()

RETURNED VALUE

    Returns inputs and outputs via ARGS structure.

GLOBAL VARIABLES

    NONE

NON-STANDARD FEATURES

    NONE

==============================================================================*/

/*=== CM_ONESHOT ROUTINE ===*/

/***************************************************************************************
*
*  This model describes a totally analog oneshot.
*  After a rising edge is detected, the model will
*  output a pulse width specified by the controling
*  voltage.
*                          HWL 20Mar91
*
*
*
*                              ___________________________________
*                             /<---pulse width --->              :\
*                            / :                  :              : \
*         <---rise_delay--> /  :                  :<-fall_delay->:  \
*      ___|________________/   :                  :              :   \____________
*         ^                <-->:                  :              :<-->
*         Trigger         Risetime                               Falltime
*
*
****************************************************************************************/

#include <stdlib.h>

void cm_oneshot(Mif_Private_t *mif_private)  /* structure holding parms,
                                   inputs, outputs, etc.     */
{
    int i;             /* generic loop counter index                    */
    int *locked;        /* pointer used to store the locked1 variable */
    int locked1;        /* flag which allows the time points to be
                          reset.  value determined by retrig parameter  */
    int cntl_size;     /* size of the control array                     */
    int pw_size;       /* size of the pulse-width array                 */
    int *state;        /* pointer used to store state1 variable         */
    int state1;        /* if state1 = 1, then oneshot has
                          been triggered.  if state1 = 0, no change     */
    int *set;          /* pointer used to store the state of set1       */
    int set1;          /* flag used to set/reset the oneshot            */
    int trig_pos_edge; /* flag used to define positive or negative
                          edge triggering.  1=positive, 0=negative      */

    double *x;         /* pointer used to store the control array       */
    double *y;         /* pointer used to store the pulse-width array   */
    double cntl_input; /* the actual value of the control input         */
    /*double out;*/        /* value of the output                           */
    double dout_din;   /* slope of the pw wrt the control voltage       */
    double output_low; /* output low value                              */
    double output_hi;  /* output high value                             */
    double pw=0.0;         /* actual value of the pulse-width               */
    /*    double del_out;     value of the delay time between triggering
                              and a change in the output                    */
    double del_rise;    /* value of the delay time between triggering
                          and a change in the output                    */
    double del_fall;    /* value of the delay time between the end of the
                           pw and a change in the output                */
    double *t1;        /* pointer used to store time1                   */
    double *t2;        /* pointer used to store time2                   */
    double *t3;        /* pointer used to store time3                   */
    double *t4;        /* pointer used to store time4                   */
    double time1;      /* time at which the output first begins to
                          change (trigger + delay)                      */
    double time2;      /* time2 = time1 + risetime                      */
    double time3;      /* time3 = time2 + pw                            */
    double time4;      /* time4 = time3 + falltime                      */
    double t_rise;     /* risetime                                      */
    double t_fall;     /* falltime                                      */
    double *output_old;/* pointer which stores the previous output      */
    double *clock;     /* pointer which stores the clock                */
    double *old_clock; /* pointer which stores the previous clock       */
    double trig_clk;   /* value at which the clock triggers the oneshot */

    Mif_Complex_t ac_gain;

    Local_Data_t *loc;        /* Pointer to local static data, not to be included
                                       in the state vector */

    /**** Retrieve frequently used parameters... ****/

    cntl_size = mif_private->param[0]->size;
    pw_size = mif_private->param[1]->size;
    trig_clk = mif_private->param[2]->element[0].rvalue;
    trig_pos_edge = mif_private->param[3]->element[0].bvalue;
    output_low = mif_private->param[4]->element[0].rvalue;
    output_hi = mif_private->param[5]->element[0].rvalue;
    /*del_out = PARAM(delay);*/
    del_rise = mif_private->param[7]->element[0].rvalue;
    del_fall = mif_private->param[8]->element[0].rvalue;
    t_rise = mif_private->param[6]->element[0].rvalue;
    t_fall = mif_private->param[9]->element[0].rvalue;

    /* set minimum rise and fall_times */

    if(t_rise < 1e-12) {
        t_rise = 1e-12;
    }

    if(t_fall < 1e-12) {
        t_fall = 1e-12;
    }

    /* the control array must be the same size as the pulse-width array */

    if(cntl_size != pw_size) {
        cm_message_send(oneshot_array_error);
        return;
    }

    if(mif_private->circuit.init == 1) { /* first time through, allocate memory */

        cm_analog_alloc(T1,sizeof(double));
        cm_analog_alloc(T2,sizeof(double));
        cm_analog_alloc(T3,sizeof(double));
        cm_analog_alloc(T4,sizeof(double));
        cm_analog_alloc(SET,sizeof(int));
        cm_analog_alloc(STATE,sizeof(int));
        cm_analog_alloc(CLOCK,sizeof(double));
        cm_analog_alloc(LOCKED,sizeof(int));
        cm_analog_alloc(OUTPUT_OLD,sizeof(double));

        /*** allocate static storage for *loc ***/
         mif_private->inst_var[0]->element[0].pvalue = calloc (1 , sizeof ( Local_Data_t ));
        loc =  mif_private->inst_var[0]->element[0].pvalue;

        /* Allocate storage for breakpoint domain & pulse width values */
        x = loc->control = (double *) calloc((size_t) cntl_size, sizeof(double));
        if (!x) {
            cm_message_send(oneshot_allocation_error);
            return;
        }
        y = loc->pw = (double *) calloc((size_t) pw_size, sizeof(double));
        if (!y) {
            cm_message_send(oneshot_allocation_error);
            return;
        }

        loc->tran_init = FALSE;

    }

    if(mif_private->circuit.anal_type == MIF_DC) {

        /* for DC, initialize values and set the output = output_low */

        t1 = (double *) cm_analog_get_ptr(T1,0);
        t2 = (double *) cm_analog_get_ptr(T2,0);
        t3 = (double *) cm_analog_get_ptr(T3,0);
        t4 = (double *) cm_analog_get_ptr(T4,0);
        set = (int *) cm_analog_get_ptr(SET,0);
        state = (int *) cm_analog_get_ptr(STATE,0);
        locked = (int *) cm_analog_get_ptr(LOCKED,0);
        output_old = (double *) cm_analog_get_ptr(OUTPUT_OLD,0);

        /* initialize time and state values */
        *t1 = -1;
        *t2 = -1;
        *t3 = -1;
        *t4 = -1;
        *set = 0;
        *locked = 0;
        *state = 0;
        *output_old = output_low;

        mif_private->conn[3]->port[0]->output.rvalue = output_low;
        if(mif_private->conn[1]->is_null != 1) {
            mif_private->conn[3]->port[0]->partial[1].port[0] = 0;
        }
        if(mif_private->conn[2]->is_null != 1) {
            mif_private->conn[3]->port[0]->partial[2].port[0] = 0;
        }
        mif_private->conn[3]->port[0]->partial[0].port[0] = 0;

    } else if(mif_private->circuit.anal_type == MIF_TRAN) {

        /* retrieve previous values, set them equal to the variables
           Note that these pointer values are immediately dumped into
           other variables because the previous values can't change-
           can't rewrite the old values */

        t1 = (double *) cm_analog_get_ptr(T1,1);
        t2 = (double *) cm_analog_get_ptr(T2,1);
        t3 = (double *) cm_analog_get_ptr(T3,1);
        t4 = (double *) cm_analog_get_ptr(T4,1);
        set = (int*) cm_analog_get_ptr(SET,1);
        state = (int *) cm_analog_get_ptr(STATE,1);
        locked = (int *) cm_analog_get_ptr(LOCKED,1);
        clock = (double *) cm_analog_get_ptr(CLOCK,0);
        old_clock = (double *) cm_analog_get_ptr(CLOCK,1);
        output_old = (double *) cm_analog_get_ptr(OUTPUT_OLD,1);

        time1 = *t1;
        time2 = *t2;
        time3 = *t3;
        time4 = *t4;
        set1 = *set;
        state1 = *state;
        locked1 = *locked;

        if((mif_private->conn[2]->is_null != 1) && (mif_private->conn[2]->port[0]->input.rvalue > trig_clk)) {
            time1 = -1;
            time2 = -1;
            time3 = -1;
            time4 = -1;
            set1 = 0;
            locked1 = 0;
            state1 = 0;

            mif_private->conn[3]->port[0]->output.rvalue = output_low;
        } else {
            loc =  mif_private->inst_var[0]->element[0].pvalue;
            x = loc->control;
            y = loc->pw;

            if (!loc->tran_init) {
                *old_clock = 0.0;
                loc->tran_init = TRUE;
            }

            /* Retrieve control and pulse-width values. */
            for (i=0; i<cntl_size; i++) {
                x[i] = mif_private->param[0]->element[i].rvalue;
                y[i] = mif_private->param[1]->element[i].rvalue;
            }

            /* Retrieve cntl_input and clock value. */
            if(mif_private->conn[1]->is_null != 1) {
                cntl_input = mif_private->conn[1]->port[0]->input.rvalue;
            } else {
                cntl_input = 0;
            }

            *clock = mif_private->conn[0]->port[0]->input.rvalue;

            /* Determine segment boundaries within which cntl_input resides */
            if (cntl_input <= *x) { /* cntl_input below lowest cntl_voltage */
                dout_din = (y[1] - y[0])/(x[1] - x[0]);
                pw = *y + (cntl_input - *x) * dout_din;

                if(pw < 0) {
                    cm_message_send(oneshot_pw_clamp);
                    pw = 0;
                }
            } else

                /*** cntl_input above highest cntl_voltage ***/
                if (cntl_input >= x[cntl_size-1]) {
                    dout_din = (y[cntl_size-1] - y[cntl_size-2]) /
                               (x[cntl_size-1] - x[cntl_size-2]);
                    pw = y[cntl_size-1] + (cntl_input - x[cntl_size-1]) * dout_din;

                } else {
                    /*** cntl_input within bounds of end midpoints...
                    must determine position progressively & then
                    calculate required output.                    ***/

                    for (i=0; i<cntl_size-1; i++) {
                        if ((cntl_input < x[i+1]) && (cntl_input >= x[i])) {
                            /* Interpolate to get the correct pulse width value */
                            pw = ((cntl_input - x[i])/(x[i+1] - x[i]))*
                                 (y[i+1]-y[i]) + y[i];
                        }
                    }
                }

            if(trig_pos_edge) { /* for a positive edge trigger */

                if(!set1) {
                    /* if set1=0, then look for
                         1.  a rising edge trigger
                         2.  the clock to be higher than the trigger value */

                    if((*clock > *old_clock) && (*clock > trig_clk)) {
                        state1 = 1;
                        set1 = 1;
                    }

                } else
                    /* look for a neg edge before resetting the trigger */
                    if((*clock < *old_clock) && (*clock < trig_clk)) {
                        set1 = 0;
                    }
            } else {
                /* This stuff belongs to the case where a negative edge
                is needed */

                if(!set1) {
                    if((*clock < *old_clock) && (*clock < trig_clk)) {
                        state1 = 1;
                        set1 = 1;
                    }
                } else
                    /* look for a pos edge before resetting the trigger */
                    if((*clock > *old_clock) && (*clock > trig_clk)) {
                        set1 = 0;
                    }
            }


            /*  I can only set the breakpoints if the state1 is high and
                the output is low, and locked = 0 */
            if((state1) && (*output_old - output_low < 1e-20) && (!locked1)) {

                /* if state1 is 1, and the output is low, then set the time points
                   and the temporary breakpoints */

                time1 = mif_private->circuit.time + del_rise;
                time2 = time1 + t_rise;
                time3 = time2 + pw + del_fall;
                time4 = time3 + t_fall;

                if(mif_private->param[10]->element[0].bvalue == MIF_FALSE) {
                    locked1 = 1;
                }

                if((mif_private->circuit.time < time1) || (mif_private->circuit.t[1] == 0)) {
                    cm_analog_set_perm_bkpt(time1);
                }

                cm_analog_set_perm_bkpt(time2);
                cm_analog_set_perm_bkpt(time3);
                cm_analog_set_perm_bkpt(time4);

                /* reset the state value */
                state1 = 0;
                mif_private->conn[3]->port[0]->output.rvalue = output_low;

            } else

                /* state1 = 1, and the output is high,  then just set time3 and time4.
                Temporary breakpoints don't do for now, so use permanent breakpoints.
                This implies that the oneshot was retriggered */

                if((state1) && (*output_old - output_hi < 1e-20) && (!locked1)) {

                    time3 = mif_private->circuit.time + pw + del_rise + del_fall + t_rise;
                    time4 = time3 + t_fall;

                    cm_analog_set_perm_bkpt(time3);
                    cm_analog_set_perm_bkpt(time4);

                    mif_private->conn[3]->port[0]->output.rvalue = output_hi;

                    state1 = 0;
                }

            /* reset the state if it's 1 and the locked flag is 1.  This
               means that the clock tried to retrigger the oneshot, but
               the retrig flag prevented it from doing so */

            if((state1) && (locked1)) {
                state1 = 0;

            }
            /*  set the value for the output depending on the current time, and
                the values of time1, time2, time3, and time4 */
            if(mif_private->circuit.time < time1) {
                mif_private->conn[3]->port[0]->output.rvalue = output_low;
            } else if((time1 <= mif_private->circuit.time) && (mif_private->circuit.time < time2)) {
                mif_private->conn[3]->port[0]->output.rvalue = output_low + ((mif_private->circuit.time - time1)/(time2 - time1))*
                              (output_hi - output_low);
            } else if((time2 <= mif_private->circuit.time) && (mif_private->circuit.time < time3)) {

                mif_private->conn[3]->port[0]->output.rvalue = output_hi;

            } else if((time3 <= mif_private->circuit.time) && (mif_private->circuit.time < time4)) {

                mif_private->conn[3]->port[0]->output.rvalue = output_hi + ((mif_private->circuit.time - time3)/(time4 - time3))*
                              (output_low - output_hi);

            } else {
                mif_private->conn[3]->port[0]->output.rvalue = output_low;

                /* oneshot can now be retriggered, set locked to 0 */
                if(mif_private->param[10]->element[0].bvalue == MIF_FALSE) {
                    locked1 = 0;
                }
            }
        }
        /* set the variables which need to be stored for the next iteration */

        t1 = (double *) cm_analog_get_ptr(T1,0);
        t2 = (double *) cm_analog_get_ptr(T2,0);
        t3 = (double *) cm_analog_get_ptr(T3,0);
        t4 = (double *) cm_analog_get_ptr(T4,0);
        set = (int *) cm_analog_get_ptr(SET,0);
        locked = (int *) cm_analog_get_ptr(LOCKED,0);
        state = (int *) cm_analog_get_ptr(STATE,0);
        output_old = (double *) cm_analog_get_ptr(OUTPUT_OLD,0);

        *t1 = time1;
        *t2 = time2;
        *t3 = time3;
        *t4 = time4;
        *set = set1;
        *state = state1;
        *output_old = mif_private->conn[3]->port[0]->output.rvalue;
        *locked = locked1;

        if(mif_private->conn[1]->is_null != 1) {
            mif_private->conn[3]->port[0]->partial[1].port[0] = 0;
        }
        if(mif_private->conn[2]->is_null != 1) {
            mif_private->conn[3]->port[0]->partial[2].port[0] = 0;
        }
        mif_private->conn[3]->port[0]->partial[0].port[0] = 0 ;

    } else {                      /* Output AC Gain */

        /* This model has no AC capability */

        ac_gain.real = 0.0;
        ac_gain.imag= 0.0;
        mif_private->conn[3]->port[0]->ac_gain[0].port[0] = ac_gain;
    }
}

