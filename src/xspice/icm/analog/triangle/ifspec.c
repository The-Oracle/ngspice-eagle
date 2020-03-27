
/*
 * Structures for model: triangle
 *
 * Automatically generated by cmpp preprocessor
 *
 * !!! DO NOT EDIT !!!
 *
 */


#include "ngspice/ngspice.h"
#include <stdio.h>
#include "ngspice/devdefs.h"
#include "ngspice/ifsim.h"
#include "ngspice/mifdefs.h"
#include "ngspice/mifproto.h"
#include "ngspice/mifparse.h"


static IFparm MIFmPTable[] = {
    IOP("cntl_array", 0, (IF_REAL|IF_VECTOR), "control in array"),
    IOP("freq_array", 1, (IF_REAL|IF_VECTOR), "frequency array"),
    IOP("out_low", 2, IF_REAL, "output low value"),
    IOP("out_high", 3, IF_REAL, "output high value"),
    IOP("duty_cycle", 4, IF_REAL, "rise time duty cycle"),
};


static IFparm MIFpTable[] = {
    OP("tran_init", 5, IF_FLAG, "tran initialisation"),
};


static Mif_Port_Type_t MIFportEnum0[] = {
	MIF_VOLTAGE,
	MIF_DIFF_VOLTAGE,
	MIF_CURRENT,
	MIF_DIFF_CURRENT,
	MIF_VSOURCE_CURRENT,
};


static char *MIFportStr0[] = {
	"v",
	"vd",
	"i",
	"id",
	"vnam",
};


static Mif_Port_Type_t MIFportEnum1[] = {
	MIF_VOLTAGE,
	MIF_DIFF_VOLTAGE,
	MIF_CURRENT,
	MIF_DIFF_CURRENT,
};


static char *MIFportStr1[] = {
	"v",
	"vd",
	"i",
	"id",
};


static Mif_Conn_Info_t MIFconnTable[] = {
  {
    "cntl_in",
    "input",
    MIF_IN,
    MIF_VOLTAGE,
    "v",
    5,
    MIFportEnum0,
    MIFportStr0,
    MIF_FALSE,
    MIF_FALSE,
    0,
    MIF_FALSE,
    0,
    MIF_FALSE,
  },
  {
    "out",
    "output",
    MIF_OUT,
    MIF_VOLTAGE,
    "v",
    4,
    MIFportEnum1,
    MIFportStr1,
    MIF_FALSE,
    MIF_FALSE,
    0,
    MIF_FALSE,
    0,
    MIF_FALSE,
  },
};


static Mif_Param_Info_t MIFparamTable[] = {
  {
    "cntl_array",
    "control in array",
    MIF_REAL,
    MIF_TRUE,
    {MIF_FALSE, 0, 0.000000e+00, {0.0, 0.0}, NULL},
    MIF_FALSE,
    {MIF_FALSE, 0, 0.0, {0.0, 0.0}, NULL},
    MIF_FALSE,
    {MIF_FALSE, 0, 0.0, {0.0, 0.0}, NULL},
    MIF_TRUE,
    MIF_FALSE,
    0,
    MIF_TRUE,
    2,
    MIF_FALSE,
    0,
    MIF_FALSE,
  },
  {
    "freq_array",
    "frequency array",
    MIF_REAL,
    MIF_TRUE,
    {MIF_FALSE, 0, 1.000000e+03, {0.0, 0.0}, NULL},
    MIF_TRUE,
    {MIF_FALSE, 0, 0.000000e+00, {0.0, 0.0}, NULL},
    MIF_FALSE,
    {MIF_FALSE, 0, 0.0, {0.0, 0.0}, NULL},
    MIF_TRUE,
    MIF_FALSE,
    0,
    MIF_TRUE,
    2,
    MIF_FALSE,
    0,
    MIF_FALSE,
  },
  {
    "out_low",
    "output low value",
    MIF_REAL,
    MIF_TRUE,
    {MIF_FALSE, 0, -1.000000e+00, {0.0, 0.0}, NULL},
    MIF_FALSE,
    {MIF_FALSE, 0, 0.0, {0.0, 0.0}, NULL},
    MIF_FALSE,
    {MIF_FALSE, 0, 0.0, {0.0, 0.0}, NULL},
    MIF_FALSE,
    MIF_FALSE,
    0,
    MIF_FALSE,
    0,
    MIF_FALSE,
    0,
    MIF_TRUE,
  },
  {
    "out_high",
    "output high value",
    MIF_REAL,
    MIF_TRUE,
    {MIF_FALSE, 0, 1.000000e+00, {0.0, 0.0}, NULL},
    MIF_FALSE,
    {MIF_FALSE, 0, 0.0, {0.0, 0.0}, NULL},
    MIF_FALSE,
    {MIF_FALSE, 0, 0.0, {0.0, 0.0}, NULL},
    MIF_FALSE,
    MIF_FALSE,
    0,
    MIF_FALSE,
    0,
    MIF_FALSE,
    0,
    MIF_TRUE,
  },
  {
    "duty_cycle",
    "rise time duty cycle",
    MIF_REAL,
    MIF_TRUE,
    {MIF_FALSE, 0, 5.000000e-01, {0.0, 0.0}, NULL},
    MIF_TRUE,
    {MIF_FALSE, 0, 1.000000e-06, {0.0, 0.0}, NULL},
    MIF_TRUE,
    {MIF_FALSE, 0, 9.999990e-01, {0.0, 0.0}, NULL},
    MIF_FALSE,
    MIF_FALSE,
    0,
    MIF_FALSE,
    0,
    MIF_FALSE,
    0,
    MIF_TRUE,
  },
};


