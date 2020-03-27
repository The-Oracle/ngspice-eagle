#line 1 "./xtraevt/d_to_real/cfunc.mod"
#include "ngspice/cm.h"
extern void ucm_d_to_real(Mif_Private_t *);
#line 1 "./xtraevt/d_to_real/cfunc.mod"

void ucm_d_to_real (Mif_Private_t *mif_private)
{

    Digital_State_t     in;

    double              *out;
    double              delay;
    double              zero;
    double              one;
    double              ena;


    in = ((Digital_t*)(mif_private->conn[0]->port[0]->input.pvalue))->state;
    if(mif_private->conn[1]->is_null)
        ena = 1.0;
    else if(((Digital_t*)(mif_private->conn[1]->port[0]->input.pvalue))->state == ONE)
        ena = 1.0;
    else
        ena = 0.0;
    out = (double *) mif_private->conn[2]->port[0]->output.pvalue;

    zero  = mif_private->param[0]->element[0].rvalue;
    one   = mif_private->param[1]->element[0].rvalue;
    delay = mif_private->param[2]->element[0].rvalue;


    if(in == ZERO)
        *out = zero * ena;
    else if(in == UNKNOWN)
        *out = (zero + one) / 2.0 * ena;
    else
        *out = one * ena;

    if(mif_private->circuit.time > 0.0)
        mif_private->conn[2]->port[0]->delay = delay;

}




