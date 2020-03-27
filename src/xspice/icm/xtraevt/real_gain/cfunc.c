#line 1 "./xtraevt/real_gain/cfunc.mod"
#include "ngspice/cm.h"
extern void ucm_real_gain(Mif_Private_t *);
#line 1 "./xtraevt/real_gain/cfunc.mod"

void ucm_real_gain (Mif_Private_t *mif_private)
{
    double      *in;
    double      *out;

    double      in_offset;
    double      gain;
    double      out_offset;
    double      delay;
    double      ic;


    /* Get the input and output pointers */
    in = (double *) mif_private->conn[0]->port[0]->input.pvalue;
    out = (double *) mif_private->conn[1]->port[0]->output.pvalue;

    /* Get the parameters */
    in_offset  = mif_private->param[0]->element[0].rvalue;
    gain       = mif_private->param[1]->element[0].rvalue;
    out_offset = mif_private->param[2]->element[0].rvalue;
    delay      = mif_private->param[3]->element[0].rvalue;
    ic         = mif_private->param[4]->element[0].rvalue;


    /* Assign the output and delay */    
    if(mif_private->circuit.anal_type == DC) {
        *out = ic;
        if(mif_private->circuit.init)
            cm_event_queue(delay);
    }
    else {
        *out = gain * (*in + in_offset) + out_offset;
        mif_private->conn[1]->port[0]->delay = delay;
    }
}


