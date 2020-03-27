#line 1 "./xtraevt/real_delay/cfunc.mod"
#include "ngspice/cm.h"
extern void ucm_real_delay(Mif_Private_t *);
#line 1 "./xtraevt/real_delay/cfunc.mod"


#define CLK_STATE       0


void ucm_real_delay (Mif_Private_t *mif_private)
{

    double              *in;
    double              *out;

    Digital_State_t     *state;
    Digital_State_t     *old_state;


    if(mif_private->circuit.init) {
        cm_event_alloc(CLK_STATE, sizeof(Digital_State_t));
        state = (Digital_State_t *) cm_event_get_ptr(CLK_STATE, 0);
        old_state = state;
        *state = ((Digital_t*)(mif_private->conn[1]->port[0]->input.pvalue))->state;
    }
    else {
        state = (Digital_State_t *) cm_event_get_ptr(CLK_STATE, 0);
        old_state = (Digital_State_t *) cm_event_get_ptr(CLK_STATE, 1);
    }

    if(mif_private->circuit.anal_type != TRANSIENT)
        mif_private->conn[2]->port[0]->changed = FALSE;
    else {
        *state = ((Digital_t*)(mif_private->conn[1]->port[0]->input.pvalue))->state;
        if(*state == *old_state)
            mif_private->conn[2]->port[0]->changed = FALSE;
        else if(*state != ONE)
            mif_private->conn[2]->port[0]->changed = FALSE;
        else {
            in = (double *) mif_private->conn[0]->port[0]->input.pvalue;
            out = (double *) mif_private->conn[2]->port[0]->output.pvalue;
            *out = *in;
            mif_private->conn[2]->port[0]->delay = mif_private->param[0]->element[0].rvalue;
        }
    }
}