static Mif_Inst_Var_Info_t MIFinst_varTable[] = {
  {
    "tran_init",
    "tran initialisation",
    MIF_BOOLEAN,
    MIF_FALSE,
  },
};


extern void cm_triangle(Mif_Private_t *);

static int val_terms             = 0;
static int val_numNames          = 0;
static int val_numInstanceParms  = 1;
static int val_numModelParms     = 5;
static int val_sizeofMIFinstance = sizeof(MIFinstance);
static int val_sizeofMIFmodel    = sizeof(MIFmodel);

SPICEdev cm_triangle_info = {
    .DEVpublic = {
        .name = "triangle",
        .description = "controlled triangle wave oscillator",
        .terms = &val_terms,
        .numNames = &val_numNames,
        .termNames = NULL,
        .numInstanceParms = &val_numInstanceParms,
        .instanceParms = MIFpTable,
        .numModelParms = &val_numModelParms,
        .modelParms = MIFmPTable,
        .flags = 0,

        .cm_func = cm_triangle,
        .num_conn = 2,
        .conn = MIFconnTable,
        .num_param = 5,
        .param = MIFparamTable,
        .num_inst_var = 1,
        .inst_var = MIFinst_varTable,
    },

    .DEVparam = NULL,
    .DEVmodParam = MIFmParam,
    .DEVload = MIFload,
    .DEVsetup = MIFsetup,
    .DEVunsetup = MIFunsetup,
    .DEVpzSetup = NULL,
    .DEVtemperature = NULL,
    .DEVtrunc = MIFtrunc,
    .DEVfindBranch = NULL,
    .DEVacLoad = MIFload,
    .DEVaccept = NULL,
    .DEVdestroy = MIFdestroy,
    .DEVmodDelete = MIFmDelete,
    .DEVdelete = MIFdelete,
    .DEVsetic = NULL,
    .DEVask = MIFask,
    .DEVmodAsk = MIFmAsk,
    .DEVpzLoad = NULL,
    .DEVconvTest = MIFconvTest,
    .DEVsenSetup = NULL,
    .DEVsenLoad = NULL,
    .DEVsenUpdate = NULL,
    .DEVsenAcLoad = NULL,
    .DEVsenPrint = NULL,
    .DEVsenTrunc = NULL,
    .DEVdisto = NULL,
    .DEVnoise = NULL,
    .DEVsoaCheck = NULL,
    .DEVinstSize = &val_sizeofMIFinstance,
    .DEVmodSize = &val_sizeofMIFmodel,

#ifdef CIDER
    .DEVdump = NULL,
    .DEVacct = NULL,
#endif
};

