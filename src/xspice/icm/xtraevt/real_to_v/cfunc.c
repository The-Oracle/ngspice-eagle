#line 1 "./xtraevt/real_to_v/cfunc.mod"
#include "ngspice/cm.h"
extern void ucm_real_to_v(Mif_Private_t *);
#line 1 "./xtraevt/real_to_v/cfunc.mod"


#define TS 0
#define VS 1


void ucm_real_to_v (Mif_Private_t *mif_private)
{

    double *t, *v;
    double *in;

    /*double out;*/


    in = (double *) mif_private->conn[0]->port[0]->input.pvalue;

    if(mif_private->circuit.init) {
        cm_event_alloc(TS, 2 * sizeof(double));
        cm_event_alloc(VS, 2 * sizeof(double));
        t = (double *) cm_event_get_ptr(TS, 0);
        v = (double *) cm_event_get_ptr(VS, 0);
        t[0] = -2.0;
        t[1] = -1.0;
        v[0] = *in;
        v[1] = *in;
    }
    else {
        t = (double *) cm_event_get_ptr(TS, 0);
        v = (double *) cm_event_get_ptr(VS, 0);
    }

    switch(mif_private->circuit.call_type) {

    case ANALOG:
        if(mif_private->circuit.time == 0.0) {
            mif_private->conn[1]->port[0]->output.rvalue = *in;
            v[0] = *in;
            v[1] = *in;
        }
        else {
            if(mif_private->circuit.time <= t[0])
                mif_private->conn[1]->port[0]->output.rvalue = v[0];
            else if(mif_private->circuit.time >= t[1])
                mif_private->conn[1]->port[0]->output.rvalue = v[1];
            else {
                mif_private->conn[1]->port[0]->output.rvalue = v[0] + (v[1] - v[0]) *
                                (mif_private->circuit.time - t[0]) / (t[1] - t[0]);
            }
        }
        break;

    case EVENT:
        if(mif_private->circuit.time == 0.0)
            return;
        if(mif_private->circuit.time >= t[1]) {
            v[0] = v[1];
            v[1] = *in;
            t[0] = mif_private->circuit.time;
            t[1] = mif_private->circuit.time + mif_private->param[1]->element[0].rvalue;
        }
        else {
            v[0] = v[0] + (v[1] - v[0]) *
                                (mif_private->circuit.time - t[0]) / (t[1] - t[0]);
            v[1] = *in;
            t[0] = mif_private->circuit.time;
            t[1] = mif_private->circuit.time + mif_private->param[1]->element[0].rvalue;
        }
        break;

    }
}




