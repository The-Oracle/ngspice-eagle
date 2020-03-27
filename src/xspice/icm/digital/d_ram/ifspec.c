
/*
 * Structures for model: d_ram
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
    IOP("select_value", 0, IF_INTEGER, "decimal active value for select line comparison"),
    IOP("ic", 1, IF_INTEGER, "initial bit state @ DC"),
    IOP("read_delay", 2, IF_REAL, "read delay from address/select/write_en active"),
    IOP("data_load", 3, IF_REAL, "data_in load value (F)"),
    IOP("address_load", 4, IF_REAL, "address line load value (F)"),
    IOP("select_load", 5, IF_REAL, "select load value (F)"),
    IOP("enable_load", 6, IF_REAL, "enable line load value (F)"),
};


static Mif_Port_Type_t MIFportEnum0[] = {
	MIF_DIGITAL,
};


static char *MIFportStr0[] = {
	"d",
};


static Mif_Port_Type_t MIFportEnum1[] = {
	MIF_DIGITAL,
};


static char *MIFportStr1[] = {
	"d",
};


static Mif_Port_Type_t MIFportEnum2[] = {
	MIF_DIGITAL,
};


static char *MIFportStr2[] = {
	"d",
};


static Mif_Port_Type_t MIFportEnum3[] = {
	MIF_DIGITAL,
};


static char *MIFportStr3[] = {
	"d",
};


static Mif_Port_Type_t MIFportEnum4[] = {
	MIF_DIGITAL,
};


static char *MIFportStr4[] = {
	"d",
};


static Mif_Conn_Info_t MIFconnTable[] = {
  {
    "data_in",
    "data input line(s)",
    MIF_IN,
    MIF_DIGITAL,
    "d",
    1,
    MIFportEnum0,
    MIFportStr0,
    MIF_TRUE,
    MIF_TRUE,
    1,
    MIF_FALSE,
    0,
    MIF_FALSE,
  },
  {
    "data_out",
    "data output line(s)",
    MIF_OUT,
    MIF_DIGITAL,
    "d",
    1,
    MIFportEnum1,
    MIFportStr1,
    MIF_TRUE,
    MIF_TRUE,
    1,
    MIF_FALSE,
    0,
    MIF_FALSE,
  },
  {
    "address",
    "address input line(s)",
    MIF_IN,
    MIF_DIGITAL,
    "d",
    1,
    MIFportEnum2,
    MIFportStr2,
    MIF_TRUE,
    MIF_TRUE,
    1,
    MIF_FALSE,
    0,
    MIF_FALSE,
  },
  {
    "write_en",
    "write enable",
    MIF_IN,
    MIF_DIGITAL,
    "d",
    1,
    MIFportEnum3,
    MIFportStr3,
    MIF_FALSE,
    MIF_FALSE,
    0,
    MIF_FALSE,
    0,
    MIF_FALSE,
  },
  {
    "select",
    "chip select line(s)",
    MIF_IN,
    MIF_DIGITAL,
    "d",
    1,
    MIFportEnum4,
    MIFportStr4,
    MIF_TRUE,
    MIF_TRUE,
    1,
    MIF_TRUE,
    16,
    MIF_FALSE,
  },
};


static Mif_Param_Info_t MIFparamTable[] = {
  {
    "select_value",
    "decimal active value for select line comparison",
    MIF_INTEGER,
    MIF_TRUE,
    {MIF_FALSE, 1, 0.0, {0.0, 0.0}, NULL},
    MIF_TRUE,
    {MIF_FALSE, 0, 0.0, {0.0, 0.0}, NULL},
    MIF_TRUE,
    {MIF_FALSE, 32767, 0.0, {0.0, 0.0}, NULL},
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
    "ic",
    "initial bit state @ DC",
    MIF_INTEGER,
    MIF_TRUE,
    {MIF_FALSE, 2, 0.0, {0.0, 0.0}, NULL},
    MIF_TRUE,
    {MIF_FALSE, 0, 0.0, {0.0, 0.0}, NULL},
    MIF_TRUE,
    {MIF_FALSE, 2, 0.0, {0.0, 0.0}, NULL},
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
    "read_delay",
    "read delay from address/select/write_en active",
    MIF_REAL,
    MIF_TRUE,
    {MIF_FALSE, 0, 1.000000e-07, {0.0, 0.0}, NULL},
    MIF_TRUE,
    {MIF_FALSE, 0, 1.000000e-12, {0.0, 0.0}, NULL},
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
    "data_load",
    "data_in load value (F)",
    MIF_REAL,
    MIF_TRUE,
    {MIF_FALSE, 0, 1.000000e-12, {0.0, 0.0}, NULL},
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
    "address_load",
    "address line load value (F)",
    MIF_REAL,
    MIF_TRUE,
    {MIF_FALSE, 0, 1.000000e-12, {0.0, 0.0}, NULL},
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
    "select_load",
    "select load value (F)",
    MIF_REAL,
    MIF_TRUE,
    {MIF_FALSE, 0, 1.000000e-12, {0.0, 0.0}, NULL},
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
    "enable_load",
    "enable line load value (F)",
    MIF_REAL,
    MIF_TRUE,
    {MIF_FALSE, 0, 1.000000e-12, {0.0, 0.0}, NULL},
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
};


extern void cm_d_ram(Mif_Private_t *);

static int val_terms             = 0;
static int val_numNames          = 0;
static int val_numInstanceParms  = 0;
static int val_numModelParms     = 7;
static int val_sizeofMIFinstance = sizeof(MIFinstance);
static int val_sizeofMIFmodel    = sizeof(MIFmodel);

SPICEdev cm_d_ram_info = {
    .DEVpublic = {
        .name = "d_ram",
        .description = "digital random-access memory",
        .terms = &val_terms,
        .numNames = &val_numNames,
        .termNames = NULL,
        .numInstanceParms = &val_numInstanceParms,
        .instanceParms = NULL,
        .numModelParms = &val_numModelParms,
        .modelParms = MIFmPTable,
        .flags = 0,

        .cm_func = cm_d_ram,
        .num_conn = 5,
        .conn = MIFconnTable,
        .num_param = 7,
        .param = MIFparamTable,
        .num_inst_var = 0,
        .inst_var = NULL,
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

