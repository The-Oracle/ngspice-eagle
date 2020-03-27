#line 1 "./analog/multi_input_pwl/cfunc.mod"
#include "ngspice/cm.h"
extern void cm_multi_input_pwl(Mif_Private_t *);
#line 1 "./analog/multi_input_pwl/cfunc.mod"
/*.......1.........2.........3.........4.........5.........6.........7.........8
================================================================================

FILE multi_input_pwl/cfunc.mod

Copyright 2005
Intrinsity, Inc. Austin, TX 78738
All Rights Reserved

AUTHORS                      

     20 Oct 2005     Phil Barker


MODIFICATIONS   

     20 Oct 2005     Phil Barker
                                   

SUMMARY

    This file contains the model-specific routines used to
    functionally describe the multi-input gate pwl.


INTERFACES       

    FILE                 ROUTINE CALLED     

    N/A                  N/A


REFERENCED FILES

    Inputs from and outputs to ARGS structure.
                     

NON-STANDARD FEATURES

    NONE

===============================================================================*/

/*=== INCLUDE FILES ====================*/


                                      

/*=== CONSTANTS ========================*/




/*=== MACROS ===========================*/



  
/*=== LOCAL VARIABLES & TYPEDEFS =======*/                         


    
           
/*=== FUNCTION PROTOTYPE DEFINITIONS ===*/




                   
/*==============================================================================

FUNCTION void cm_multi_input_pwl()

AUTHORS                      

     20 Oct 2005     Phil Barker

MODIFICATIONS   

     20 Oct 2005     Phil Barker

SUMMARY

    This function implements the multi-input gate pwl code model.

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

/*=== CM_MULTI_INPUT_PWL ROUTINE ===*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static double
get_smallest_input( Mif_Private_t *mif_private )
{
  double smallest = mif_private->conn[0]->port[0]->input.rvalue;
  int    size     = mif_private->conn[0]->size;
  int    i;

  for ( i = 1; i < size; i++ )
    if ( mif_private->conn[0]->port[i]->input.rvalue < smallest ) smallest = mif_private->conn[0]->port[i]->input.rvalue;

  return smallest;
}

static double
get_largest_input( Mif_Private_t *mif_private )
{
  double largest = mif_private->conn[0]->port[0]->input.rvalue;
  int    size    = mif_private->conn[0]->size;
  int    i;

  for ( i = 1; i < size; i++ )
    if ( mif_private->conn[0]->port[i]->input.rvalue > largest ) largest = mif_private->conn[0]->port[i]->input.rvalue;

  return largest;
}

static double
get_slope( Mif_Private_t *mif_private, int i )
{
  return ( mif_private->param[1]->element[i].rvalue - mif_private->param[1]->element[i-1].rvalue )/( mif_private->param[0]->element[i].rvalue - mif_private->param[0]->element[i-1].rvalue );
}

static double
y_intercept( Mif_Private_t *mif_private, int i, double slope )
{
  return ( mif_private->param[1]->element[i].rvalue - slope*mif_private->param[0]->element[i].rvalue );
}

static double
get_output( Mif_Private_t *mif_private, double x )
{
  int    size   = mif_private->param[0]->size;
  double result = 0;
  double slope  = 0;
  int    i;

  /* check if x beyond specified limits */
  if ( x <= mif_private->param[0]->element[0].rvalue      ) return mif_private->param[1]->element[0].rvalue;
  if ( x >= mif_private->param[0]->element[size-1].rvalue ) return mif_private->param[1]->element[size-1].rvalue;
       
  for ( i = 1; i < size; i++ )
    if ( x > mif_private->param[0]->element[i-1].rvalue && x <= mif_private->param[0]->element[i].rvalue )
      {
	slope  = get_slope( mif_private, i );
	result = slope*x + y_intercept( mif_private, i, slope );
	break;
      }
  return result;
}

void
cm_multi_input_pwl(Mif_Private_t *mif_private) 
{
  const char*  model = ( mif_private->param[2]->is_null == 1 ) ? "and" : mif_private->param[2]->element[0].svalue;
  double output;

  if ( mif_private->circuit.anal_type == TRANSIENT || mif_private->circuit.anal_type == DC )
    {
      if ( strcmp( model, "and"  ) != 0 && strcmp( model, "or"  ) != 0 &&
	   strcmp( model, "nand" ) != 0 && strcmp( model, "nor" ) != 0 )
	{
	  fprintf( stderr, "ERROR(cm_multi_input_pwl): unknown gate model type '%s'; expecting 'and|or|nand|nor'.\n", model );
	  exit(-1);
	}
      if ( mif_private->param[0]->size != mif_private->param[1]->size )
	{
	  fprintf( stderr, "ERROR(cm_multi_input_pwl): 'x' and 'y' input vectors are not the same size!\n" );
	  exit(-1);
	}
      /*
	Iterate through each input and find output value
	  and/nand: controlling input is chosen on the basis of the smallest value
	  or/nor:   controlling input is chosen on the basis of the largest value
      */
      if (strstr(model, "and")) output = get_output(mif_private, get_smallest_input(mif_private));
      else                      output = get_output(mif_private, get_largest_input(mif_private));

      mif_private->conn[1]->port[0]->output.rvalue = output;
    }
}
