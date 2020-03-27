#line 1 "./analog/file_source/cfunc.mod"
#include "ngspice/cm.h"
extern void cm_filesource(Mif_Private_t *);
#line 1 "./analog/file_source/cfunc.mod"
/*.......1.........2.........3.........4.........5.........6.........7.........8
================================================================================

FILE filesource/cfunc.mod

Copyright 2011
Thomas Sailer

              

AUTHORS                      

    20 May 2011     Thomas Sailer
    03 Sep 2012     Holger Vogt
    27 Feb 2017     Marcel Hendrix
    23 JUL 2018     Holger Vogt


MODIFICATIONS


SUMMARY

    This file contains the model-specific routines used to
    functionally describe the file source code model used
    to read an array from a file containing lines with
    time and analog values, and returning them per time step.


INTERFACES       

    FILE                 ROUTINE CALLED     

    N/A                  N/A                     


REFERENCED FILES

    Inputs from and outputs to ARGS structure.
                     

NON-STANDARD FEATURES

    NONE

===============================================================================*/

/*=== INCLUDE FILES ====================*/

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/*=== CONSTANTS ========================*/




/*=== MACROS ===========================*/

#if defined(__MINGW32__) || defined(_MSC_VER)
#define DIR_PATHSEP    "\\"
#else
#define DIR_PATHSEP    "/"
#endif

#if defined(_MSC_VER)
#define strdup _strdup
#endif
  
/*=== LOCAL VARIABLES & TYPEDEFS =======*/                         

struct filesource_state {
    FILE *fp;
    unsigned char atend;
};


struct infiledata {
    double *datavec;
    int vecallocated;
    int maxoccupied;
    int actpointer;
    int size;
};

typedef struct {

    double   *amplinterval;   /* the storage array for the
                                   amplitude offsets   */

    double   *timeinterval;   /* the storage array for the
                                   time offset   */

    struct filesource_state  *state;   /* the storage array for the
                                          filesource status.    */

    struct infiledata *indata; /* the storage vector for the input data
                                  sourced from file. */

} Local_Data_t;

           
/*=== FUNCTION PROTOTYPE DEFINITIONS ===*/




                   
/*==============================================================================

FUNCTION void cm_filesource()

AUTHORS                      

    20 May 2011     Thomas Sailer

MODIFICATIONS   

    07 Sept 2012    Holger Vogt
    27 Feb  2017    Marcel Hendrix
    23 JUL  2018    Holger Vogt

SUMMARY

    This function implements the filesource code model.

INTERFACES       

    FILE                 ROUTINE CALLED     

    N/A                  N/A


RETURNED VALUE
    
    Returns inputs and outputs via ARGS structure.

GLOBAL VARIABLES
    
    NONE

NON-STANDARD FEATURES

    NONE

==============================================================================*/


/*=== CM_FILESOURCE ROUTINE ===*/
                                                   

static void
cm_filesource_callback(Mif_Private_t *mif_private, Mif_Callback_Reason_t reason)
{
    switch (reason) {
        case MIF_CB_DESTROY: {
            Local_Data_t *loc =  mif_private->inst_var[0]->element[0].pvalue;
            if (loc->state->fp)
                fclose(loc->state->fp);
            free(loc->state);
            free(loc->amplinterval);
            free(loc->timeinterval);
            free(loc->indata->datavec);
            free(loc->indata);
            free(loc);
            break;
        }
    }
}


