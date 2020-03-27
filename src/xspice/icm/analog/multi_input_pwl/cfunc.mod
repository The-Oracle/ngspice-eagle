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
get_smallest_input( ARGS )
{
  double smallest = INPUT(in[0]);
  int    size     = PORT_SIZE(in);
  int    i;

  for ( i = 1; i < size; i++ )
    if ( INPUT(in[i]) < smallest ) smallest = INPUT(in[i]);

  return smallest;
}

static double
get_largest_input( ARGS )
{
  double largest = INPUT(in[0]);
  int    size    = PORT_SIZE(in);
  int    i;

  for ( i = 1; i < size; i++ )
    if ( INPUT(in[i]) > largest ) largest = INPUT(in[i]);

  return largest;
}

static double
get_slope( ARGS, int i )
{
  return ( PARAM(y[i]) - PARAM(y[i-1]) )/( PARAM(x[i]) - PARAM(x[i-1]) );
}

static double
y_intercept( ARGS, int i, double slope )
{
  return ( PARAM(y[i]) - slope*PARAM(x[i]) );
}

static double
get_output( ARGS, double x )
{
  int    size   = PARAM_SIZE(x);
  double result = 0;
  double slope  = 0;
  int    i;

  /* check if x beyond specified limits */
  if ( x <= PARAM(x[0])      ) return PARAM(y[0]);
  if ( x >= PARAM(x[size-1]) ) return PARAM(y[size-1]);
       
  for ( i = 1; i < size; i++ )
    if ( x > PARAM(x[i-1]) && x <= PARAM(x[i]) )
      {
	slope  = get_slope( mif_private, i );
	result = slope*x + y_intercept( mif_private, i, slope );
	break;
      }
  return result;
}

void
cm_multi_input_pwl(ARGS) 
{
  const char*  model = ( PARAM_NULL(model) == 1 ) ? "and" : PARAM(model);
  double output;

  if ( ANALYSIS == TRANSIENT || ANALYSIS == DC )
    {
      if ( strcmp( model, "and"  ) != 0 && strcmp( model, "or"  ) != 0 &&
	   strcmp( model, "nand" ) != 0 && strcmp( model, "nor" ) != 0 )
	{
	  fprintf( stderr, "ERROR(cm_multi_input_pwl): unknown gate model type '%s'; expecting 'and|or|nand|nor'.\n", model );
	  exit(-1);
	}
      if ( PARAM_SIZE(x) != PARAM_SIZE(y) )
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

      OUTPUT(out) = output;
    }
}