void cm_filesource(Mif_Private_t *mif_private)   /* structure holding parms, inputs, outputs, etc.     */
{
    int size = mif_private->conn[0]->size;
    int stepsize = size + 1;
    int amplscalesize;
    int amploffssize;
    int j;

    Local_Data_t *loc;        /* Pointer to local static data, not to be included
                                       in the state vector */

    if(mif_private->circuit.anal_type == MIF_AC) {
        return;
    }
    if (mif_private->circuit.init == 1) {

        int count;

        *(mif_private->callback) = cm_filesource_callback;

        /*** allocate static storage for *loc ***/
         mif_private->inst_var[0]->element[0].pvalue = calloc (1 , sizeof ( Local_Data_t ));
        loc =  mif_private->inst_var[0]->element[0].pvalue;

        /* Allocate storage for internal state */
        loc->timeinterval = (double*)calloc(2, sizeof(double));
        loc->amplinterval = (double*)calloc(2 * (size_t) size, sizeof(double));
        loc->state = (struct filesource_state*)malloc(sizeof(struct filesource_state));
        loc->indata = (struct infiledata*)malloc(sizeof(struct infiledata));
        loc->indata->datavec = (double*)malloc(sizeof(double) * stepsize * 1000);
        loc->indata->vecallocated = stepsize * 1000;
        loc->indata->maxoccupied = 0;
        loc->indata->actpointer = 0;
        loc->indata->size = stepsize;

        /* open the file */
        loc->state->fp = fopen_with_path(mif_private->param[6]->element[0].svalue, "r");
        loc->state->atend = 0;
        if (!loc->state->fp) {
            char *lbuffer, *p;
            lbuffer = getenv("NGSPICE_INPUT_DIR");
            if (lbuffer && *lbuffer) {
                p = (char*) malloc(strlen(lbuffer) + strlen(DIR_PATHSEP) + strlen(mif_private->param[6]->element[0].svalue) + 1);
                sprintf(p, "%s%s%s", lbuffer, DIR_PATHSEP, mif_private->param[6]->element[0].svalue);
                loc->state->fp = fopen(p, "r");
                free(p);
            }
            if (!loc->state->fp) {
                cm_message_printf("cannot open file %s", mif_private->param[6]->element[0].svalue);
                loc->state->atend = 1;
            }
        }
        /* read, preprocess and store the data */
        amplscalesize = mif_private->param[5]->is_null ? 0 : mif_private->param[5]->size;
        amploffssize = mif_private->param[4]->is_null ? 0 : mif_private->param[4]->size;
        count = 0;
        while (!loc->state->atend) {
            char line[512];
            char *cp, *cpdel;
            char *cp2;
            double t, tprev = 0;
            int i;
            if (!fgets(line, sizeof(line), loc->state->fp)) {
                loc->state->atend = 1;
                break;
            }
            cpdel = cp = strdup(line);

            /* read the time channel; update the time difference */
            while (*cp && isspace_c(*cp))
                ++cp;
            if (*cp == '*' || *cp == '#' || *cp == ';') {
                free(cpdel);
                continue;
            }
            t = strtod(cp, &cp2);
            if (cp2 == cp) {
                free(cpdel);
                continue;
            }
            cp = cp2;
            if (!mif_private->param[1]->is_null)
                t *= mif_private->param[1]->element[0].rvalue;
            if (!mif_private->param[2]->is_null && mif_private->param[2]->element[0].bvalue == MIF_TRUE)
                t += tprev;
            else if (!mif_private->param[0]->is_null)
                t += mif_private->param[0]->element[0].rvalue;

            tprev = t;

            /* before storing, check if vector size is large enough.
               If not, add another 1000*size doubles */
            if (count > loc->indata->vecallocated - size) {
                loc->indata->vecallocated += size * 1000;
                loc->indata->datavec = (double*)realloc(loc->indata->datavec, sizeof(double) * loc->indata->vecallocated);
            }
            if(loc->indata->datavec == NULL){
                cm_message_printf("cannot allocate enough memory");
                break; // loc->state->atend = 1;
            }
            loc->indata->datavec[count++] = t;

            /* read the data channels; update the amplitude difference of each channel */
            for (i = 0; i < size; ++i) {
                while (*cp && (isspace_c(*cp) || *cp == ','))
                    ++cp;
                t = strtod(cp, &cp2);
                if (cp2 == cp)
                    break;
                cp = cp2;
                if (i < amplscalesize)
                    t *= mif_private->param[5]->element[i].rvalue;
                if (i < amploffssize)
                    t += mif_private->param[4]->element[i].rvalue;
                loc->indata->datavec[count++] = t;
            }
            free(cpdel);
        }
        loc->indata->maxoccupied = count;

        if(loc->state->fp) {
            fclose(loc->state->fp);
            loc->state->fp = NULL;
        }
        /* set the start time data */
        loc->timeinterval[0] = loc->indata->datavec[loc->indata->actpointer];
        loc->timeinterval[1] = loc->indata->datavec[loc->indata->actpointer + stepsize];
    }

    loc =  mif_private->inst_var[0]->element[0].pvalue;

    /* The file pointer is at the same position it was for the last simulator TIME ...
     * If TIME steps backward, for example due to a second invocation of a 'tran' analysis
     *   step back in datavec[loc->indata->actpointer] .
     */
    if (mif_private->circuit.time < loc->timeinterval[0]) {
        while (mif_private->circuit.time < loc->indata->datavec[loc->indata->actpointer] && loc->indata->actpointer >= 0)
            loc->indata->actpointer -= stepsize;
        loc->timeinterval[0] = loc->indata->datavec[loc->indata->actpointer];
        loc->timeinterval[1] = loc->indata->datavec[loc->indata->actpointer + stepsize];
    }

    while (mif_private->circuit.time > loc->timeinterval[1]) {
        loc->indata->actpointer += stepsize;
        if (loc->indata->actpointer > loc->indata->maxoccupied) {
            /* we are done */
            return;
        }
        loc->timeinterval[1] = loc->indata->datavec[loc->indata->actpointer + stepsize];
        loc->timeinterval[0] = loc->indata->datavec[loc->indata->actpointer];
    }

    for (j = 0; j < size; j++) {
        loc->amplinterval[2 * j] = loc->indata->datavec[loc->indata->actpointer + j + 1];
        loc->amplinterval[2 * j + 1] = loc->indata->datavec[loc->indata->actpointer + stepsize + j + 1];
    }

    if (loc->timeinterval[0] <= mif_private->circuit.time && mif_private->circuit.time <= loc->timeinterval[1]) {
        if (!mif_private->param[3]->is_null && mif_private->param[3]->element[0].bvalue == MIF_TRUE) {
            int i;
            for (i = 0; i < size; ++i)
                mif_private->conn[0]->port[i]->output.rvalue = loc->amplinterval[2 * i];
        } else {
            double mul0 = (loc->timeinterval[1] - mif_private->circuit.time) / (loc->timeinterval[1] - loc->timeinterval[0]);
            double mul1 = 1.0 - mul0;
            int i;
            for (i = 0; i < size; ++i)
                mif_private->conn[0]->port[i]->output.rvalue = mul0 * loc->amplinterval[2 * i] + mul1 * loc->amplinterval[2 * i + 1];
        }
    } else {
        int i;
        for (i = 0; i < size; ++i)
            mif_private->conn[0]->port[i]->output.rvalue = loc->amplinterval[2 * i + 1];
    }
}
