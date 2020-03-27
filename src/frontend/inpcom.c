/**********
Copyright 1990 Regents of the University of California.  All rights reserved.
Author: 1985 Wayne A. Christopher
**********/

/*
  For dealing with spice input decks and command scripts

  Central function is inp_readall()
*/

#include "ngspice/ngspice.h"

#include "ngspice/compatmode.h"
#include "ngspice/cpdefs.h"
#include "ngspice/dvec.h"
#include "ngspice/ftedefs.h"
#include "ngspice/fteext.h"
#include "ngspice/fteinp.h"

#include <limits.h>
#include <stdlib.h>

#include <sys/stat.h>
#include <sys/types.h>

#if !defined(__MINGW32__) && !defined(_MSC_VER)
#include <unistd.h>
#endif

#include "../misc/util.h" /* ngdirname() */
#include "inpcom.h"
#include "ngspice/stringskip.h"
#include "ngspice/stringutil.h"
#include "ngspice/wordlist.h"
#include "subckt.h"
#include "variable.h"

#ifdef XSPICE
/* gtri - add - 12/12/90 - wbk - include new stuff */
#include "ngspice/enh.h"
#include "ngspice/ipctiein.h"
/* gtri - end - 12/12/90 */
#endif

/* SJB - Uncomment this line for debug tracing */
/*#define TRACE*/

/* globals -- wanted to avoid complicating inp_readall interface */
#define N_LIBRARIES 1000
#define N_PARAMS 1000
#define N_SUBCKT_W_PARAMS 4000

#define NPARAMS 10000
#define FCN_PARAMS 1000

#define VALIDCHARS "!$%_#?@.[]&"

static struct library {
    char *realpath;
    char *habitat;
    struct card *deck;
} libraries[N_LIBRARIES];

static int num_libraries;

struct names {
    char *names[N_SUBCKT_W_PARAMS];
    int num_names;
};

struct function_env {
    struct function_env *up;

    struct function {
        struct function *next;
        char *name;
        char *body;
        char *params[N_PARAMS];
        int num_parameters;
        const char *accept;
    } * functions;
};

struct func_temper {
    char *funcname;
    int subckt_depth;
    int subckt_count;
    struct func_temper *next;
};

extern void line_free_x(struct card *deck, bool recurse);

COMPATMODE_T inp_compat_mode;

/* Collect information for dynamic allocation of numparam arrays */
/* number of lines in input deck */
int dynmaxline; /* inpcom.c 1529 */
/* number of lines in deck after expansion */
int dynMaxckt = 0; /* subckt.c 307 */
/* number of parameter substitutions */
long dynsubst; /* spicenum.c 221 */

static bool has_if = FALSE; /* if we have an .if ... .endif pair */

static char *readline(FILE *fd);
static int get_number_terminals(char *c);
static void inp_stripcomments_deck(struct card *deck, bool cs);
static void inp_stripcomments_line(char *s, bool cs);
static void inp_fix_for_numparam(
        struct names *subckt_w_params, struct card *deck);
static void inp_remove_excess_ws(struct card *deck);
static void expand_section_references(struct card *deck, char *dir_name);
static void inp_grab_func(struct function_env *, struct card *deck);
static void inp_fix_inst_calls_for_numparam(
        struct names *subckt_w_params, struct card *deck);
static void inp_expand_macros_in_func(struct function_env *);
static struct card *inp_expand_macros_in_deck(
        struct function_env *, struct card *deck);
static void inp_fix_param_values(struct card *deck);
static void inp_reorder_params(
        struct names *subckt_w_params, struct card *list_head);
static int inp_split_multi_param_lines(struct card *deck, int line_number);
static void inp_sort_params(struct card *param_cards,
        struct card *card_bf_start, struct card *s_c, struct card *e_c);
static char *inp_remove_ws(char *s);
static void inp_compat(struct card *deck);
static void inp_bsource_compat(struct card *deck);
static bool inp_temper_compat(struct card *card);
static void inp_meas_current(struct card *card);
static void inp_dot_if(struct card *deck);
static char *inp_modify_exp(char *expression);
static struct func_temper *inp_new_func(char *funcname, char *funcbody,
        struct card *card, int *sub_count, int subckt_depth);
static void inp_delete_funcs(struct func_temper *funcs);

static bool chk_for_line_continuation(char *line);
static void comment_out_unused_subckt_models(struct card *start_card);
static char inp_get_elem_ident(char *type);
static void rem_mfg_from_models(struct card *start_card);
static void inp_fix_macro_param_func_paren_io(struct card *begin_card);
static void inp_fix_gnd_name(struct card *deck);
static void inp_chk_for_multi_in_vcvs(struct card *deck, int *line_number);
static void inp_add_control_section(struct card *deck, int *line_number);
static char *get_quoted_token(char *string, char **token);
static void replace_token(char *string, char *token, int where, int total);
static void inp_add_series_resistor(struct card *deck);
static void subckt_params_to_param(struct card *deck);
static void inp_fix_temper_in_param(struct card *deck);
static void inp_fix_agauss_in_param(struct card *deck, char *fcn);
static void inp_vdmos_model(struct card *deck);
static void inp_check_syntax(struct card *deck);

static char *inp_spawn_brace(char *s);

static char *inp_pathresolve(const char *name);
static char *inp_pathresolve_at(char *name, char *dir);
static char *search_plain_identifier(char *str, const char *identifier);

static struct nscope *inp_add_levels(struct card *deck);
static struct card_assoc *find_subckt(struct nscope *scope, const char *name);
static void inp_rem_levels(struct nscope *root);
static void inp_rem_unused_models(struct nscope *root, struct card *deck);
static struct modellist *inp_find_model(struct nscope *scope, const char *name);

void tprint(struct card *deck);
static struct card *pspice_compat(struct card *newcard);
static void pspice_compat_a(struct card *oldcard);
static struct card *ltspice_compat(struct card *oldcard);
static void ltspice_compat_a(struct card *oldcard);

struct inp_read_t {
    struct card *cc;
    int line_number;
};

static struct inp_read_t inp_read(
        FILE *fp, int call_depth, char *dir_name, bool comfile, bool intfile);


#ifndef XSPICE
static void inp_poly_err(struct card *deck);
#endif


/* insert a new card, just behind the given card */
static struct card *insert_new_line(
        struct card *card, char *line, int linenum, int linenum_orig)
{
    struct card *x = TMALLOC(struct card, 1);

    x->nextcard = card ? card->nextcard : NULL;
    x->error = NULL;
    x->actualLine = NULL;
    x->line = line;
    x->linenum = linenum;
    x->linenum_orig = linenum_orig;
    x->level = card ? card->level : NULL;

    if (card)
        card->nextcard = x;

    return x;
}


/* insert new_card, just behind the given card */
static struct card *insert_deck(struct card *card, struct card *new_card)
{
    if (card) {
        new_card->nextcard = card->nextcard;
        card->nextcard = new_card;
    }
    else {
        new_card->nextcard = NULL;
    }
    return new_card;
}


static struct library *new_lib(void)
{
    if (num_libraries >= N_LIBRARIES) {
        fprintf(stderr, "ERROR, N_LIBRARIES overflow\n");
        controlled_exit(EXIT_FAILURE);
    }

    return &libraries[num_libraries++];
}


static void delete_libs(void)
{
    int i;

    for (i = 0; i < num_libraries; i++) {
        tfree(libraries[i].realpath);
        tfree(libraries[i].habitat);
        line_free_x(libraries[i].deck, TRUE);
    }
}


static struct library *find_lib(char *name)
{
    int i;

    for (i = 0; i < num_libraries; i++)
        if (cieq(libraries[i].realpath, name))
            return &libraries[i];

    return NULL;
}


static struct card *find_section_definition(struct card *c, char *name)
{
    for (; c; c = c->nextcard) {

        char *line = c->line;

        if (ciprefix(".lib", line)) {

            char *s, *t, *y;

            s = skip_non_ws(line);
            while (isspace_c(*s) || isquote(*s))
                s++;
            for (t = s; *t && !isspace_c(*t) && !isquote(*t); t++)
                ;
            y = t;
            while (isspace_c(*y) || isquote(*y))
                y++;

            if (!*y) {
                /* library section definition: `.lib <section-name>' ..
                 * `.endl' */

                char keep_char = *t;
                *t = '\0';

                if (strcasecmp(name, s) == 0) {
                    *t = keep_char;
                    return c;
                }

                *t = keep_char;
            }
        }
    }

    return NULL;
}


static struct library *read_a_lib(char *y, char *dir_name)
{
    char *yy, *y_resolved;

    struct library *lib;

    y_resolved = inp_pathresolve_at(y, dir_name);

    if (!y_resolved) {
        fprintf(cp_err, "Error: Could not find library file %s\n", y);
        return NULL;
    }

#if defined(__MINGW32__) || defined(_MSC_VER)
    yy = _fullpath(NULL, y_resolved, 0);
#else
    yy = realpath(y_resolved, NULL);
#endif

    if (!yy) {
        fprintf(cp_err, "Error: Could not `realpath' library file %s\n", y);
        controlled_exit(EXIT_FAILURE);
    }

    lib = find_lib(yy);

    if (!lib) {

        FILE *newfp = fopen(y_resolved, "r");

        if (!newfp) {
            fprintf(cp_err, "Error: Could not open library file %s\n", y);
            return NULL;
        }

        /* lib points to a new entry in global lib array
         * libraries[N_LIBRARIES] */
        lib = new_lib();

        lib->realpath = copy(yy);
        lib->habitat = ngdirname(yy);

        lib->deck =
                inp_read(newfp, 1 /*dummy*/, lib->habitat, FALSE, FALSE).cc;

        fclose(newfp);
    }

    tfree(yy);
    tfree(y_resolved);

    return lib;
}


static struct names *new_names(void)
{
    struct names *p = TMALLOC(struct names, 1);
    p->num_names = 0;

    return p;
}


static void delete_names(struct names *p)
{
    int i;
    for (i = 0; i < p->num_names; i++)
        tfree(p->names[i]);
    tfree(p);
}



/* line1
   + line2
   ---->
   line1 line 2
   Proccedure: store regular card in prev, skip comment lines (*..) and some
   others
   */

static void inp_stitch_continuation_lines(struct card *working)
{
    struct card *prev = NULL;

    while (working) {
        char *s, c, *buffer;

        for (s = working->line; (c = *s) != '\0' && c <= ' '; s++)
            ;

#ifdef TRACE
        /* SDB debug statement */
        printf("In inp_read, processing linked list element line = %d, s = "
               "%s . . . \n",
                working->linenum, s);
#endif

        switch (c) {
            case '#':
            case '$':
            case '*':
            case '\0':
                /* skip these cards, and keep prev as the last regular card */
                working = working->nextcard; /* for these chars, go to next
                                                card */
                break;

            case '+': /* handle continuation */
                if (!prev) {
                    working->error =
                            copy("Illegal continuation line: ignored.");
                    working = working->nextcard;
                    break;
                }

                /* We now may have lept over some comment lines, which are
                located among the continuation lines. We have to delete them
                here to prevent a memory leak */
                while (prev->nextcard != working) {
                    struct card *tmpl = prev->nextcard->nextcard;
                    line_free_x(prev->nextcard, FALSE);
                    prev->nextcard = tmpl;
                }

                /* create buffer and write last and current line into it. */
                buffer = tprintf("%s %s", prev->line, s + 1);

                /* replace prev->line by buffer */
                s = prev->line;
                prev->line = buffer;
                prev->nextcard = working->nextcard;
                working->nextcard = NULL;
                /* add original line to prev->actualLine */
                if (prev->actualLine) {
                    struct card *end;
                    for (end = prev->actualLine; end->nextcard;
                            end = end->nextcard)
                        ;
                    end->nextcard = working;
                    tfree(s);
                }
                else {
                    prev->actualLine =
                            insert_new_line(NULL, s, prev->linenum, 0);
                    prev->actualLine->level = prev->level;
                    prev->actualLine->nextcard = working;
                }
                working = prev->nextcard;
                break;

            default: /* regular one-line card */
                prev = working;
                working = working->nextcard;
                break;
        }
    }
}


/*
 * search for `=' assignment operator
 *   take care of `!=' `<=' `==' and `>='
 */

char *find_assignment(const char *str)
{
    const char *p = str;

    while ((p = strchr(p, '=')) != NULL) {

        // check for equality '=='
        if (p[1] == '=') {
            p += 2;
            continue;
        }

        // check for '!=', '<=', '>='
        if (p > str)
            if (p[-1] == '!' || p[-1] == '<' || p[-1] == '>') {
                p += 1;
                continue;
            }

        return (char *) p;
    }

    return NULL;
}


/*
 * backward search for an assignment
 *   fixme, doesn't honour neither " nor ' quotes
 */

char *find_back_assignment(const char *p, const char *start)
{
    while (--p >= start) {
        if (*p != '=')
            continue;
        // check for '!=', '<=', '>=', '=='
        if (p <= start || !strchr("!<=>", p[-1]))
            return (char *) p;
        p--;
    }

    return NULL;
}


/* Set a compatibility flag.
Currently available are flags for:
- ngspice (standard)
- a commercial simulator
- Spice3
- all compatibility stuff
*/
static COMPATMODE_T ngspice_compat_mode(void)
{
    char behaviour[80];

    if (cp_getvar("ngbehavior", CP_STRING, behaviour, sizeof(behaviour))) {
        if (strcasecmp(behaviour, "all") == 0)
            return COMPATMODE_ALL;
        if (strcasecmp(behaviour, "hs") == 0)
            return COMPATMODE_HS;
        if (strcasecmp(behaviour, "ps") == 0)
            return COMPATMODE_PS;
        if (strcasecmp(behaviour, "lt") == 0)
            return COMPATMODE_LT;
        if (strcasecmp(behaviour, "ltps") == 0)
            return COMPATMODE_LTPS;
        if (strcasecmp(behaviour, "psa") == 0)
            return COMPATMODE_PSA;
        if (strcasecmp(behaviour, "lta") == 0)
            return COMPATMODE_LTA;
        if (strcasecmp(behaviour, "ltpsa") == 0)
            return COMPATMODE_LTPSA;
        if (strcasecmp(behaviour, "spice3") == 0)
            return COMPATMODE_SPICE3;
    }

    return COMPATMODE_ALL;
}

/*-------------------------------------------------------------------------
  Read the entire input file and return  a pointer to the first line of
  the linked list of 'card' records in data.  The pointer is stored in
  *data.
  Called from fcn inp_spsource() in inp.c to load circuit or command files.
  Called from fcn com_alter_mod() in device.c to load model files.
  Called from here to load .library or .include files.

  Procedure:
  read in all lines & put them in the struct cc
  read next line
  process .TITLE line
  store contents in string new_title
  process .lib lines
  read file and library name, open file using fcn inp_pathopen()
  read file contents and put into struct libraries[].deck, one entry per .lib
  line process .inc lines read file and library name, open file using fcn
  inp_pathopen() read file contents and add lines to cc make line entry lower
  case allow for shell end of line continuation (\\) add '+' to beginning of
  next line add line entry to list cc add '.global gnd' add libraries find
  library section add lines add .end card strip end-of-line comments make
  continuation lines a single line
  *** end of processing for command files ***
  start preparation of input deck for numparam
  ...
  debug printout to debug-out.txt
  remove the 'level' entries from each card
  *-------------------------------------------------------------------------*/

struct card *inp_readall(FILE *fp, char *dir_name, bool comfile, bool intfile,
        bool *expr_w_temper_p)
{
    struct card *cc;
    struct inp_read_t rv;

    num_libraries = 0;
    inp_compat_mode = ngspice_compat_mode();

    rv = inp_read(fp, 0, dir_name, comfile, intfile);
    cc = rv.cc;

    /* files starting with *ng_script are user supplied command files */
    if (cc && ciprefix("*ng_script", cc->line))
        comfile = TRUE;

    /* The following processing of an input file is not required for command
       files like spinit or .spiceinit, so return command files here. */

    if (!comfile && cc) {

        unsigned int no_braces; /* number of '{' */
        size_t max_line_length; /* max. line length in input deck */
        struct card *tmp_ptr1;
        struct names *subckt_w_params = new_names();

        /* skip title line */
        struct card *working = cc->nextcard;

        delete_libs();

        /* some syntax checks, including title line */
        inp_check_syntax(cc);

        if (inp_compat_mode == COMPATMODE_LTA)
            ltspice_compat_a(working);
        else if (inp_compat_mode == COMPATMODE_PSA)
            pspice_compat_a(working);
        else if (inp_compat_mode == COMPATMODE_LTPSA) {
            ltspice_compat_a(working);
            pspice_compat_a(working);
        }

        struct nscope *root = inp_add_levels(working);

        inp_fix_for_numparam(subckt_w_params, working);

        inp_remove_excess_ws(working);

        inp_vdmos_model(working);
        /* don't remove unused model if we have an .if clause, because we
           cannot yet decide here which model we finally will need */
        if (!has_if) {
            comment_out_unused_subckt_models(working);
            inp_rem_unused_models(root, working);
        }

        rem_mfg_from_models(working);

        subckt_params_to_param(working);

        rv.line_number = inp_split_multi_param_lines(working, rv.line_number);

        inp_fix_macro_param_func_paren_io(working);

        static char *statfcn[] = {
                "agauss", "gauss", "aunif", "unif", "limit"};
        int ii;
        for (ii = 0; ii < 5; ii++)
            inp_fix_agauss_in_param(working, statfcn[ii]);
        inp_fix_temper_in_param(working);

        inp_expand_macros_in_deck(NULL, working);
        inp_fix_param_values(working);

        inp_reorder_params(subckt_w_params, cc);
        inp_fix_inst_calls_for_numparam(subckt_w_params, working);

        delete_names(subckt_w_params);
        subckt_w_params = NULL;
        if (!cp_getvar("no_auto_gnd", CP_BOOL, NULL, 0))
            inp_fix_gnd_name(working);
        inp_chk_for_multi_in_vcvs(working, &rv.line_number);

        if (cp_getvar("addcontrol", CP_BOOL, NULL, 0))
            inp_add_control_section(working, &rv.line_number);
#ifndef XSPICE
        inp_poly_err(working);
#endif
        bool expr_w_temper = FALSE;
        if (inp_compat_mode != COMPATMODE_SPICE3) {
            /* Do all the compatibility stuff here */
            working = cc->nextcard;
            inp_meas_current(working);
            /* E, G, L, R, C compatibility transformations */
            inp_compat(working);
            working = cc->nextcard;
            /* B source numparam compatibility transformation */
            inp_bsource_compat(working);
            inp_dot_if(working);
            expr_w_temper = inp_temper_compat(working);
        }
        if (expr_w_temper_p)
            *expr_w_temper_p = expr_w_temper;

        inp_add_series_resistor(working);

        /* get max. line length and number of lines in input deck,
           and renumber the lines,
           count the number of '{' per line as an upper estimate of the number
           of parameter substitutions in a line */
        dynmaxline = 0;
        max_line_length = 0;
        no_braces = 0;
        for (tmp_ptr1 = cc; tmp_ptr1; tmp_ptr1 = tmp_ptr1->nextcard) {
            char *s;
            unsigned int braces_per_line = 0;
            /* count number of lines */
            dynmaxline++;
            /* renumber the lines of the processed input deck */
            tmp_ptr1->linenum = dynmaxline;
            if (max_line_length < strlen(tmp_ptr1->line))
                max_line_length = strlen(tmp_ptr1->line);
            /* count '{' */
            for (s = tmp_ptr1->line; *s; s++)
                if (*s == '{')
                    braces_per_line++;
            if (no_braces < braces_per_line)
                no_braces = braces_per_line;
        }

        if (ft_ngdebug) {
            FILE *fd = fopen("debug-out.txt", "w");
            if (fd) {
                /*debug: print into file*/
                struct card *t;
                fprintf(fd,
                        "**************** uncommented deck "
                        "**************\n\n");
                /* always print first line */
                fprintf(fd, "%6d  %6d  %s\n", cc->linenum_orig, cc->linenum,
                        cc->line);
                /* here without out-commented lines */
                for (t = cc->nextcard; t; t = t->nextcard) {
                    if (*(t->line) == '*')
                        continue;
                    fprintf(fd, "%6d  %6d  %s\n", t->linenum_orig, t->linenum,
                            t->line);
                }
                fprintf(fd,
                        "\n****************** complete deck "
                        "***************\n\n");
                /* now completely */
                for (t = cc; t; t = t->nextcard)
                    fprintf(fd, "%6d  %6d  %s\n", t->linenum_orig, t->linenum,
                            t->line);
                fclose(fd);

                fprintf(stdout,
                        "max line length %d, max subst. per line %d, number "
                        "of lines %d\n",
                        (int) max_line_length, no_braces, dynmaxline);
            }
            else
                fprintf(stderr,
                        "Warning: Cannot open file debug-out.txt for saving "
                        "debug info\n");
        }
        inp_rem_levels(root);
    }

    return cc;
}


struct inp_read_t inp_read(
        FILE *fp, int call_depth, char *dir_name, bool comfile, bool intfile)
/* fp: in, pointer to file to be read,
   call_depth: in, nested call to fcn
   dir_name: in, name of directory of file to be read
   comfile: in, TRUE if command file (e.g. spinit, .spiceinit)
   intfile: in, TRUE if deck is generated from internal circarray
*/
{
    struct inp_read_t rv;
    struct card *end = NULL, *cc = NULL;
    char *buffer = NULL;
    /* segfault fix */
#ifdef XSPICE
    char big_buff[5000];
    int line_count = 0;
#endif
    char *new_title = NULL;
    int line_number =
            1; /* sjb - renamed to avoid confusion with struct card */
    int line_number_orig = 1;
    int cirlinecount = 0; /* length of circarray */
    static int is_control = 0; /* We are reading from a .control section */

    bool found_end = FALSE, shell_eol_continuation = FALSE;

    /* First read in all lines & put them in the struct cc */
    for (;;) {
        /* derive lines from circarray */
        if (intfile) {
            buffer = circarray[cirlinecount++];
            if (!buffer) {
                tfree(circarray);
                break;
            }
        }
        /* read lines from file fp */
        else {

#ifdef XSPICE
            /* gtri - modify - 12/12/90 - wbk - read from mailbox if ipc
             * enabled */

            /* If IPC is not enabled, do equivalent of what SPICE did before
             */
            if (!g_ipc.enabled) {
                if (call_depth == 0 && line_count == 0) {
                    line_count++;
                    if (fgets(big_buff, 5000, fp))
                        buffer = copy(big_buff);
                }
                else {
                    buffer = readline(fp);
                    if (!buffer)
                        break;
                }
            }
            else {
                /* else, get the line from the ipc channel. */
                /* We assume that newlines are not sent by the client */
                /* so we add them here */
                char ipc_buffer[1025]; /* Had better be big enough */
                int ipc_len;
                Ipc_Status_t ipc_status =
                        ipc_get_line(ipc_buffer, &ipc_len, IPC_WAIT);
                if (ipc_status == IPC_STATUS_END_OF_DECK) {
                    buffer = NULL;
                    break;
                }
                else if (ipc_status == IPC_STATUS_OK) {
                    buffer = TMALLOC(char, strlen(ipc_buffer) + 3);
                    strcpy(buffer, ipc_buffer);
                    strcat(buffer, "\n");
                }
                else { /* No good way to report this so just die */
                    controlled_exit(EXIT_FAILURE);
                }
            }

            /* gtri - end - 12/12/90 */
#else

            buffer = readline(fp);
            if (!buffer)
                break;

#endif
        }

#ifdef TRACE
        /* SDB debug statement */
        printf("in inp_read, just read   %s", buffer);
#endif

        if (!buffer)
            continue;

        /* OK -- now we have loaded the next line into 'buffer'.  Process it.
         */
        /* If input line is blank, ignore it & continue looping.  */
        if ((strcmp(buffer, "\n") == 0) || (strcmp(buffer, "\r\n") == 0))
            if (call_depth != 0 || (call_depth == 0 && cc != NULL)) {
                line_number_orig++;
                tfree(buffer); /* was allocated by readline() */
                continue;
            }

        if (*buffer == '@') {
            tfree(buffer); /* was allocated by readline() */
            break;
        }

        /* now check if we are in a .control section */
        if (ciprefix(".control", buffer))
            is_control++;
        else if (ciprefix(".endc", buffer))
            is_control--;

        /* now handle .title statement */
        if (ciprefix(".title", buffer)) {
            char *s;
            s = skip_non_ws(buffer); /* skip over .title */
            s = skip_ws(s); /* advance past space chars */

            /* only the last title line remains valid */
            tfree(new_title);
            new_title = copy(s);
            if ((s = strchr(new_title, '\n')) != NULL)
                *s = '\0';
            if ((s = strchr(new_title, '\r')) != NULL)
                *s = '\0';
            *buffer = '*'; /* change .TITLE line to comment line */
        }

        /* now handle old style .lib entries */
        /* new style .lib entries handling is in expand_section_references()
         */
        if (ciprefix(".lib", buffer))
            if (inp_compat_mode == COMPATMODE_PS ||
                    inp_compat_mode == COMPATMODE_PSA ||
                    inp_compat_mode == COMPATMODE_LTPS ||
                    inp_compat_mode == COMPATMODE_LTPSA) {
                /* compatibility mode,
                 *   this is neither a libray section definition nor a
                 * reference interpret as old style .lib <file name> (no lib
                 * name given)
                 */
                char *s = skip_non_ws(buffer); /* skip over .lib */
                fprintf(cp_err, "  File included as:   .inc %s\n", s);
                memcpy(buffer, ".inc", 4);
            }

        /* now handle .include statements */
        if (ciprefix(".include", buffer) || ciprefix(".inc", buffer)) {

            char *y = NULL;
            char *s;

            struct card *newcard;

            inp_stripcomments_line(buffer, FALSE);

            s = skip_non_ws(buffer); /* advance past non-space chars */

            s = get_quoted_token(s, &y);

            if (!y) {
                fprintf(cp_err, "Error: .include filename missing\n");
                tfree(buffer); /* was allocated by readline() */
                controlled_exit(EXIT_FAILURE);
            }

            {
                char *y_resolved = inp_pathresolve_at(y, dir_name);
                char *y_dir_name;
                FILE *newfp;

                if (!y_resolved) {
                    fprintf(cp_err, "Error: Could not find include file %s\n",
                            y);
                    rv.line_number = line_number;
                    rv.cc = NULL;
                    return rv;
                }

                newfp = fopen(y_resolved, "r");

                if (!newfp) {
                    fprintf(cp_err, "Error: .include statement failed.\n");
                    tfree(buffer); /* allocated by readline() above */
                    controlled_exit(EXIT_FAILURE);
                }

                y_dir_name = ngdirname(y_resolved);

                newcard = inp_read(
                        newfp, call_depth + 1, y_dir_name, FALSE, FALSE)
                                  .cc; /* read stuff in include file into
                                          netlist */

                tfree(y_dir_name);
                tfree(y_resolved);

                (void) fclose(newfp);
            }

            /* Make the .include a comment */
            *buffer = '*';

            /* append `buffer' to the (cc, end) chain of decks */
            {
                end = insert_new_line(
                        end, copy(buffer), line_number, line_number);

                if (!cc)
                    cc = end;

                line_number++;
            }

            if (newcard) {
                if (inp_compat_mode == COMPATMODE_LT)
                    newcard = ltspice_compat(newcard);
                else if (inp_compat_mode == COMPATMODE_PS)
                    newcard = pspice_compat(newcard);
                else if (inp_compat_mode == COMPATMODE_LTPS) {
                    newcard = ltspice_compat(newcard);
                    newcard = pspice_compat(newcard);
                }

                int line_number_inc = 1;
                end->nextcard = newcard;
                /* Renumber the lines */
                for (end = newcard; end && end->nextcard;
                        end = end->nextcard) {
                    end->linenum = line_number++;
                    end->linenum_orig = line_number_inc++;
                }
                end->linenum =
                        line_number++; /* SJB - renumber the last line */
                end->linenum_orig =
                        line_number_inc++; /* SJB - renumber the last line */
            }

            /* Fix the buffer up a bit. */
            (void) strncpy(buffer + 1, "end of: ", 8);
        } /*  end of .include handling  */

        /* loop through 'buffer' until end is reached. Make all letters lower
         * case except for the commands given below. Special treatment for
         * commands 'hardcopy' and 'plot', where all letters are made lower
         * case except for the tokens following xlabel, ylabel and title.
         * These tokens may contain spaces, if they are enclosed in single or
         * double quotes. Single quotes are later on swallowed and disappear,
         * double quotes are printed. */
        {
            char *s;
            if (ciprefix("plot", buffer) ||
                ciprefix("gnuplot", buffer) ||
                ciprefix("hardcopy", buffer)) {
                /* lower case excluded for tokens following title, xlabel,
                 * ylabel. tokens may contain spaces, then they have to be
                 * enclosed in quotes. keywords and tokens have to be
                 * separated by spaces. */
                int j;
                char t = ' ';
                for (s = buffer; *s && (*s != '\n'); s++) {
                    *s = tolower_c(*s);
                    if (ciprefix("title", s)) {
                        /* jump beyond title */
                        for (j = 0; j < 5; j++) {
                            s++;
                            *s = tolower_c(*s);
                        }
                        while (*s == ' ')
                            s++;
                        if (!s || (*s == '\n'))
                            break;
                        /* check if single quote is at start of token */
                        else if (*s == '\'') {
                            s++;
                            t = '\'';
                        }
                        /* check if double quote is at start of token */
                        else if (*s == '\"') {
                            s++;
                            t = '\"';
                        }
                        else
                            t = ' ';
                        /* jump beyond token without lower casing */
                        while ((*s != '\n') && (*s != t))
                            s++;
                    }
                    else if (ciprefix("xlabel", s) || ciprefix("ylabel", s)) {
                        /* jump beyond xlabel, ylabel */
                        for (j = 0; j < 6; j++) {
                            s++;
                            *s = tolower_c(*s);
                        }
                        while (*s == ' ')
                            s++;
                        if (!s || (*s == '\n'))
                            break;
                        /* check if single quote is at start of token */
                        else if (*s == '\'') {
                            s++;
                            t = '\'';
                        }
                        /* check if double quote is at start of token */
                        else if (*s == '\"') {
                            s++;
                            t = '\"';
                        }
                        else
                            t = ' ';
                        /* jump beyond token without lower casing */
                        while ((*s != '\n') && (*s != t))
                            s++;
                    }
                }
            }
            else if (ciprefix("print", buffer) ||
                     ciprefix("eprint", buffer) ||
                     ciprefix("asciiplot", buffer)) {
                /* lower case excluded for tokens following output redirection
                 * '>' */
                bool redir = FALSE;
                for (s = buffer; *s && (*s != '\n'); s++) {
                    if (*s == '>')
                        redir = TRUE; /* do not lower, but move to end of
                                         string */
                    if (!redir)
                        *s = tolower_c(*s);
                }
            }
            /* no lower case letters for lines beginning with: */
            else if (!ciprefix("write", buffer) &&
                     !ciprefix("wrdata", buffer) &&                     
                     !ciprefix(".lib", buffer) &&
                     !ciprefix(".inc", buffer) &&
                     !ciprefix("codemodel", buffer) &&
                     !ciprefix("echo", buffer) &&
                     !ciprefix("shell", buffer) &&
                     !ciprefix("source", buffer) &&
                     !ciprefix("load", buffer) &&
                     !ciprefix("setcs", buffer)) {
                /* lower case for all other lines */
                for (s = buffer; *s && (*s != '\n'); s++)
                    *s = tolower_c(*s);
            }
            else
            {
                /* s points to end of buffer for all cases not treated so far */
                for (s = buffer; *s && (*s != '\n'); s++)
                    ;
            }
            
            /* add Inp_Path to sourcepath variable */
            if (cieq(buffer, "set") || cieq(buffer, "setcs")) {    
                char *p = strstr(buffer, "sourcepath");
                if (p) {
                    p = strchr(buffer, ')');
                    if (p) {
                        *p = 0; // clear ) and insert Inp_Path in between
                        p = tprintf("%s %s ) %s", buffer,
                                Inp_Path ? Inp_Path : "", p + 1);
                        tfree(buffer);
                        buffer = p;
                        /* s points to end of buffer */
                        for (s = buffer; *s && (*s != '\n'); s++)
                            ;
                    }
                }
            }

            if (!*s) {
                // fprintf(cp_err, "Warning: premature EOF\n");
            }
            *s = '\0'; /* Zap the newline. */

            if ((s - 1) >= buffer &&
                    *(s - 1) ==
                            '\r') /* Zop the carriage return under windows */
                *(s - 1) = '\0';
        }

        /* find the true .end command out of .endc, .ends, .endl, .end
         * (comments may follow) */
        if (ciprefix(".end", buffer))
            if ((buffer[4] == '\0') || isspace_c(buffer[4])) {
                found_end = TRUE;
                *buffer = '*';
            }

        if (shell_eol_continuation) {
            char *new_buffer = tprintf("+%s", buffer);

            tfree(buffer);
            buffer = new_buffer;
        }

        /* If \\ at end of line is found, next line in loop will get + (see
         * code above) */
        shell_eol_continuation = chk_for_line_continuation(buffer);

        {
            end = insert_new_line(
                    end, copy(buffer), line_number++, line_number_orig++);

            if (!cc)
                cc = end;
        }

        tfree(buffer);
    } /* end while ((buffer = readline(fp)) != NULL) */

    if (!cc) /* No stuff here */
    {
        rv.line_number = line_number;
        rv.cc = cc;
        return rv;
    }

    /* files starting with *ng_script are user supplied command files */
    if (call_depth == 0 && ciprefix("*ng_script", cc->line))
        comfile = TRUE;

    if (call_depth == 0 && !comfile) {
        if (!cp_getvar("no_auto_gnd", CP_BOOL, NULL, 0))
            insert_new_line(cc, copy(".global gnd"), 1, 0);
        else
            insert_new_line(
                    cc, copy("* gnd is not set to 0 automatically "), 1, 0);

        if (inp_compat_mode == COMPATMODE_ALL ||
                inp_compat_mode == COMPATMODE_HS ||
                inp_compat_mode == COMPATMODE_NATIVE) {
            /* process all library section references */
            expand_section_references(cc, dir_name);
        }
    }

    /*
      add a terminal ".end" card
    */

    if (call_depth == 0 && !comfile)
        if (found_end == TRUE)
            end = insert_new_line(
                    end, copy(".end"), line_number++, line_number_orig++);

    /* Replace first line with the new title, if available */
    if (call_depth == 0 && !comfile && new_title) {
        tfree(cc->line);
        cc->line = new_title;
    }

    /* Strip or convert end-of-line comments.
       Afterwards stitch the continuation lines.
       If the line only contains an end-of-line comment then it is converted
       into a normal comment with a '*' at the start.  Some special handling
       if this is a command file or called from within a .control section. */
    inp_stripcomments_deck(cc->nextcard, comfile || is_control);

    inp_stitch_continuation_lines(cc->nextcard);

    rv.line_number = line_number;
    rv.cc = cc;
    return rv;
}


static bool is_absolute_pathname(const char *p)
{
#if defined(__MINGW32__) || defined(_MSC_VER)
    /* /... or \... or D:\... or D:/... */
    return p[0] == DIR_TERM || p[0] == DIR_TERM_LINUX ||
            (isalpha_c(p[0]) && p[1] == ':' &&
                    (p[2] == DIR_TERM_LINUX || p[2] == DIR_TERM));
#else
    return p[0] == DIR_TERM;
#endif
}


#if 0

static bool
is_plain_filename(const char *p)
{
#if defined(__MINGW32__) || defined(_MSC_VER)
    return
        !strchr(p, DIR_TERM) &&
        !strchr(p, DIR_TERM_LINUX);
#else
    return
        !strchr(p, DIR_TERM);
#endif
}

#endif


FILE *inp_pathopen(char *name, char *mode)
{
    char *path = inp_pathresolve(name);

    if (path) {
        FILE *fp = fopen(path, mode);
        tfree(path);
        return fp;
    }

    return NULL;
}


/*-------------------------------------------------------------------------*
  Look up the variable sourcepath and try everything in the list in order
  if the file isn't in . and it isn't an abs path name.
  *-------------------------------------------------------------------------*/

static char *inp_pathresolve(const char *name)
{
    char buf[BSIZE_SP];
    struct variable *v;
    struct stat st;

#if defined(__MINGW32__) || defined(_MSC_VER)

    /* If variable 'mingwpath' is set: convert mingw /d/... to d:/... */
    if (cp_getvar("mingwpath", CP_BOOL, NULL, 0) &&
            name[0] == DIR_TERM_LINUX && isalpha_c(name[1]) &&
            name[2] == DIR_TERM_LINUX) {
        strcpy(buf, name);
        buf[0] = buf[1];
        buf[1] = ':';
        return inp_pathresolve(buf);
    }

#endif

    /* just try it */
    if (stat(name, &st) == 0)
        return copy(name);

    /* fail if this was an absolute filename or if there is no sourcepath var
     */
    if (is_absolute_pathname(name) ||
            !cp_getvar("sourcepath", CP_LIST, &v, 0))
        return NULL;

    for (; v; v = v->va_next) {

        switch (v->va_type) {
            case CP_STRING:
                cp_wstrip(v->va_string);
                (void) sprintf(
                        buf, "%s%s%s", v->va_string, DIR_PATHSEP, name);
                break;
            case CP_NUM:
                (void) sprintf(buf, "%d%s%s", v->va_num, DIR_PATHSEP, name);
                break;
            case CP_REAL: /* This is foolish */
                (void) sprintf(buf, "%g%s%s", v->va_real, DIR_PATHSEP, name);
                break;
            default:
                fprintf(stderr,
                        "ERROR: enumeration value `CP_BOOL' or `CP_LIST' not "
                        "handled in inp_pathresolve\nAborting...\n");
                controlled_exit(EXIT_FAILURE);
                break;
        }

        if (stat(buf, &st) == 0)
            return copy(buf);
    }

    return (NULL);
}


static char *inp_pathresolve_at(char *name, char *dir)
{
    char buf[BSIZE_SP], *end;

    /* if name is an absolute path name,
     *   or if we haven't anything to prepend anyway
     */

    if (is_absolute_pathname(name) || !dir || !dir[0])
        return inp_pathresolve(name);

    if (name[0] == '~' && name[1] == '/') {
        char *y = cp_tildexpand(name);
        if (y) {
            char *r = inp_pathresolve(y);
            tfree(y);
            return r;
        }
    }

    /*
     * Try in current dir and then in the actual dir the file was read.
     * Current dir . is needed to correctly support absolute paths in
     * sourcepath
     */

    strcpy(buf, ".");

    end = strchr(buf, '\0');
    if (end[-1] != DIR_TERM)
        *end++ = DIR_TERM;

    strcpy(end, name);

    char *r = inp_pathresolve(buf);
    if (r)
        return r;

    strcpy(buf, dir);

    end = strchr(buf, '\0');
    if (end[-1] != DIR_TERM)
        *end++ = DIR_TERM;

    strcpy(end, name);

    return inp_pathresolve(buf);
}


/*-------------------------------------------------------------------------*
 *  This routine reads a line (of arbitrary length), up to a '\n' or 'EOF' *
 *  and returns a pointer to the resulting null terminated string.         *
 *  The '\n' if found, is included in the returned string.                 *
 *  From: jason@ucbopal.BERKELEY.EDU (Jason Venner)                        *
 *  Newsgroups: net.sources                                                *
 *-------------------------------------------------------------------------*/

#define STRGROW 256

static char *readline(FILE *fd)
{
    int c;
    int memlen;
    char *strptr;
    int strlen;

    strlen = 0;
    memlen = STRGROW;
    strptr = TMALLOC(char, memlen);
    memlen -= 1; /* Save constant -1's in while loop */

    while ((c = getc(fd)) != EOF) {

        if (strlen == 0 && (c == '\t' || c == ' ')) /* Leading spaces away */
            continue;

        if (c == '\r')
            continue;
        strptr[strlen++] = (char) c;

        if (strlen >= memlen) {
            memlen += STRGROW;
            if ((strptr = TREALLOC(char, strptr, memlen + 1)) == NULL)
                return (NULL);
        }

        if (c == '\n')
            break;
    }

    if (!strlen) {
        tfree(strptr);
        return (NULL);
    }

    // strptr[strlen] = '\0';
    /* Trim the string */
    strptr = TREALLOC(char, strptr, strlen + 1);
    strptr[strlen] = '\0';

    return (strptr);
}


/* Replace "gnd" by " 0 "
   Delimiters of gnd may be ' ' or ',' or '(' or ')',
   may be disabled by setting variable no_auto_gnd */

static void inp_fix_gnd_name(struct card *c)
{
    for (; c; c = c->nextcard) {

        char *gnd = c->line;

        // if there is a comment or no gnd, go to next line
        if ((*gnd == '*') || !strstr(gnd, "gnd"))
            continue;

        // replace "?gnd?" by "? 0 ?", ? being a ' '  ','  '('  ')'.
        while ((gnd = strstr(gnd, "gnd")) != NULL) {
            if ((isspace_c(gnd[-1]) || gnd[-1] == '(' || gnd[-1] == ',') &&
                    (isspace_c(gnd[3]) || gnd[3] == ')' || gnd[3] == ',')) {
                memcpy(gnd, " 0 ", 3);
            }
            gnd += 3;
        }

        // now remove the extra white spaces around 0
        c->line = inp_remove_ws(c->line);
    }
}


/*
 * transform a VCVS "gate" instance into a XSPICE instance
 *
 *   Exx  out+ out-  <VCVS>  {nand|nor|and|or}(n)
 *   +  in[1]+ in[1]- ... in[n]+ in[n]-
 *   +  x1,y1 x2,y2
 * ==>
 *   Axx  %vd[ in[1]+ in[1]- ... in[n]+ in[n]- ]
 *   +    %vd( out+ out- )  Exx
 *   .model Exx multi_input_pwd ( x = [x1 x2] x = [y1 y2] model =
 * {nand|nor|and|or} )
 *
 * fixme,
 *   `n' is not checked
 *   the x,y list is fixed to length 2
 */

static void inp_chk_for_multi_in_vcvs(struct card *c, int *line_number)
{
    int skip_control = 0;

    for (; c; c = c->nextcard) {

        char *line = c->line;

        /* there is no e source inside .control ... .endc */
        if (ciprefix(".control", line)) {
            skip_control++;
            continue;
        }
        else if (ciprefix(".endc", line)) {
            skip_control--;
            continue;
        }
        else if (skip_control > 0) {
            continue;
        }

        if (*line == 'e') {

            char *fcn_b;

            if (((fcn_b = strstr(line, "nand(")) != NULL ||
                        (fcn_b = strstr(line, "and(")) != NULL ||
                        (fcn_b = strstr(line, "nor(")) != NULL ||
                        (fcn_b = strstr(line, "or(")) != NULL) &&
                    isspace_c(fcn_b[-1])) {
                char keep, *comma_ptr, *xy_values1[5], *xy_values2[5];
                char *out_str, *ctrl_nodes_str,
                        *xy_values1_b = NULL, *ref_str, *fcn_name,
                        *fcn_e = NULL, *out_b, *out_e, *ref_e;
                char *m_instance, *m_model;
                char *xy_values2_b = NULL, *xy_values1_e = NULL,
                     *ctrl_nodes_b = NULL, *ctrl_nodes_e = NULL;
                int xy_count1, xy_count2;
                bool ok = FALSE;

#ifndef XSPICE
                fprintf(stderr,
                        "\n"
                        "Error: XSPICE is required to run the 'multi-input "
                        "pwl' option in line %d\n"
                        "  %s\n"
                        "\n"
                        "See manual chapt. 31 for installation "
                        "instructions\n",
                        *line_number, line);
                controlled_exit(EXIT_BAD);
#endif

                do {
                    ref_e = skip_non_ws(line);

                    out_b = skip_ws(ref_e);

                    out_e = skip_back_ws(fcn_b, out_b);
                    if (out_e <= out_b)
                        break;

                    fcn_e = strchr(fcn_b, '(');

                    ctrl_nodes_b = strchr(fcn_e, ')');
                    if (!ctrl_nodes_b)
                        break;
                    ctrl_nodes_b = skip_ws(ctrl_nodes_b + 1);

                    comma_ptr = strchr(ctrl_nodes_b, ',');
                    if (!comma_ptr)
                        break;

                    xy_values1_b = skip_back_ws(comma_ptr, ctrl_nodes_b);
                    if (xy_values1_b[-1] == '}') {
                        while (--xy_values1_b >= ctrl_nodes_b)
                            if (*xy_values1_b == '{')
                                break;
                    }
                    else {
                        xy_values1_b =
                                skip_back_non_ws(xy_values1_b, ctrl_nodes_b);
                    }
                    if (xy_values1_b <= ctrl_nodes_b)
                        break;

                    ctrl_nodes_e = skip_back_ws(xy_values1_b, ctrl_nodes_b);
                    if (ctrl_nodes_e <= ctrl_nodes_b)
                        break;

                    xy_values1_e = skip_ws(comma_ptr + 1);
                    if (*xy_values1_e == '{') {
                        xy_values1_e = inp_spawn_brace(xy_values1_e);
                    }
                    else {
                        xy_values1_e = skip_non_ws(xy_values1_e);
                    }
                    if (!xy_values1_e)
                        break;

                    xy_values2_b = skip_ws(xy_values1_e);

                    ok = TRUE;
                } while (0);

                if (!ok) {
                    fprintf(stderr, "ERROR: malformed line: %s\n", line);
                    controlled_exit(EXIT_FAILURE);
                }

                ref_str = copy_substring(line, ref_e);
                out_str = copy_substring(out_b, out_e);
                fcn_name = copy_substring(fcn_b, fcn_e);
                ctrl_nodes_str = copy_substring(ctrl_nodes_b, ctrl_nodes_e);

                keep = *xy_values1_e;
                *xy_values1_e = '\0';
                xy_count1 =
                        get_comma_separated_values(xy_values1, xy_values1_b);
                *xy_values1_e = keep;

                xy_count2 =
                        get_comma_separated_values(xy_values2, xy_values2_b);

                // place restrictions on only having 2 point values; this can
                // change later
                if (xy_count1 != 2 && xy_count2 != 2)
                    fprintf(stderr,
                            "ERROR: only expecting 2 pair values for "
                            "multi-input vcvs!\n");

                m_instance = tprintf("%s %%vd[ %s ] %%vd( %s ) %s", ref_str,
                        ctrl_nodes_str, out_str, ref_str);
                m_instance[0] = 'a';

                m_model = tprintf(".model %s multi_input_pwl ( x = [%s %s] y "
                                  "= [%s %s] model = \"%s\" )",
                        ref_str, xy_values1[0], xy_values2[0], xy_values1[1],
                        xy_values2[1], fcn_name);

                tfree(ref_str);
                tfree(out_str);
                tfree(fcn_name);
                tfree(ctrl_nodes_str);
                tfree(xy_values1[0]);
                tfree(xy_values1[1]);
                tfree(xy_values2[0]);
                tfree(xy_values2[1]);

                *c->line = '*';
                c = insert_new_line(c, m_instance, (*line_number)++, 0);
                c = insert_new_line(c, m_model, (*line_number)++, 0);
            }
        }
    }
}


/* If ngspice is started with option -a, then variable 'autorun'
 *   will be set and the following function scans the deck.
 * If 'run' is not found, a .control section will be added:
 *   .control
 *   run
 *   op              ; if .op is found
 *   write rawfile   ; if rawfile given
 *   .endc
 */
static void inp_add_control_section(struct card *deck, int *line_number)
{
    struct card *c, *prev_card = NULL;
    bool found_control = FALSE, found_run = FALSE;
    bool found_end = FALSE;
    char *op_line = NULL, rawfile[1000], *line;

    for (c = deck; c; c = c->nextcard) {

        if (*c->line == '*')
            continue;

        if (ciprefix(".op ", c->line)) {
            *c->line = '*';
            op_line = c->line + 1;
        }

        if (ciprefix(".end", c->line))
            found_end = TRUE;

        if (found_control && ciprefix("run", c->line))
            found_run = TRUE;

        if (ciprefix(".control", c->line))
            found_control = TRUE;

        if (ciprefix(".endc", c->line)) {
            found_control = FALSE;

            if (!found_run) {
                prev_card = insert_new_line(
                        prev_card, copy("run"), (*line_number)++, 0);
                found_run = TRUE;
            }

            if (cp_getvar("rawfile", CP_STRING, rawfile, sizeof(rawfile))) {
                line = tprintf("write %s", rawfile);
                prev_card =
                        insert_new_line(prev_card, line, (*line_number)++, 0);
            }
        }

        prev_card = c;
    }

    // check if need to add control section
    if (!found_run && found_end) {

        deck = insert_new_line(deck, copy(".control"), (*line_number)++, 0);

        deck = insert_new_line(deck, copy("run"), (*line_number)++, 0);

        if (op_line)
            deck = insert_new_line(deck, copy(op_line), (*line_number)++, 0);

        if (cp_getvar("rawfile", CP_STRING, rawfile, sizeof(rawfile))) {
            line = tprintf("write %s", rawfile);
            deck = insert_new_line(deck, line, (*line_number)++, 0);
        }

        deck = insert_new_line(deck, copy(".endc"), (*line_number)++, 0);
    }
}


/* overwrite shell-style end-of-line continuation '\\' with spaces,
 *   and return TRUE when found */
static bool chk_for_line_continuation(char *line)
{
    if (*line != '*' && *line != '$') {

        char *ptr = skip_back_ws(strchr(line, '\0'), line);

        if ((ptr - 2 >= line) && (ptr[-1] == '\\') && (ptr[-2] == '\\')) {
            ptr[-1] = ' ';
            ptr[-2] = ' ';
            return TRUE;
        }
    }

    return FALSE;
}


//
// change .macro --> .subckt
//        .eom   --> .ends
//        .subckt name 1 2 3 params: w=9u l=180n --> .subckt name 1 2 3 w=9u
//        l=180n .subckt name (1 2 3) --> .subckt name 1 2 3 x1 (1 2 3) --> x1
//        1 2 3 .param func1(x,y) = {x*y} --> .func func1(x,y) {x*y}

static void inp_fix_macro_param_func_paren_io(struct card *card)
{
    char *str_ptr, *new_str;

    for (; card; card = card->nextcard) {

        if (*card->line == '*')
            continue;

        if (ciprefix(".macro", card->line) || ciprefix(".eom", card->line)) {
            str_ptr = skip_non_ws(card->line);

            if (ciprefix(".macro", card->line)) {
                new_str = tprintf(".subckt%s", str_ptr);
            }
            else {
                new_str = tprintf(".ends%s", str_ptr);
            }

            tfree(card->line);
            card->line = new_str;
        }

        if (ciprefix(".subckt", card->line) || ciprefix("x", card->line)) {
            /* remove () */
            str_ptr = skip_non_ws(
                    card->line); // skip over .subckt, instance name
            str_ptr = skip_ws(str_ptr);
            if (ciprefix(".subckt", card->line)) {
                str_ptr = skip_non_ws(str_ptr); // skip over subckt name
                str_ptr = skip_ws(str_ptr);
            }
            if (*str_ptr == '(') {
                *str_ptr = ' ';
                while (*str_ptr != '\0') {
                    if (*str_ptr == ')') {
                        *str_ptr = ' ';
                        break;
                    }
                    str_ptr++;
                }
                card->line = inp_remove_ws(
                        card->line); /* remove the extra white spaces just
                                        introduced */
            }
        }

        if (ciprefix(".para", card->line)) {
            bool is_func = FALSE;
            str_ptr = skip_non_ws(card->line); // skip over .param
            str_ptr = skip_ws(str_ptr);
            while (!isspace_c(*str_ptr) && *str_ptr != '=') {
                if (*str_ptr == '(')
                    is_func = TRUE;
                str_ptr++;
            }

            if (is_func) {
                str_ptr = strchr(card->line, '=');
                if (str_ptr)
                    *str_ptr = ' ';
                str_ptr = card->line + 1;
                str_ptr[0] = 'f';
                str_ptr[1] = 'u';
                str_ptr[2] = 'n';
                str_ptr[3] = 'c';
                str_ptr[4] = ' ';
            }
        }
    }
}


static char *get_instance_subckt(char *line)
{
    char *end_ptr, *inst_name_ptr;
    char *equal_ptr = strchr(line, '=');

    // see if instance has parameters
    if (equal_ptr) {
        end_ptr = skip_back_ws(equal_ptr, line);
        end_ptr = skip_back_non_ws(end_ptr, line);
    }
    else {
        end_ptr = strchr(line, '\0');
    }

    end_ptr = skip_back_ws(end_ptr, line);

    inst_name_ptr = skip_back_non_ws(end_ptr, line);

    return copy_substring(inst_name_ptr, end_ptr);
}


static char *get_subckt_model_name(char *line)
{
    char *name, *end_ptr;

    name = skip_non_ws(line); // eat .subckt|.model
    name = skip_ws(name);

    end_ptr = skip_non_ws(name);

    return copy_substring(name, end_ptr);
}


static char *get_model_name(char *line, int num_terminals)
{
    char *beg_ptr, *end_ptr;
    int i = 0;

    beg_ptr = skip_non_ws(line); /* eat device name */
    beg_ptr = skip_ws(beg_ptr);

    for (i = 0; i < num_terminals; i++) { /* skip the terminals */
        beg_ptr = skip_non_ws(beg_ptr);
        beg_ptr = skip_ws(beg_ptr);
    }

    if (*line == 'r') /* special dealing for r models */
        if ((*beg_ptr == '+') || (*beg_ptr == '-') ||
                isdigit_c(*beg_ptr)) { /* looking for a value before model */
            beg_ptr = skip_non_ws(beg_ptr); /* skip the value */
            beg_ptr = skip_ws(beg_ptr);
        }

    end_ptr = skip_non_ws(beg_ptr);

    return copy_substring(beg_ptr, end_ptr);
}


static char *get_model_type(char *line)
{
    char *beg_ptr;

    if (!ciprefix(".model", line))
        return NULL;

    beg_ptr = skip_non_ws(line); /* eat .model */
    beg_ptr = skip_ws(beg_ptr);

    beg_ptr = skip_non_ws(beg_ptr); /* eat model name */
    beg_ptr = skip_ws(beg_ptr);

    return gettok_noparens(&beg_ptr);
}


static char *get_adevice_model_name(char *line)
{
    char *ptr_end, *ptr_beg;

    ptr_end = skip_back_ws(strchr(line, '\0'), line);
    ptr_beg = skip_back_non_ws(ptr_end, line);

    return copy_substring(ptr_beg, ptr_end);
}


/*
 *   To distinguish modelname tokens from other tokens
 *   by checking if token is not a valid ngspice number
 */
static int is_a_modelname(const char *s)
{
    char *st;
    double testval;
    /*token contains a '=' */
    if (strchr(s, '='))
        return FALSE;
    /* first character of model name is character from alphabet */
    if (isalpha_c(s[0]))
        return TRUE;
    /* first characters not allowed in model name (including '\0')*/
    if (strchr("{*^@\\\'", s[0]))
        return FALSE;
    /* not beeing a valid number */
    testval = strtod(s, &st);
    /* conversion failed, so no number */
    if (eq(s, st))
        return TRUE;
    /* test if we have a true number */
    else if (*st == '\0' || isspace(*st))
        return FALSE;
    else {
        /* look for the scale factor (alphabetic) and skip it.
         * INPevaluate will not do it because is does not swallow
         * the scale factor from the string.
         */
        switch (*st) {
            case 't':
            case 'T':
            case 'g':
            case 'G':
            case 'k':
            case 'K':
            case 'u':
            case 'U':
            case 'n':
            case 'N':
            case 'p':
            case 'P':
            case 'f':
            case 'F':
                st = st + 1;
                break;
            case 'm':
            case 'M':
                if (((st[1] == 'E') || (st[1] == 'e')) &&
                        ((st[2] == 'G') || (st[2] == 'g'))) {
                    st = st + 3; /* Meg */
                }
                else if (((st[1] == 'I') || (st[1] == 'i')) &&
                        ((st[2] == 'L') || (st[2] == 'l'))) {
                    st = st + 3; /* Mil */
                }
                else {
                    st = st + 1; /* m, milli */
                }
                break;
            default:
                break;
        }
        /* test if we have a true scale factor */
        if (*st == '\0' || isspace(*st))
            return FALSE;

        /* test if people use Ohms, F, H for RLC, like pF or uOhms */
        if (ciprefix("ohms", st))
            st = st + 4;
        else if (ciprefix("farad", st))
            st = st + 5;
        else if (ciprefix("henry", st))
            st = st + 5;
        else if ((*st == 'f') || (*st == 'h'))
            st = st + 1;

        if (*st == '\0' || isspace(*st))
            return FALSE;
    }

    /* token starts with non alphanum character */
    return TRUE;
}


struct nlist {
    char **names;
    int num_names;
    int size;
};


static const char *nlist_find(const struct nlist *nlist, const char *name)
{
    int i;
    for (i = 0; i < nlist->num_names; i++)
        if (strcmp(nlist->names[i], name) == 0)
            return nlist->names[i];
    return NULL;
}

#if 0
static const char *nlist_model_find(
        const struct nlist *nlist, const char *name)
{
    int i;
    for (i = 0; i < nlist->num_names; i++)
        if (model_name_match(nlist->names[i], name))
            return nlist->names[i];
    return NULL;
}
#endif

static void nlist_adjoin(struct nlist *nlist, char *name)
{
    if (nlist_find(nlist, name)) {
        tfree(name);
        return;
    }

    if (nlist->num_names >= nlist->size)
        nlist->names = TREALLOC(char *, nlist->names, nlist->size *= 2);

    nlist->names[nlist->num_names++] = name;
}


static struct nlist *nlist_allocate(int size)
{
    struct nlist *t = TMALLOC(struct nlist, 1);

    t->names = TMALLOC(char *, size);
    t->size = size;

    return t;
}


static void nlist_destroy(struct nlist *nlist)
{
    int i;
    for (i = 0; i < nlist->num_names; i++)
        tfree(nlist->names[i]);

    tfree(nlist->names);
    tfree(nlist);
}


static void get_subckts_for_subckt(struct card *start_card, char *subckt_name,
        struct nlist *used_subckts, struct nlist *used_models,
        bool has_models)
{
    struct card *card;
    int first_new_subckt = used_subckts->num_names;

    bool found_subckt = FALSE;
    int i, fence;

    for (card = start_card; card; card = card->nextcard) {

        char *line = card->line;

        /* no models embedded in these lines */
        if (strchr("*vibefghkt", *line))
            continue;

        if ((ciprefix(".ends", line) || ciprefix(".eom", line)) &&
                found_subckt)
            break;

        if (ciprefix(".subckt", line) || ciprefix(".macro", line)) {
            char *curr_subckt_name = get_subckt_model_name(line);

            if (strcmp(curr_subckt_name, subckt_name) == 0)
                found_subckt = TRUE;

            tfree(curr_subckt_name);
        }

        if (found_subckt) {
            if (*line == 'x') {
                char *inst_subckt_name = get_instance_subckt(line);
                nlist_adjoin(used_subckts, inst_subckt_name);
            }
            else if (*line == 'a') {
                char *model_name = get_adevice_model_name(line);
                nlist_adjoin(used_models, model_name);
            }
            else if (has_models) {
                int num_terminals = get_number_terminals(line);
                if (num_terminals != 0) {
                    char *model_name = get_model_name(line, num_terminals);
                    if (is_a_modelname(model_name))
                        nlist_adjoin(used_models, model_name);
                    else
                        tfree(model_name);
                }
            }
        }
    }

    // now make recursive call on instances just found above
    fence = used_subckts->num_names;
    for (i = first_new_subckt; i < fence; i++)
        get_subckts_for_subckt(start_card, used_subckts->names[i],
                used_subckts, used_models, has_models);
}


/*
  iterate through the deck and comment out unused subckts, models
  (don't want to waste time processing everything)
  also comment out .param lines with no parameters defined
*/

static void comment_out_unused_subckt_models(struct card *start_card)
{
    struct card *card;
    struct nlist *used_subckts, *used_models;
    int i = 0, fence;
    bool processing_subckt = FALSE, remove_subckt = FALSE, has_models = FALSE;
    int skip_control = 0, nested_subckt = 0;

    used_subckts = nlist_allocate(100);
    used_models = nlist_allocate(100);

    for (card = start_card; card; card = card->nextcard) {
        if (ciprefix(".model", card->line))
            has_models = TRUE;
        if (ciprefix(".cmodel", card->line))
            has_models = TRUE;
        if (ciprefix(".para", card->line) && !strchr(card->line, '='))
            *card->line = '*';
    }

    for (card = start_card; card; card = card->nextcard) {

        char *line = card->line;

        /* no models embedded in these lines */
        if (strchr("*vibefghkt", *line))
            continue;

        /* there is no .subckt, .model or .param inside .control ... .endc */
        if (ciprefix(".control", line)) {
            skip_control++;
            continue;
        }
        else if (ciprefix(".endc", line)) {
            skip_control--;
            continue;
        }
        else if (skip_control > 0) {
            continue;
        }

        if (ciprefix(".subckt", line) || ciprefix(".macro", line))
            processing_subckt = TRUE;
        if (ciprefix(".ends", line) || ciprefix(".eom", line))
            processing_subckt = FALSE;

        if (!processing_subckt) {
            if (*line == 'x') {
                char *subckt_name = get_instance_subckt(line);
                nlist_adjoin(used_subckts, subckt_name);
            }
            else if (*line == 'a') {
                char *model_name = get_adevice_model_name(line);
                nlist_adjoin(used_models, model_name);
            }
            else if (has_models) {
                /* This is a preliminary version, until we have found a
                   reliable method to detect the model name out of the input
                   line (Many options have to be taken into account.). */
                int num_terminals = get_number_terminals(line);
                if (num_terminals != 0) {
                    char *model_name = get_model_name(line, num_terminals);
                    if (is_a_modelname(model_name))
                        nlist_adjoin(used_models, model_name);
                    else
                        tfree(model_name);
                }
            } /* if (has_models)  */
        } /* if (!processing_subckt) */
    } /* for loop through all cards */

    fence = used_subckts->num_names;
    for (i = 0; i < fence; i++)
        get_subckts_for_subckt(start_card, used_subckts->names[i],
                used_subckts, used_models, has_models);

    /* comment out any unused subckts, currently only at top level */
    for (card = start_card; card; card = card->nextcard) {

        char *line = card->line;

        if (*line == '*')
            continue;

        if (ciprefix(".subckt", line) || ciprefix(".macro", line)) {
            char *subckt_name = get_subckt_model_name(line);
            /* check if unused, only at top level */
            if (nested_subckt++ == 0)
                remove_subckt = !nlist_find(used_subckts, subckt_name);
            tfree(subckt_name);
        }

        if (ciprefix(".ends", line) || ciprefix(".eom", line)) {
            if (remove_subckt)
                *line = '*';
            if (--nested_subckt == 0)
                remove_subckt = FALSE;
        }

        if (remove_subckt)
            *line = '*';
#if 0
        else if (has_models &&
                (ciprefix(".model", line) || ciprefix(".cmodel", line))) {
            char *model_type = get_model_type(line);
            char *model_name = get_subckt_model_name(line);

            /* keep R, L, C models because in addition to no. of terminals the
               value may be given, as in RE1 1 2 800 newres dtemp=5, so model
               name may be token no. 4 or 5, and, if 5, will not be detected
               by get_subckt_model_name()*/
            if (!cieq(model_type, "c") && !cieq(model_type, "l") &&
                    !cieq(model_type, "r") &&
                    !nlist_model_find(used_models, model_name)) {
                *line = '*';
            }

            tfree(model_type);
            tfree(model_name);
        }
#endif
    }

    nlist_destroy(used_subckts);
    nlist_destroy(used_models);
}


#if 0
// find closing paren
static char *
inp_search_closing_paren(char *s)
{
    int count = 0;
    // assert(*s == '(')
    while (*s) {
        if (*s == '(')
            count++;
        if (*s == ')')
            count--;
        if (count == 0)
            return s + 1;
        s++;
    }

    return NULL;
}
#endif


#if 0
/* search backwards for opening paren */
static char *
inp_search_opening_paren(char *s, char *start)
{
    int count = 0;
    // assert(*s == ')')
    while (s >= start) {
        if (*s == '(')
            count--;
        if (*s == ')')
            count++;
        if (count == 0)
            return s;
        s--;
    }

    return NULL;
}
#endif


/* search forward for closing brace */
static char *inp_spawn_brace(char *s)
{
    int count = 0;
    // assert(*s == '{')
    while (*s) {
        if (*s == '{')
            count++;
        if (*s == '}')
            count--;
        if (count == 0)
            return s + 1;
        s++;
    }

    return NULL;
}


/*-------------------------------------------------------------------------*
  removes  " " quotes, returns lower case letters,
  replaces non-printable characterss with '_', however if
  non-printable character is the only character in a line,
  replace it by '*'
  *-------------------------------------------------------------------------*/

void inp_casefix(char *string)
{
#ifdef HAVE_CTYPE_H
    /* single non-printable character */
    if(string && !isspace_c(*string) && !isprint_c(*string) &&
    (string[1] == '\0' || isspace_c(string[1]))) {
        *string = '*';
        return;
    }
    if (string)
        while (*string) {
#ifdef HAS_ASCII
            /* ((*string) & 0177): mask off all but the first seven bits,
             * 0177: octal */
            *string = (char) strip(*string);
#endif
            if (*string == '"') {
                *string++ = ' ';
                while (*string && *string != '"')
                    string++;
                if (*string == '\0')
                    continue; /* needed if string is "something ! */
                if (*string == '"')
                    *string = ' ';
            }
            if (!isspace_c(*string) && !isprint_c(*string))
                *string = '_';
            if (isupper_c(*string))
                *string = tolower_c(*string);
            string++;
        }
#endif
}


/* Strip all end-of-line comments from a deck
   For cf == TRUE (script files, command files like spinit, .spiceinit)
   and for .control sections only '$ ' is accepted as end-of-line comment,
   to avoid conflict with $variable definition, otherwise we accept '$'. */
static void inp_stripcomments_deck(struct card *c, bool cf)
{
    bool found_control = FALSE;
    for (; c; c = c->nextcard) {
        /* exclude lines between .control and .endc from removing white spaces
         */
        if (ciprefix(".control", c->line))
            found_control = TRUE;
        if (ciprefix(".endc", c->line))
            found_control = FALSE;
        inp_stripcomments_line(c->line, found_control | cf);
    }
}


/*
 * Support for end-of-line comments that begin with any of the following:
 *   ';'
 *   '$' (only outside of a .control section)
 *   '$ '
 *   '//' (like in c++ and as per the numparam code)
 * Any following text to the end of the line is ignored.
 * Note requirement for $ to be followed by a space, if we are inside of a
 * .control section or in a command file. This is to avoid conflict
 * with use of $ in front of a variable.
 * Comments on a continuation line (i.e. line begining with '+') are allowed
 * and are removed before lines are stitched.
 * Lines that contain only an end-of-line comment with or without leading
 white
 * space are also allowed.

 If there is only white space before the end-of-line comment the
 the whole line is converted to a normal comment line (i.e. one that
 begins with a '*').
 BUG: comment characters in side of string literals are not ignored
 ('$' outside of .control section is o.k. however).

 If the comaptibility mode is PS, LTPS or LTPSA, '$' is treated as a valid
 character, not as end-of-line comment delimiter, except for that it is
 located at the beginning of a line. If inside of a control section,
 still '$ ' is read a an end-of-line comment delimiter.*/
static void inp_stripcomments_line(char *s, bool cs)
{
    char c = ' '; /* anything other than a comment character */
    char *d = s;
    if (*s == '\0')
        return; /* empty line */
    if (*s == '*')
        return; /* line is already a comment */
    /* look for comments */
    while ((c = *d) != '\0') {
        d++;
        if (*d == ';') {
            break;
        }
        /* outside of .control section, and not in PS mode */
        else if (!cs && (c == '$') && inp_compat_mode != COMPATMODE_PS &&
                inp_compat_mode != COMPATMODE_LTPS &&
                inp_compat_mode != COMPATMODE_LTPSA) {
            /* The character before '&' has to be ',' or ' ' or tab.
               A valid numerical expression directly before '$' is not yet
               supported. */
            if ((d - 2 >= s) &&
                    ((d[-2] == ' ') || (d[-2] == ',') || (d[-2] == '\t'))) {
                d--;
                break;
            }
        }
        else if (cs && (c == '$') &&
                (*d == ' ')) { /* inside of .control section or command file
                                */
            d--; /* move d back to first comment character */
            break;
        }
        else if ((c == '/') && (*d == '/')) {
            d--; /* move d back to first comment character */
            break;
        }
    }
    /* d now points to the first comment character or the null at the string
     * end */

    /* check for special case of comment at start of line */
    if (d == s) {
        *s = '*'; /* turn into normal comment */
        return;
    }

    if (d > s) {
        d--;
        /* d now points to character just before comment */

        /* eat white space at new end of line */
        while (d >= s) {
            if ((*d != ' ') && (*d != '\t'))
                break;
            d--;
        }
        d++;
        /* d now points to the first white space character before the
           end-of-line or end-of-line comment, or it points to the first
           end-of-line comment character, or to the begining of the line */
    }

    /* Check for special case of comment at start of line
       with or without preceeding white space */
    if (d <= s) {
        *s = '*'; /* turn the whole line into normal comment */
        return;
    }

    *d = '\0'; /* terminate line in new location */
}


static void inp_change_quotes(char *s)
{
    bool first_quote = FALSE;

    for (; *s; s++)
        if (*s == '\'') {
            if (first_quote == FALSE) {
                *s = '{';
                first_quote = TRUE;
            }
            else {
                *s = '}';
                first_quote = FALSE;
            }
        }
}


static void add_name(struct names *p, char *name)
{
    if (p->num_names >= N_SUBCKT_W_PARAMS) {
        fprintf(stderr, "ERROR, N_SUBCKT_W_PARMS overflow\n");
        controlled_exit(EXIT_FAILURE);
    }

    p->names[p->num_names++] = name;
}


static char **find_name(struct names *p, char *name)
{
    int i;

    for (i = 0; i < p->num_names; i++)
        if (strcmp(p->names[i], name) == 0)
            return &p->names[i];

    return NULL;
}


static char *inp_fix_subckt(struct names *subckt_w_params, char *s)
{
    struct card *head, *first_param_card, *c;
    char *equal, *beg, *buffer, *ptr1, *ptr2, *new_str;

    equal = strchr(s, '=');
    if (equal && !strstr(s, "params:")) {
        /* get subckt name (ptr1 will point to name) */
        ptr1 = skip_non_ws(s);
        ptr1 = skip_ws(ptr1);
        for (ptr2 = ptr1; *ptr2 && !isspace_c(*ptr2) && !isquote(*ptr2);
                ptr2++)
            ;

        add_name(subckt_w_params, copy_substring(ptr1, ptr2));

        /* go to beginning of first parameter word  */
        /* s    will contain only subckt definition */
        /* beg  will point to start of param list   */
        beg = skip_back_ws(equal, s);
        beg = skip_back_non_ws(beg, s);
        beg[-1] = '\0'; /* fixme can be < s */

        head = insert_new_line(NULL, NULL, 0, 0);
        /* create list of parameters that need to get sorted */
        first_param_card = c = NULL;
        while ((ptr1 = strchr(beg, '=')) != NULL) {
            ptr2 = skip_ws(ptr1 + 1);
            ptr1 = skip_back_ws(ptr1, beg);
            ptr1 = skip_back_non_ws(ptr1, beg);
            /* ptr1 points to beginning of parameter */

            if (*ptr2 == '{')
                ptr2 = inp_spawn_brace(ptr2);
            else
                ptr2 = skip_non_ws(ptr2);

            if (!ptr2) {
                fprintf(stderr, "Error: Missing } in line %s\n", s);
                controlled_exit(EXIT_FAILURE);
            }

            beg = ptr2;

            c = insert_new_line(c, copy_substring(ptr1, ptr2), 0, 0);

            if (!first_param_card)
                first_param_card = c;
        }
        /* now sort parameters in order of dependencies */
        inp_sort_params(first_param_card, head, NULL, NULL);

        /* create new ordered parameter string for subckt call */
        new_str = NULL;
        for (c = head->nextcard; c; c = c->nextcard)
            if (new_str == NULL) {
                new_str = copy(c->line);
            }
            else {
                char *x = tprintf("%s %s", new_str, c->line);
                tfree(new_str);
                new_str = x;
            }

        line_free_x(head, TRUE);

        /* create buffer and insert params: */
        buffer = tprintf("%s params: %s", s, new_str);

        tfree(s);
        tfree(new_str);

        s = buffer;
    }

    return s;
}


/*
 * this function shall:
 *   reduce sequences of whitespace to one space
 *   and to drop even that if it seems to be at a `safe' place to do so
 * safe place means:
 *   before or behind a '='
 *   before or behind an operator within a {} expression
 *     whereby `operator' is classified by `is_arith_char()'
 * fixme:
 *   thats odd and very naive business
 */

static char *inp_remove_ws(char *s)
{
    char *x = s;
    char *d = s;

    int brace_level = 0;

    /* preserve at least one whitespace at beginning of line
     * fixme,
     *   is this really necessary ?
     *   or is this an artefact of original inp_remove_ws() implementation ?
     */
    if (isspace_c(*s))
        *d++ = *s++;

    while (*s != '\0') {
        if (*s == '{')
            brace_level++;
        if (*s == '}')
            brace_level--;

        if (isspace_c(*s)) {
            s = skip_ws(s);
            if (!(*s == '\0' || *s == '=' ||
                        ((brace_level > 0) &&
                                (is_arith_char(*s) || *s == ','))))
                *d++ = ' ';
            continue;
        }

        if (*s == '=' ||
                ((brace_level > 0) && (is_arith_char(*s) || *s == ','))) {
            *d++ = *s++;
            s = skip_ws(s);
            continue;
        }

        *d++ = *s++;
    }

    *d = '\0';

    if (d == s)
        return x;

    s = copy(x);
    tfree(x);

    return s;
}


/*
  change quotes from '' to {}
  .subckt name 1 2 3 params: l=1 w=2 --> .subckt name 1 2 3 l=1 w=2
  x1 1 2 3 params: l=1 w=2 --> x1 1 2 3 l=1 w=2
  modify .subckt lines by calling inp_fix_subckt()
  No changes to lines in .control section !
*/

static void inp_fix_for_numparam(
        struct names *subckt_w_params, struct card *c)
{
    bool found_control = FALSE;

    for (; c; c = c->nextcard) {

        if (*(c->line) == '*' || ciprefix(".lib", c->line))
            continue;

        /* exclude lines between .control and .endc from getting quotes
         * changed */
        if (ciprefix(".control", c->line))
            found_control = TRUE;
        if (ciprefix(".endc", c->line))
            found_control = FALSE;

        if (found_control)
            continue;

        inp_change_quotes(c->line);

        if ((inp_compat_mode == COMPATMODE_ALL) ||
                (inp_compat_mode == COMPATMODE_PS) ||
                (inp_compat_mode == COMPATMODE_PSA) ||
                (inp_compat_mode == COMPATMODE_LTPS) ||
                (inp_compat_mode == COMPATMODE_LTPSA))
            if (ciprefix(".subckt", c->line) || ciprefix("x", c->line)) {
                /* remove params: */
                char *str_ptr = strstr(c->line, "params:");
                if (str_ptr)
                    memcpy(str_ptr, "       ", 7);
            }

        if (ciprefix(".subckt", c->line))
            c->line = inp_fix_subckt(subckt_w_params, c->line);
    }
}


static void inp_remove_excess_ws(struct card *c)
{
    bool found_control = FALSE;

    for (; c; c = c->nextcard) {

        if (*c->line == '*')
            continue;

        /* exclude echo lines between .control and .endc from removing white
         * spaces */
        if (ciprefix(".control", c->line))
            found_control = TRUE;
        if (ciprefix(".endc", c->line))
            found_control = FALSE;

        if (found_control && ciprefix("echo", c->line))
            continue;

        c->line = inp_remove_ws(c->line); /* freed in fcn */
    }
}


static struct card *expand_section_ref(struct card *c, char *dir_name)
{
    char *line = c->line;

    char *s, *s_e, *y;

    s = skip_non_ws(line);
    while (isspace_c(*s) || isquote(*s))
        s++;
    for (s_e = s; *s_e && !isspace_c(*s_e) && !isquote(*s_e); s_e++)
        ;
    y = s_e;
    while (isspace_c(*y) || isquote(*y))
        y++;

    if (*y) {
        /* library section reference: `.lib <library-file> <section-name>' */

        struct card *section_def;
        char keep_char1, keep_char2;
        char *y_e;
        struct library *lib;

        for (y_e = y; *y_e && !isspace_c(*y_e) && !isquote(*y_e); y_e++)
            ;
        keep_char1 = *s_e;
        keep_char2 = *y_e;
        *s_e = '\0';
        *y_e = '\0';

        lib = read_a_lib(s, dir_name);

        if (!lib) {
            fprintf(stderr, "ERROR, library file %s not found\n", s);
            controlled_exit(EXIT_FAILURE);
        }

        section_def = find_section_definition(lib->deck, y);

        if (!section_def) {
            fprintf(stderr,
                    "ERROR, library file %s, section definition %s not "
                    "found\n",
                    s, y);
            controlled_exit(EXIT_FAILURE);
        }

        /* recursively expand the refered section itself */
        {
            struct card *t = section_def;
            for (; t; t = t->nextcard) {
                if (ciprefix(".endl", t->line))
                    break;
                if (ciprefix(".lib", t->line))
                    t = expand_section_ref(t, lib->habitat);
            }
            if (!t) {
                fprintf(stderr, "ERROR, .endl not found\n");
                controlled_exit(EXIT_FAILURE);
            }
        }

        /* insert the library section definition into `c' */
        {
            struct card *t = section_def;
            for (; t; t = t->nextcard) {
                c = insert_new_line(
                        c, copy(t->line), t->linenum, t->linenum_orig);
                if (t == section_def) {
                    c->line[0] = '*';
                    c->line[1] = '<';
                }
                if (ciprefix(".endl", t->line)) {
                    c->line[0] = '*';
                    c->line[1] = '>';
                    break;
                }
            }
            if (!t) {
                fprintf(stderr, "ERROR, .endl not found\n");
                controlled_exit(EXIT_FAILURE);
            }
        }

        *line = '*'; /* comment out .lib line */
        *s_e = keep_char1;
        *y_e = keep_char2;
    }

    return c;
}


/*
 * recursively expand library section references,
 * either
 *    every library section reference (when the given section_name_ === NULL)
 * or
 *    just those references occuring in the given library section definition
 */

static void expand_section_references(struct card *c, char *dir_name)
{
    for (; c; c = c->nextcard)
        if (ciprefix(".lib", c->line))
            c = expand_section_ref(c, dir_name);
}


static char *inp_get_subckt_name(char *s)
{
    char *subckt_name, *end_ptr = strchr(s, '=');

    if (end_ptr) {
        end_ptr = skip_back_ws(end_ptr, s);
        end_ptr = skip_back_non_ws(end_ptr, s);
    }
    else {
        end_ptr = strchr(s, '\0');
    }

    end_ptr = skip_back_ws(end_ptr, s);
    subckt_name = skip_back_non_ws(end_ptr, s);

    return copy_substring(subckt_name, end_ptr);
}


static int inp_get_params(
        char *line, char *param_names[], char *param_values[])
{
    char *equal_ptr;
    char *end, *name, *value;
    int num_params = 0;
    char keep;

    while ((equal_ptr = find_assignment(line)) != NULL) {

        /* get parameter name */
        end = skip_back_ws(equal_ptr, line);
        name = skip_back_non_ws(end, line);

        if (num_params == NPARAMS) {
            fprintf(stderr, "Error: to many params in a line, max is %d\n",
                    NPARAMS);
            controlled_exit(EXIT_FAILURE);
        }

        param_names[num_params++] = copy_substring(name, end);

        /* get parameter value */
        value = skip_ws(equal_ptr + 1);

        if (*value == '{')
            end = inp_spawn_brace(value);
        else
            end = skip_non_ws(value);

        if (!end) {
            fprintf(stderr, "Error: Missing } in %s\n", line);
            controlled_exit(EXIT_FAILURE);
        }

        keep = *end;
        *end = '\0';

        if (*value == '{' || isdigit_c(*value) ||
                (*value == '.' && isdigit_c(value[1]))) {
            value = copy(value);
        }
        else {
            value = tprintf("{%s}", value);
        }

        param_values[num_params - 1] = value;
        *end = keep;

        line = end;
    }

    return num_params;
}


static char *inp_fix_inst_line(char *inst_line, int num_subckt_params,
        char *subckt_param_names[], char *subckt_param_values[],
        int num_inst_params, char *inst_param_names[],
        char *inst_param_values[])
{
    char *end, *inst_name, *inst_name_end;
    char *curr_line = inst_line, *new_line = NULL;
    int i, j;

    inst_name_end = skip_non_ws(inst_line);
    inst_name = copy_substring(inst_line, inst_name_end);

    end = strchr(inst_line, '=');
    if (end) {
        end = skip_back_ws(end, inst_line);
        end = skip_back_non_ws(end, inst_line);
        end[-1] = '\0'; /* fixme can be < inst_line */
    }

    for (i = 0; i < num_subckt_params; i++)
        for (j = 0; j < num_inst_params; j++)
            if (strcmp(subckt_param_names[i], inst_param_names[j]) == 0) {
                tfree(subckt_param_values[i]);
                subckt_param_values[i] = copy(inst_param_values[j]);
            }

    for (i = 0; i < num_subckt_params; i++) {
        new_line = tprintf("%s %s", curr_line, subckt_param_values[i]);

        tfree(curr_line);
        tfree(subckt_param_names[i]);
        tfree(subckt_param_values[i]);

        curr_line = new_line;
    }

    for (i = 0; i < num_inst_params; i++) {
        tfree(inst_param_names[i]);
        tfree(inst_param_values[i]);
    }

    tfree(inst_name);

    return curr_line;
}


/* If multiplier parameter 'm' is found on a X line, flag is set
   to TRUE.
   Function is called from inp_fix_inst_calls_for_numparam() */

static bool found_mult_param(int num_params, char *param_names[])
{
    int i;

    for (i = 0; i < num_params; i++)
        if (strcmp(param_names[i], "m") == 0)
            return TRUE;

    return FALSE;
}


/* If a subcircuit invocation (X-line) is found, which contains the
   multiplier parameter 'm', m is added to all lines inside
   the corresponding subcircuit except of some excluded in the code below
   Function is called from inp_fix_inst_calls_for_numparam() */

static int inp_fix_subckt_multiplier(struct names *subckt_w_params,
        struct card *subckt_card, int num_subckt_params,
        char *subckt_param_names[], char *subckt_param_values[])
{
    struct card *card;
    char *new_str;

    subckt_param_names[num_subckt_params] = copy("m");
    subckt_param_values[num_subckt_params] = copy("1");
    num_subckt_params++;

    if (!strstr(subckt_card->line, "params:")) {
        new_str = tprintf("%s params: m=1", subckt_card->line);
        add_name(subckt_w_params, get_subckt_model_name(subckt_card->line));
    }
    else {
        new_str = tprintf("%s m=1", subckt_card->line);
    }

    tfree(subckt_card->line);
    subckt_card->line = new_str;

    for (card = subckt_card->nextcard; card && !ciprefix(".ends", card->line);
            card = card->nextcard) {
        char *curr_line = card->line;
        /* no 'm' for comment line, B, V, E, H and some others that are not
         * using 'm' in their model description */
        if (strchr("*bvehaknopstuwy", curr_line[0]))
            continue;
        /* no 'm' for model cards */
        if (ciprefix(".model", curr_line))
            continue;
        new_str = tprintf("%s m={m}", curr_line);

        tfree(card->line);
        card->line = new_str;
    }

    return num_subckt_params;
}


static void inp_fix_inst_calls_for_numparam(
        struct names *subckt_w_params, struct card *deck)
{
    struct card *c;
    char *subckt_param_names[NPARAMS];
    char *subckt_param_values[NPARAMS];
    char *inst_param_names[NPARAMS];
    char *inst_param_values[NPARAMS];
    int i;

    // first iterate through instances and find occurences where 'm'
    // multiplier needs to be added to the subcircuit -- subsequent instances
    // will then need this parameter as well
    for (c = deck; c; c = c->nextcard) {
        char *inst_line = c->line;

        if (*inst_line == '*')
            continue;

        if (ciprefix("x", inst_line)) {
            int num_inst_params = inp_get_params(
                    inst_line, inst_param_names, inst_param_values);
            char *subckt_name = inp_get_subckt_name(inst_line);

            if (found_mult_param(num_inst_params, inst_param_names)) {
                struct card_assoc *a = find_subckt(c->level, subckt_name);
                if (a)
                {
                    int num_subckt_params = inp_get_params(a->line->line,
                            subckt_param_names, subckt_param_values);

                    if (!found_mult_param(
                                num_subckt_params, subckt_param_names))
                        inp_fix_subckt_multiplier(subckt_w_params, a->line,
                                num_subckt_params, subckt_param_names,
                                subckt_param_values);

                    for (i = 0; i < num_subckt_params; i++) {
                        tfree(subckt_param_names[i]);
                        tfree(subckt_param_values[i]);
                    }
                }
            }

            tfree(subckt_name);
            for (i = 0; i < num_inst_params; i++) {
                tfree(inst_param_names[i]);
                tfree(inst_param_values[i]);
            }
        }
    }

    for (c = deck; c; c = c->nextcard) {
        char *inst_line = c->line;

        if (*inst_line == '*')
            continue;

        if (ciprefix("x", inst_line)) {

            char *subckt_name = inp_get_subckt_name(inst_line);

            if (find_name(subckt_w_params, subckt_name)) {
                struct card *d;

                d = find_subckt(c->level, subckt_name)->line;
                {
                    char *subckt_line = d->line;
                    subckt_line = skip_non_ws(subckt_line);
                    subckt_line = skip_ws(subckt_line);

                    int num_subckt_params = inp_get_params(subckt_line,
                            subckt_param_names, subckt_param_values);
                    int num_inst_params = inp_get_params(
                            inst_line, inst_param_names, inst_param_values);

                    c->line = inp_fix_inst_line(inst_line, num_subckt_params,
                            subckt_param_names, subckt_param_values,
                            num_inst_params, inst_param_names,
                            inst_param_values);
                    for (i = 0; i < num_subckt_params; i++) {
                        tfree(subckt_param_names[i]);
                        tfree(subckt_param_values[i]);
                    }

                    for (i = 0; i < num_inst_params; i++) {
                        tfree(inst_param_names[i]);
                        tfree(inst_param_values[i]);
                    }
                }
            }

            tfree(subckt_name);
        }
    }
}


static struct function *new_function(struct function_env *env, char *name)
{
    struct function *f = TMALLOC(struct function, 1);

    f->name = name;
    f->num_parameters = 0;

    f->next = env->functions;
    env->functions = f;

    return f;
}


static struct function *find_function(struct function_env *env, char *name)
{
    struct function *f;

    for (; env; env = env->up)
        for (f = env->functions; f; f = f->next)
            if (strcmp(f->name, name) == 0)
                return f;

    return NULL;
}


static void free_function(struct function *fcn)
{
    int i;

    tfree(fcn->name);
    tfree(fcn->body);
    tfree(fcn->accept);

    for (i = 0; i < fcn->num_parameters; i++)
        tfree(fcn->params[i]);
}


static void new_function_parameter(struct function *fcn, char *parameter)
{
    if (fcn->num_parameters >= N_PARAMS) {
        fprintf(stderr, "ERROR, N_PARAMS overflow\n");
        controlled_exit(EXIT_FAILURE);
    }

    fcn->params[fcn->num_parameters++] = parameter;
}


static bool inp_strip_braces(char *s)
{
    int nesting = 0;
    char *d = s;

    for (; *s; s++)
        if (*s == '{') {
            nesting++;
        }
        else if (*s == '}') {
            if (--nesting < 0)
                return FALSE;
        }
        else if (!isspace_c(*s)) {
            *d++ = *s;
        }

    *d++ = '\0';

    return TRUE;
}


static void inp_get_func_from_line(struct function_env *env, char *line)
{
    char *end, *orig_line = line;
    struct function *function;

    /* skip `.func' */
    line = skip_non_ws(line);
    line = skip_ws(line);

    /* get function name */
    end = line;
    while (*end && !isspace_c(*end) && *end != '(')
        end++;

    function = new_function(env, copy_substring(line, end));

    end = skip_ws(end);

    if (*end != '(')
        goto Lerror;

    end = skip_ws(end + 1);

    /* get function parameters */
    for (;;) {
        char *beg = end;
        while (*end && !isspace_c(*end) && *end != ',' && *end != ')')
            end++;
        if (end == beg)
            break;
        new_function_parameter(function, copy_substring(beg, end));
        end = skip_ws(end);
        if (*end != ',')
            break;
        end = skip_ws(end + 1);
        if (*end == ')')
            goto Lerror;
    }

    if (*end != ')')
        goto Lerror;

    end = skip_ws(end + 1);

    // skip an unwanted and non advertised optional '='
    if (*end == '=')
        end = skip_ws(end + 1);

    function->body = copy(end);

    if (inp_strip_braces(function->body)) {
        int i;

        char *accept = TMALLOC(char, function->num_parameters + 1);
        for (i = 0; i < function->num_parameters; i++)
            accept[i] = function->params[i][0];
        accept[i] = '\0';

        function->accept = accept;
        return;
    }

    tfree(function->body);

Lerror:
    // fixme, free()
    fprintf(stderr, "ERROR: failed to parse .func in: %s\n", orig_line);
    controlled_exit(EXIT_FAILURE);
}


/*
 * grab functions at the current .subckt nesting level
 */

static void inp_grab_func(struct function_env *env, struct card *c)
{
    int nesting = 0;

    for (; c; c = c->nextcard) {

        if (*c->line == '*')
            continue;

        if (ciprefix(".subckt", c->line))
            nesting++;
        if (ciprefix(".ends", c->line))
            nesting--;

        if (nesting < 0)
            break;

        if (nesting > 0)
            continue;

        if (ciprefix(".func", c->line)) {
            inp_get_func_from_line(env, c->line);
            *c->line = '*';
        }
    }
}


static char *search_func_arg(
        char *str, struct function *fcn, int *which, char *str_begin)
{
    for (; (str = strpbrk(str, fcn->accept)) != NULL; str++) {
        char before;

        if (str > str_begin)
            before = str[-1];
        else
            before = '\0';

        if (is_arith_char(before) || isspace_c(before) ||
                strchr(",=", before)) {
            int i;
            for (i = 0; i < fcn->num_parameters; i++) {
                size_t len = strlen(fcn->params[i]);
                if (strncmp(str, fcn->params[i], len) == 0) {
                    char after = str[len];
                    if (is_arith_char(after) || isspace_c(after) ||
                            strchr(",=", after)) {
                        *which = i;
                        return str;
                    }
                }
            }
        }
    }

    return NULL;
}


static char *inp_do_macro_param_replace(struct function *fcn, char *params[])
{
    char *str = copy(fcn->body);
    int i;

    char *collect_ptr = NULL;
    char *arg_ptr = str;
    char *rest = str;

    while ((arg_ptr = search_func_arg(arg_ptr, fcn, &i, str)) != NULL) {
        char *p;
        int is_vi = 0;

        /* exclude v(nn, parameter), v(parameter, nn), v(parameter),
           and i(parameter) if here 'parameter' is also a node name */

        /* go backwards from 'parameter' and find '(' */
        for (p = arg_ptr; --p > str;)
            if (*p == '(' || *p == ')') {
                if ((*p == '(') && strchr("vi", p[-1]) &&
                        (p - 2 < str || is_arith_char(p[-2]) ||
                                isspace_c(p[-2]) || strchr(",=", p[-2])))
                    is_vi = 1;
                break;
            }

        /* if we have a true v( or i( */
        if (is_vi) {
            /* go forward and find closing ')' */
            for (p = arg_ptr + 1; *p; p++)
                if (*p == '(' || *p == ')')
                    break;
            /* We have a true v(...) or i(...),
               so skip it, and continue searching for new 'parameter' */
            if (*p == ')') {
                arg_ptr = p;
                continue;
            }
        }

        {
            size_t collect_ptr_len = collect_ptr ? strlen(collect_ptr) : 0;
            size_t len = strlen(rest) + strlen(params[i]) + 1;
            int prefix_len = (int) (arg_ptr - rest);
            if (str_has_arith_char(params[i])) {
                collect_ptr = TREALLOC(
                        char, collect_ptr, collect_ptr_len + len + 2);
                sprintf(collect_ptr + collect_ptr_len, "%.*s(%s)", prefix_len,
                        rest, params[i]);
            }
            else {
                collect_ptr =
                        TREALLOC(char, collect_ptr, collect_ptr_len + len);
                sprintf(collect_ptr + collect_ptr_len, "%.*s%s", prefix_len,
                        rest, params[i]);
            }
        }

        arg_ptr += strlen(fcn->params[i]);
        rest = arg_ptr;
    }

    if (collect_ptr) {
        char *new_str = tprintf("%s%s", collect_ptr, rest);
        tfree(collect_ptr);
        tfree(str);
        str = new_str;
    }

    return str;
}


static char *inp_expand_macro_in_str(struct function_env *env, char *str)
{
    struct function *function;
    char *open_paren_ptr, *close_paren_ptr, *fcn_name, *params[FCN_PARAMS];
    char *curr_ptr, *macro_str, *curr_str = NULL;
    int num_params, i;
    char *orig_ptr = str, *search_ptr = str, *orig_str = copy(str);
    char keep;

    // printf("%s: enter(\"%s\")\n", __FUNCTION__, str);
    while ((open_paren_ptr = strchr(search_ptr, '(')) != NULL) {

        fcn_name = open_paren_ptr;
        while (--fcn_name >= search_ptr)
            /* function name consists of numbers, letters and special
             * characters (VALIDCHARS) */
            if (!isalnum_c(*fcn_name) && !strchr(VALIDCHARS, *fcn_name))
                break;
        fcn_name++;

        search_ptr = open_paren_ptr + 1;
        if (open_paren_ptr == fcn_name)
            continue;

        *open_paren_ptr = '\0';

        function = find_function(env, fcn_name);

        *open_paren_ptr = '(';

        if (!function)
            continue;

        /* find the closing paren */
        {
            int num_parens = 1;
            char *c = open_paren_ptr + 1;

            for (; *c; c++) {
                if (*c == '(')
                    num_parens++;
                if (*c == ')' && --num_parens == 0)
                    break;
            }

            if (num_parens) {
                fprintf(stderr,
                        "ERROR: did not find closing parenthesis for "
                        "function call in str: %s\n",
                        orig_str);
                controlled_exit(EXIT_FAILURE);
            }

            close_paren_ptr = c;
        }

        /*
         * if (ciprefix("v(", curr_ptr)) {
         *     // look for any commas and change to ' '
         *     char *str_ptr = curr_ptr;
         *     while (*str_ptr != '\0' && *str_ptr != ')') {
         *         if (*str_ptr == ',' || *str_ptr == '(')
         *             *str_ptr = ' '; str_ptr++; }
         *     if (*str_ptr == ')')
         *         *str_ptr = ' ';
         * }
         */

        /* get the parameters */
        curr_ptr = open_paren_ptr + 1;

        for (num_params = 0; curr_ptr < close_paren_ptr; curr_ptr++) {
            char *beg_parameter;
            int num_parens;
            if (isspace_c(*curr_ptr))
                continue;
            beg_parameter = curr_ptr;
            num_parens = 0;
            for (; curr_ptr < close_paren_ptr; curr_ptr++) {
                if (*curr_ptr == '(')
                    num_parens++;
                if (*curr_ptr == ')')
                    num_parens--;
                if (*curr_ptr == ',' && num_parens == 0)
                    break;
            }
            if (num_params == FCN_PARAMS) {
                fprintf(stderr, "Error: Too many params in fcn, max is %d\n",
                        FCN_PARAMS);
                controlled_exit(EXIT_FAILURE);
            }
            params[num_params++] = inp_expand_macro_in_str(
                    env, copy_substring(beg_parameter, curr_ptr));
        }

        if (function->num_parameters != num_params) {
            fprintf(stderr,
                    "ERROR: parameter mismatch for function call in str: "
                    "%s\n",
                    orig_str);
            controlled_exit(EXIT_FAILURE);
        }

        macro_str = inp_do_macro_param_replace(function, params);
        macro_str = inp_expand_macro_in_str(env, macro_str);
        keep = *fcn_name;
        *fcn_name = '\0';
        {
            size_t curr_str_len = curr_str ? strlen(curr_str) : 0;
            size_t len = strlen(str) + strlen(macro_str) + 3;
            curr_str = TREALLOC(char, curr_str, curr_str_len + len);
            sprintf(curr_str + curr_str_len, "%s(%s)", str, macro_str);
        }
        *fcn_name = keep;
        tfree(macro_str);

        search_ptr = str = close_paren_ptr + 1;

        for (i = 0; i < num_params; i++)
            tfree(params[i]);
    }

    if (curr_str == NULL) {
        curr_str = orig_ptr;
    }
    else {
        if (str != NULL) {
            size_t curr_str_len = strlen(curr_str);
            size_t len = strlen(str) + 1;
            curr_str = TREALLOC(char, curr_str, curr_str_len + len);
            sprintf(curr_str + curr_str_len, "%s", str);
        }
        tfree(orig_ptr);
    }

    tfree(orig_str);
    // printf("%s: --> \"%s\"\n", __FUNCTION__, curr_str);

    return curr_str;
}


static void inp_expand_macros_in_func(struct function_env *env)
{
    struct function *f;

    for (f = env->functions; f; f = f->next)
        f->body = inp_expand_macro_in_str(env, f->body);
}


static struct function_env *new_function_env(struct function_env *up)
{
    struct function_env *env = TMALLOC(struct function_env, 1);

    env->up = up;
    env->functions = NULL;

    return env;
}


static struct function_env *delete_function_env(struct function_env *env)
{
    struct function_env *up = env->up;
    struct function *f;

    for (f = env->functions; f;) {
        struct function *here = f;
        f = f->next;
        free_function(here);
        tfree(here);
    }

    tfree(env);

    return up;
}


static struct card *inp_expand_macros_in_deck(
        struct function_env *env, struct card *c)
{
    env = new_function_env(env);

    inp_grab_func(env, c);

    inp_expand_macros_in_func(env);

    for (; c; c = c->nextcard) {

        if (*c->line == '*')
            continue;

        if (ciprefix(".subckt", c->line)) {
            struct card *subckt = c;
            c = inp_expand_macros_in_deck(env, c->nextcard);
            if (c)
                continue;

            fprintf(stderr, "Error: line %d, missing .ends\n  %s\n",
                    subckt->linenum_orig, subckt->line);
            controlled_exit(EXIT_BAD);
        }

        if (ciprefix(".ends", c->line))
            break;

        c->line = inp_expand_macro_in_str(env, c->line);
    }

    env = delete_function_env(env);

    return c;
}


/* Put {} around tokens for handling in numparam.
   Searches for the next '=' in the line to become active.
   Several exceptions (eg. no 'set' or 'b' lines, no .cmodel lines,
   no lines between .control and .endc, no .option lines).
   Special handling of vectors with [] and complex values with < >

   h_vogt 20 April 2008
   * For xspice and num_pram compatibility .cmodel added
   * .cmodel will be replaced by .model in inp_fix_param_values()
   * and then the entire line is skipped (will not be changed by this
   function).
   * Usage of numparam requires {} around the parameters in the .cmodel line.
   * May be obsolete?
   */

static void inp_fix_param_values(struct card *c)
{
    char *beg_of_str, *end_of_str, *old_str, *equal_ptr, *new_str;
    char *vec_str, *tmp_str, *natok, *buffer, *newvec, *whereisgt;
    bool control_section = FALSE;
    wordlist *nwl;
    int parens;

    for (; c; c = c->nextcard) {
        char *line = c->line;

        if (*line == '*' || (ciprefix(".para", line) && strchr(line, '{')))
            continue;

        if (ciprefix(".control", line)) {
            control_section = TRUE;
            continue;
        }

        if (ciprefix(".endc", line)) {
            control_section = FALSE;
            continue;
        }

        /* no handling of params in "option" lines */
        if (control_section || ciprefix(".option", line))
            continue;

        /* no handling of params in "set" lines */
        if (ciprefix("set", line))
            continue;

        /* no handling of params in B source lines */
        if (*line == 'b')
            continue;

        /* for xspice .cmodel: replace .cmodel with .model and skip entire
         * line) */
        if (ciprefix(".cmodel", line)) {
            *(++line) = 'm';
            *(++line) = 'o';
            *(++line) = 'd';
            *(++line) = 'e';
            *(++line) = 'l';
            *(++line) = ' ';
            continue;
        }

        /* exclude CIDER models */
        if (ciprefix(".model", line) &&
                (strstr(line, "numos") || strstr(line, "numd") ||
                        strstr(line, "nbjt") || strstr(line, "nbjt2") ||
                        strstr(line, "numd2"))) {
            continue;
        }

        /* exclude CIDER devices with ic.file parameter */
        if (strstr(line, "ic.file"))
            continue;

        while ((equal_ptr = find_assignment(line)) != NULL) {

            // special case: .MEASURE {DC|AC|TRAN} result FIND out_variable
            // WHEN out_variable2=out_variable3 no braces around
            // out_variable3. out_variable3 may be v(...) or i(...)
            if (ciprefix(".meas", line))
                if (((equal_ptr[1] == 'v') || (equal_ptr[1] == 'i')) &&
                        (equal_ptr[2] == '(')) {
                    // find closing ')' and skip token v(...) or i(...)
                    while (*equal_ptr != ')' && equal_ptr[1] != '\0')
                        equal_ptr++;
                    line = equal_ptr + 1;
                    continue;
                }

            beg_of_str = skip_ws(equal_ptr + 1);
            /* all cases where no {} have to be put around selected token */
            if (isdigit_c(*beg_of_str) || *beg_of_str == '{' ||
                    *beg_of_str == '.' || *beg_of_str == '"' ||
                    ((*beg_of_str == '-' || *beg_of_str == '+') &&
                            isdigit_c(beg_of_str[1])) ||
                    ((*beg_of_str == '-' || *beg_of_str == '+') &&
                            beg_of_str[1] == '.' &&
                            isdigit_c(beg_of_str[2])) ||
                    ciprefix("true", beg_of_str) ||
                    ciprefix("false", beg_of_str)) {
                line = equal_ptr + 1;
            }
            else if (*beg_of_str == '[') {
                /* A vector following the '=' token: code to put curly
                   brackets around all params
                   inside a pair of square brackets */
                end_of_str = beg_of_str;
                while (*end_of_str != ']')
                    end_of_str++;
                /* string xx yyy from vector [xx yyy] */
                tmp_str = vec_str =
                        copy_substring(beg_of_str + 1, end_of_str);

                /* work on vector elements inside [] */
                nwl = NULL;
                for (;;) {
                    natok = gettok(&vec_str);
                    if (!natok)
                        break;

                    buffer = TMALLOC(char, strlen(natok) + 4);
                    if (isdigit_c(*natok) || *natok == '{' || *natok == '.' ||
                            *natok == '"' ||
                            (*natok == '-' && isdigit_c(natok[1])) ||
                            ciprefix("true", natok) ||
                            ciprefix("false", natok) || eq(natok, "<") ||
                            eq(natok, ">")) {
                        (void) sprintf(buffer, "%s", natok);
                        /* A complex value found inside a vector [< x1 y1> <x2
                         * y2>] */
                        /* < xx and yy > have been dealt with before */
                        /* <xx */
                    }
                    else if (*natok == '<') {
                        if (isdigit_c(natok[1]) ||
                                (natok[1] == '-' && isdigit_c(natok[2]))) {
                            (void) sprintf(buffer, "%s", natok);
                        }
                        else {
                            *natok = '{';
                            (void) sprintf(buffer, "<%s}", natok);
                        }
                        /* yy> */
                    }
                    else if (strchr(natok, '>')) {
                        if (isdigit_c(*natok) ||
                                (*natok == '-' && isdigit_c(natok[1]))) {
                            (void) sprintf(buffer, "%s", natok);
                        }
                        else {
                            whereisgt = strchr(natok, '>');
                            *whereisgt = '}';
                            (void) sprintf(buffer, "{%s>", natok);
                        }
                        /* all other tokens */
                    }
                    else {
                        (void) sprintf(buffer, "{%s}", natok);
                    }
                    tfree(natok);
                    nwl = wl_cons(copy(buffer), nwl);
                    tfree(buffer);
                }
                tfree(tmp_str);
                nwl = wl_reverse(nwl);
                /* new vector elements */
                newvec = wl_flatten(nwl);
                wl_free(nwl);
                /* insert new vector into actual line */
                *equal_ptr = '\0';
                new_str = tprintf(
                        "%s=[%s] %s", c->line, newvec, end_of_str + 1);
                tfree(newvec);

                old_str = c->line;
                c->line = new_str;
                line = new_str + strlen(old_str) + 1;
                tfree(old_str);
            }
            else if (*beg_of_str == '<') {
                /* A complex value following the '=' token: code to put curly
                   brackets around all params inside a pair < > */
                end_of_str = beg_of_str;
                while (*end_of_str != '>')
                    end_of_str++;
                /* string xx yyy from vector [xx yyy] */
                vec_str = copy_substring(beg_of_str + 1, end_of_str);

                /* work on tokens inside <> */
                nwl = NULL;
                for (;;) {
                    natok = gettok(&vec_str);
                    if (!natok)
                        break;

                    buffer = TMALLOC(char, strlen(natok) + 4);
                    if (isdigit_c(*natok) || *natok == '{' || *natok == '.' ||
                            *natok == '"' ||
                            (*natok == '-' && isdigit_c(natok[1])) ||
                            ciprefix("true", natok) ||
                            ciprefix("false", natok)) {
                        (void) sprintf(buffer, "%s", natok);
                    }
                    else {
                        (void) sprintf(buffer, "{%s}", natok);
                    }
                    tfree(natok);
                    nwl = wl_cons(copy(buffer), nwl);
                    tfree(buffer);
                }
                nwl = wl_reverse(nwl);
                /* new elements of complex variable */
                newvec = wl_flatten(nwl);
                wl_free(nwl);
                /* insert new complex value into actual line */
                *equal_ptr = '\0';
                new_str = tprintf(
                        "%s=<%s> %s", c->line, newvec, end_of_str + 1);
                tfree(newvec);

                old_str = c->line;
                c->line = new_str;
                line = new_str + strlen(old_str) + 1;
                tfree(old_str);
            }
            else {
                /* put {} around token to be accepted as numparam */
                end_of_str = beg_of_str;
                parens = 0;
                while (*end_of_str != '\0' &&
                        (!isspace_c(*end_of_str) || (parens > 0))) {
                    if (*end_of_str == '(')
                        parens++;
                    if (*end_of_str == ')')
                        parens--;
                    end_of_str++;
                }

                *equal_ptr = '\0';

                if (*end_of_str == '\0') {
                    new_str = tprintf("%s={%s}", c->line, beg_of_str);
                }
                else {
                    *end_of_str = '\0';
                    new_str = tprintf("%s={%s} %s", c->line, beg_of_str,
                            end_of_str + 1);
                }
                old_str = c->line;
                c->line = new_str;

                line = new_str + strlen(old_str) + 1;
                tfree(old_str);
            }
        }
    }
}


static char *get_param_name(char *line)
{
    char *beg;
    char *equal_ptr = strchr(line, '=');

    if (!equal_ptr) {
        fprintf(stderr, "ERROR: could not find '=' on parameter line '%s'!\n",
                line);
        controlled_exit(EXIT_FAILURE);
    }

    equal_ptr = skip_back_ws(equal_ptr, line);

    beg = skip_back_non_ws(equal_ptr, line);

    return copy_substring(beg, equal_ptr);
}


static char *get_param_str(char *line)
{
    char *equal_ptr = strchr(line, '=');

    if (equal_ptr)
        return skip_ws(equal_ptr + 1);
    else
        return line;
}


struct dependency {
    int level;
    int skip;
    char *param_name;
    char *param_str;
    char *depends_on[100];
    struct card *card;
};


static int inp_get_param_level(
        int param_num, struct dependency *deps, int num_params)
{
    int i, k, l, level = 0;

    if (deps[param_num].level != -1)
        return deps[param_num].level;

    for (i = 0; deps[param_num].depends_on[i]; i++) {

        for (k = 0; k < num_params; k++)
            if (deps[param_num].depends_on[i] == deps[k].param_name)
                break;

        if (k >= num_params) {
            fprintf(stderr,
                    "ERROR: unable to find dependency parameter for %s!\n",
                    deps[param_num].param_name);
            controlled_exit(EXIT_FAILURE);
        }

        l = inp_get_param_level(k, deps, num_params) + 1;

        if (level < l)
            level = l;
    }

    deps[param_num].level = level;

    return level;
}


static int get_number_terminals(char *c)
{
    int i, j, k;
    char *name[12];
    char nam_buf[128];
    bool area_found = FALSE;

    switch (*c) {
        case 'r':
        case 'c':
        case 'l':
        case 'k':
        case 'f':
        case 'h':
        case 'b':
        case 'v':
        case 'i':
        case 'd':
            return 2;
            break;
        case 'u':
        case 'j':
        case 'w':
        case 'z':
            return 3;
            break;
        case 't':
        case 'o':
        case 'g':
        case 'e':
        case 's':
        case 'y':
            return 4;
            break;
        case 'm': /* recognition of 4, 5, 6, or 7 nodes for SOI devices needed
                   */
            i = 0;
            /* find the first token with "off" or "=" in the line*/
            while ((i < 20) && (*c != '\0')) {
                char *inst = gettok_instance(&c);
                strncpy(nam_buf, inst, sizeof(nam_buf) - 1);
                txfree(inst);
                if (strstr(nam_buf, "off") || strchr(nam_buf, '='))
                    break;
                i++;
            }
            return i - 2;
            break;
        case 'p': /* recognition of up to 100 cpl nodes */
            i = j = 0;
            /* find the last token in the line*/
            while ((i < 100) && (*c != '\0')) {
                char *tmp_inst = gettok_instance(&c);
                strncpy(nam_buf, tmp_inst, 32);
                tfree(tmp_inst);
                if (strchr(nam_buf, '='))
                    j++;
                i++;
            }
            if (i == 100)
                return 0;
            return i - j - 2;
            break;
        case 'q': /* recognition of 3, 4 or 5 terminal bjt's needed */
            /* QXXXXXXX NC NB NE <NS> <NT> MNAME <AREA> <OFF> <IC=VBE, VCE>
             * <TEMP=T> */
            /* 12 tokens maximum */
            i = j = 0;
            while ((i < 12) && (*c != '\0')) {
                char *comma;
                name[i] = gettok_instance(&c);
                if (strstr(name[i], "off") || strchr(name[i], '='))
                    j++;
#ifdef CIDER
                if (strstr(name[i], "save") || strstr(name[i], "print"))
                    j++;
#endif
                /* If we have IC=VBE, VCE instead of IC=VBE,VCE we need to inc
                 * j */
                if ((comma = strchr(name[i], ',')) != NULL &&
                        (*(++comma) == '\0'))
                    j++;
                /* If we have IC=VBE , VCE ("," is a token) we need to inc j
                 */
                if (eq(name[i], ","))
                    j++;
                i++;
            }
            i--;
            area_found = FALSE;
            for (k = i; k > i - j - 1; k--) {
                bool only_digits = TRUE;
                char *nametmp = name[k];
                /* MNAME has to contain at least one alpha character. AREA may
                   be assumed if we have a token with only digits, and where
                   the previous token does not end with a ',' */
                while (*nametmp) {
                    if (isalpha_c(*nametmp) || (*nametmp == ','))
                        only_digits = FALSE;
                    nametmp++;
                }
                if (only_digits && (strchr(name[k - 1], ',') == NULL))
                    area_found = TRUE;
            }
            for (k = i; k >= 0; k--)
                tfree(name[k]);
            if (area_found) {
                return i - j - 2;
            }
            else {
                return i - j - 1;
            }
            break;
        default:
            return 0;
            break;
    }
}


static char *ya_search_identifier(
        char *str, const char *identifier, char *str_begin);


static void inp_quote_params(struct card *s_c, struct card *e_c,
        struct dependency *deps, int num_params);

/* sort parameters based on parameter dependencies */

static void inp_sort_params(struct card *param_cards,
        struct card *card_bf_start, struct card *s_c, struct card *e_c)
{
    int i, j, num_params, ind = 0, max_level;

    struct card *c;
    int skipped;
    int arr_size;

    struct dependency *deps;

    if (param_cards == NULL)
        return;

    /* determine the number of lines with .param */

    arr_size = 0;
    for (c = param_cards; c; c = c->nextcard)
        if (strchr(c->line, '='))
            arr_size++;

    deps = TMALLOC(struct dependency, arr_size);

    num_params = 0;
    for (c = param_cards; c; c = c->nextcard)
        // ignore .param lines without '='
        if (strchr(c->line, '=')) {
            deps[num_params].depends_on[0] = NULL;
            deps[num_params].level = -1;
            deps[num_params].skip = 0;
            deps[num_params].param_name =
                    get_param_name(c->line); /* copy in fcn */
            deps[num_params].param_str = copy(get_param_str(c->line));
            deps[num_params].card = c;
            num_params++;
        }

    // look for duplicately defined parameters and mark earlier one to skip
    // param list is ordered as defined in netlist

    skipped = 0;
    for (i = 0; i < num_params; i++) {
        for (j = i + 1; j < num_params; j++)
            if (strcmp(deps[i].param_name, deps[j].param_name) == 0)
                break;
        if (j < num_params) {
            deps[i].skip = 1;
            skipped++;
        }
    }

    for (i = 0; i < num_params; i++)
        if (!deps[i].skip) {
            char *param = deps[i].param_name;
            for (j = 0; j < num_params; j++)
                if (j != i &&
                        search_plain_identifier(deps[j].param_str, param)) {
                    for (ind = 0; deps[j].depends_on[ind]; ind++)
                        ;
                    deps[j].depends_on[ind++] = param;
                    deps[j].depends_on[ind] = NULL;
                }
        }

    max_level = 0;
    for (i = 0; i < num_params; i++) {
        deps[i].level = inp_get_param_level(i, deps, num_params);
        if (max_level < deps[i].level)
            max_level = deps[i].level;
    }

    c = card_bf_start;

    ind = 0;
    for (i = 0; i <= max_level; i++)
        for (j = 0; j < num_params; j++)
            if (!deps[j].skip && deps[j].level == i) {
                c = insert_deck(c, deps[j].card);
                ind++;
            }
            else if (deps[j].skip) {
                line_free_x(deps[j].card, FALSE);
                deps[j].card = NULL;
            }

    num_params -= skipped;
    if (ind != num_params) {
        fprintf(stderr,
                "ERROR: found wrong number of parameters during levelization "
                "( %d instead of %d parameter s)!\n",
                ind, num_params);
        controlled_exit(EXIT_FAILURE);
    }

    inp_quote_params(s_c, e_c, deps, num_params);

    // clean up memory
    for (i = 0; i < arr_size; i++) {
        tfree(deps[i].param_name);
        tfree(deps[i].param_str);
    }

    tfree(deps);
}


static void inp_add_params_to_subckt(
        struct names *subckt_w_params, struct card *subckt_card)
{
    struct card *card = subckt_card->nextcard;
    char *subckt_line = subckt_card->line;
    char *new_line, *param_ptr, *subckt_name, *end_ptr;

    for (; card; card = card->nextcard) {

        char *curr_line = card->line;

        if (!ciprefix(".para", curr_line))
            break;

        param_ptr = strchr(curr_line, ' ');
        param_ptr = skip_ws(param_ptr);

        if (!strstr(subckt_line, "params:")) {
            new_line = tprintf("%s params: %s", subckt_line, param_ptr);

            subckt_name = skip_non_ws(subckt_line);
            subckt_name = skip_ws(subckt_name);
            end_ptr = skip_non_ws(subckt_name);
            add_name(subckt_w_params, copy_substring(subckt_name, end_ptr));
        }
        else {
            new_line = tprintf("%s %s", subckt_line, param_ptr);
        }

        tfree(subckt_line);
        subckt_line = new_line;

        *curr_line = '*';
    }

    subckt_card->line = subckt_line;
}


/*
 * process a sequence of decks
 *   starting from a         `.suckt' deck
 *   upto the corresponding  `.ends'  deck
 * return a pointer to the terminating `.ends' deck
 *
 * recursivly descend
 *   when another `.subckt' is found
 *
 * parameters are removed from the main list
 *   and collected into a local list `first_param_card'
 * then processed and reinserted into the main list
 *
 */

static struct card *inp_reorder_params_subckt(
        struct names *subckt_w_params, struct card *subckt_card)
{
    struct card *first_param_card = NULL;
    struct card *last_param_card = NULL;

    struct card *prev_card = subckt_card;
    struct card *c = subckt_card->nextcard;

    /* move .param lines to beginning of deck */
    while (c != NULL) {

        char *curr_line = c->line;

        if (*curr_line == '*') {
            prev_card = c;
            c = c->nextcard;
            continue;
        }

        if (ciprefix(".subckt", curr_line)) {
            prev_card = inp_reorder_params_subckt(subckt_w_params, c);
            c = prev_card->nextcard;
            continue;
        }

        if (ciprefix(".ends", curr_line)) {
            if (first_param_card) {
                inp_sort_params(first_param_card, subckt_card,
                        subckt_card->nextcard, c);
                inp_add_params_to_subckt(subckt_w_params, subckt_card);
            }
            return c;
        }

        if (ciprefix(".para", curr_line)) {
            prev_card->nextcard = c->nextcard;

            last_param_card = insert_deck(last_param_card, c);

            if (!first_param_card)
                first_param_card = last_param_card;

            c = prev_card->nextcard;
            continue;
        }

        prev_card = c;
        c = c->nextcard;
    }

    /* the terminating `.ends' deck wasn't found */
    controlled_exit(EXIT_FAILURE);
}


static void inp_reorder_params(
        struct names *subckt_w_params, struct card *list_head)
{
    struct card *first_param_card = NULL;
    struct card *last_param_card = NULL;

    struct card *prev_card = list_head;
    struct card *c = prev_card->nextcard;

    /* move .param lines to beginning of deck */
    while (c != NULL) {

        char *curr_line = c->line;

        if (*curr_line == '*') {
            prev_card = c;
            c = c->nextcard;
            continue;
        }

        if (ciprefix(".subckt", curr_line)) {
            prev_card = inp_reorder_params_subckt(subckt_w_params, c);
            c = prev_card->nextcard;
            continue;
        }

        /* check for an unexpected extra `.ends' deck */
        if (ciprefix(".ends", curr_line)) {
            fprintf(stderr, "Error: Unexpected extra .ends in line:\n  %s.\n",
                    curr_line);
            controlled_exit(EXIT_FAILURE);
        }

        if (ciprefix(".para", curr_line)) {
            prev_card->nextcard = c->nextcard;

            last_param_card = insert_deck(last_param_card, c);

            if (!first_param_card)
                first_param_card = last_param_card;

            c = prev_card->nextcard;
            continue;
        }

        prev_card = c;
        c = c->nextcard;
    }

    inp_sort_params(first_param_card, list_head, list_head->nextcard, NULL);
}


// iterate through deck and find lines with multiply defined parameters
//
// split line up into multiple lines and place those new lines immediately
// afetr the current multi-param line in the deck

static int inp_split_multi_param_lines(struct card *card, int line_num)
{
    for (; card; card = card->nextcard) {

        char *curr_line = card->line;

        if (*curr_line == '*')
            continue;

        if (ciprefix(".para", curr_line)) {

            char *equal_ptr, **array;
            int i, counter = 0;

            while ((equal_ptr = find_assignment(curr_line)) != NULL) {
                counter++;
                curr_line = equal_ptr + 1;
            }

            if (counter <= 1)
                continue;

            array = TMALLOC(char *, counter);

            // need to split multi param line
            curr_line = card->line;
            counter = 0;
            while ((equal_ptr = find_assignment(curr_line)) != NULL) {

                char *beg_param, *end_param;

                bool get_expression = FALSE;
                bool get_paren_expression = FALSE;

                beg_param = skip_back_ws(equal_ptr, curr_line);
                beg_param = skip_back_non_ws(beg_param, curr_line);
                end_param = skip_ws(equal_ptr + 1);
                while (*end_param != '\0' &&
                        (!isspace_c(*end_param) || get_expression ||
                                get_paren_expression)) {
                    if (*end_param == '{')
                        get_expression = TRUE;
                    if (*end_param == '(')
                        get_paren_expression = TRUE;
                    if (*end_param == '}')
                        get_expression = FALSE;
                    if (*end_param == ')')
                        get_paren_expression = FALSE;
                    end_param++;
                }

                if (end_param[-1] == ',')
                    end_param--;

                array[counter++] = tprintf(".param %.*s",
                        (int) (end_param - beg_param), beg_param);

                curr_line = end_param;
            }

            // comment out current multi-param line
            *(card->line) = '*';
            // insert new param lines immediately after current line
            for (i = 0; i < counter; i++)
                card = insert_new_line(card, array[i], line_num++, 0);

            tfree(array);
        }
    }

    return line_num;
}


static int identifier_char(char c)
{
    return (c == '_') || isalnum_c(c);
}


static bool b_transformation_wanted(const char *p)
{
    const char *start = p;

    for (p = start; (p = strpbrk(p, "vith")) != NULL; p++) {
        if (p > start && identifier_char(p[-1]))
            continue;
        if (strncmp(p, "v(", 2) == 0 || strncmp(p, "i(", 2) == 0)
            return TRUE;
        if (strncmp(p, "temper", 6) == 0 && !identifier_char(p[6]))
            return TRUE;
        if (strncmp(p, "hertz", 5) == 0 && !identifier_char(p[5]))
            return TRUE;
        if (strncmp(p, "time", 4) == 0 && !identifier_char(p[4]))
            return TRUE;
    }

    return FALSE;
}


char *search_identifier(char *str, const char *identifier, char *str_begin)
{
    if (str && identifier) {
        while ((str = strstr(str, identifier)) != NULL) {
            char before;

            if (str > str_begin)
                before = str[-1];
            else
                before = '\0';

            if (is_arith_char(before) || isspace_c(before) ||
                    strchr("=,{", before)) {
                char after = str[strlen(identifier)];
                if (is_arith_char(after) || isspace_c(after) ||
                        strchr(",}", after))
                    return str;
            }

            str++;
        }
    }
    return NULL;
}


char *ya_search_identifier(char *str, const char *identifier, char *str_begin)
{
    if (str && identifier) {
        while ((str = strstr(str, identifier)) != NULL) {
            char before;

            if (str > str_begin)
                before = str[-1];
            else
                before = '\0';

            if (is_arith_char(before) || isspace_c(before) ||
                    (str <= str_begin)) {
                char after = str[strlen(identifier)];
                if ((is_arith_char(after) || isspace_c(after) ||
                            after == '\0'))
                    break;
            }

            str++;
        }
    }
    return str;
}


static char *search_plain_identifier(char *str, const char *identifier)
{
    if (str && identifier) {
        char *str_begin = str;
        while ((str = strstr(str, identifier)) != NULL) {
            char before;

            if (str > str_begin)
                before = str[-1];
            else
                before = '\0';

            if (!before || !identifier_char(before)) {
                char after = str[strlen(identifier)];
                if (!after || !identifier_char(after))
                    return str;
            }

            str += strlen(identifier);
        }
    }
    return NULL;
}


/* ps compatibility:
   ECOMP 3 0 TABLE {V(1,2)} = (-1 0V) (1, 10V)
   -->
   ECOMP 3 0 int3 int0 1
   BECOMP int3 int0 V = pwl(V(1,2), -2, 0, -1, 0 , 1, 10V, 2, 10V)

   GD16 16 1 TABLE {V(16,1)} ((-100V,-1pV)(0,0)(1m,1u)(2m,1m))
   -->
   GD16 16 1 int_16 int_1 1
   BGD16 int_16 int_1 V = pwl (v(16,1) , -100V , -1pV , 0 , 0 , 1m , 1u , 2m ,
   1m)
*/

/* hs compatibility:
   Exxx n1 n2 VCVS n3 n4 gain --> Exxx n1 n2 n3 n4 gain
   Gxxx n1 n2 VCCS n3 n4 tr --> Gxxx n1 n2 n3 n4 tr

   Two step approach to keep the original names for reuse,
   i.e. for current measurements like i(Exxx):
   Exxx n1 n2 VOL = {equation}
   -->
   Exxx n1 n2 int1 0 1
   BExxx int1 0 V = {equation}

   Gxxx n1 n2 CUR = {equation}
   -->
   Gxxx n1 n2 int1 0 1
   BGxxx int1 0 V = {equation}

   Do the following transformations only if {equation} contains
   simulation output like v(node), v(node1, node2), i(branch).
   Otherwise let do numparam the substitutions (R=const is handled
   in inp2r.c).

   Rxxx n1 n2 R = {equation} or Rxxx n1 n2 {equation}
   -->
   BRxxx n1 n2 I = V(n1,n2)/{equation}

   Unfortunately the capability for ac noise calculation of
   resistance may be lost.

   Cxxx n1 n2 C = {equation} or Cxxx n1 n2 {equation}
   -->
   Exxx  n-aux 0  n1 n2  1
   Cxxx  n-aux 0         1
   Bxxx  n2 n1  I = i(Exxx) * equation

   Lxxx n1 n2 L = {equation} or Lxxx n1 n2 {equation}
   -->
   Fxxx n-aux 0  Bxxx -1
   Lxxx n-aux 0      1
   Bxxx n1 n2 V = v(n-aux) * 1e-16

*/

static void inp_compat(struct card *card)
{
    char *str_ptr, *cut_line, *title_tok, *node1, *node2;
    char *out_ptr, *exp_ptr, *beg_ptr, *end_ptr, *copy_ptr, *del_ptr;
    char *xline, *x2line = NULL, *x3line = NULL, *x4line = NULL;
    size_t xlen, i, pai = 0, paui = 0, ii;
    char *ckt_array[100];

    int skip_control = 0;

    char *equation, *tc1_ptr = NULL, *tc2_ptr = NULL;
    double tc1 = 0.0, tc2 = 0.0;

    for (; card; card = card->nextcard) {

        char *curr_line = card->line;

        /* exclude any command inside .control ... .endc */
        if (ciprefix(".control", curr_line)) {
            skip_control++;
            continue;
        }
        else if (ciprefix(".endc", curr_line)) {
            skip_control--;
            continue;
        }
        else if (skip_control > 0) {
            continue;
        }

        if (*curr_line == '*')
            continue;

        if (*curr_line == 'e') {
            /*    Exxx n1 n2 VCVS n3 n4 gain --> Exxx n1 n2 n3 n4 gain
                  remove vcvs */
            replace_token(curr_line, "vcvs", 4, 7);

            /* Exxx n1 n2 value={equation}
               -->
               Exxx n1 n2   vol={equation} */
            if ((str_ptr = search_plain_identifier(curr_line, "value")) !=
                    NULL) {
                if (str_ptr[5] == '=')
                    *str_ptr++ = ' ';
                strncpy(str_ptr, " vol=", 5);
            }
            /* Exxx n1 n2 TABLE {expression} = (x0, y0) (x1, y1) (x2, y2)
               -->
               Exxx n1 n2 int1 0 1
               BExxx int1 0 V = pwl (expression, x0-(x2-x0)/2, y0, x0, y0, x1,
               y1, x2, y2, x2+(x2-x0)/2, y2)
            */
            if ((str_ptr = strstr(curr_line, "table")) != NULL) {
                char *expression, *firstno, *ffirstno, *secondno, *midline,
                        *lastno, *lastlastno;
                double fnumber, lnumber, delta;
                int nerror;
                cut_line = curr_line;
                /* title and nodes */
                title_tok = gettok(&cut_line);
                node1 = gettok(&cut_line);
                node2 = gettok(&cut_line);
                // Exxx  n1 n2 int1 0 1
                ckt_array[0] = tprintf("%s %s %s %s_int1 0 1", title_tok,
                        node1, node2, title_tok);
                // skip "table"
                cut_line = skip_ws(cut_line);
                if (ciprefix("table", cut_line)) {
                    /* a regular TABLE line */
                    cut_line += 5;
                    // compatibility, allow table = {expr} {pairs}
                    if (*cut_line == '=')
                        *cut_line++ = ' ';
                    // get the expression
                    str_ptr = gettok_char(&cut_line, '{', FALSE, FALSE);
                    expression = gettok_char(
                            &cut_line, '}', TRUE, TRUE); /* expression */
                    if (!expression || !str_ptr) {
                        fprintf(stderr,
                                "Error: bad syntax in line %d\n  %s\n",
                                card->linenum_orig, card->line);
                        controlled_exit(EXIT_BAD);
                    }
                    tfree(str_ptr);
                    /* remove '{' and '}' from expression */
                    if ((str_ptr = strchr(expression, '{')) != NULL)
                        *str_ptr = ' ';
                    if ((str_ptr = strchr(expression, '}')) != NULL)
                        *str_ptr = ' ';
                    /* cut_line may now have a '=', if yes, it will have '{'
                       and '}' (braces around token after '=') */
                    if ((str_ptr = strchr(cut_line, '=')) != NULL)
                        *str_ptr = ' ';
                    if ((str_ptr = strchr(cut_line, '{')) != NULL)
                        *str_ptr = ' ';
                    if ((str_ptr = strchr(cut_line, '}')) != NULL)
                        *str_ptr = ' ';
                    /* get first two numbers to establish extrapolation */
                    str_ptr = cut_line;
                    ffirstno = gettok_node(&cut_line);
                    if (!ffirstno) {
                        fprintf(stderr,
                                "Error: bad syntax in line %d\n  %s\n",
                                card->linenum_orig, card->line);
                        controlled_exit(EXIT_BAD);
                    }
                    firstno = copy(ffirstno);
                    fnumber = INPevaluate(&ffirstno, &nerror, TRUE);
                    secondno = gettok_node(&cut_line);
                    midline = cut_line;
                    cut_line = strrchr(str_ptr, '(');
                    if (!cut_line) {
                        fprintf(stderr,
                                "Error: bad syntax in line %d (missing "
                                "parentheses)\n  %s\n",
                                card->linenum_orig, card->line);
                        controlled_exit(EXIT_BAD);
                    }
                    /* replace '(' with ',' and ')' with ' ' */
                    for (; *str_ptr; str_ptr++)
                        if (*str_ptr == '(')
                            *str_ptr = ',';
                        else if (*str_ptr == ')')
                            *str_ptr = ' ';
                    /* scan for last two numbers */
                    lastno = gettok_node(&cut_line);
                    lnumber = INPevaluate(&lastno, &nerror, FALSE);
                    /* check for max-min and take half the difference for
                     * delta */
                    delta = (lnumber - fnumber) / 2.;
                    lastlastno = gettok_node(&cut_line);
                    if (!secondno || (*midline == '\0') || (delta <= 0.) ||
                            !lastlastno) {
                        fprintf(stderr,
                                "Error: bad syntax in line %d\n  %s\n",
                                card->linenum_orig, card->line);
                        controlled_exit(EXIT_BAD);
                    }
                    ckt_array[1] = tprintf("b%s %s_int1 0 v = pwl(%s, %e, "
                                           "%s, %s, %s, %s, %e, %s)",
                            title_tok, title_tok, expression, fnumber - delta,
                            secondno, firstno, secondno, midline,
                            lnumber + delta, lastlastno);

                    // comment out current variable e line
                    *(card->line) = '*';
                    // insert new B source line immediately after current line
                    for (i = 0; i < 2; i++)
                        card = insert_new_line(card, ckt_array[i], 0, 0);

                    tfree(firstno);
                    tfree(lastlastno);
                }
                else {
                    /* not used */
                    tfree(ckt_array[0]);
                }
                tfree(title_tok);
                tfree(node1);
                tfree(node2);
            }
            /* Exxx n1 n2 VOL = {equation}
               -->
               Exxx n1 n2 int1 0 1
               BExxx int1 0 V = {equation}
            */
            /* search for ' vol=' or ' vol =' */
            if (((str_ptr = strchr(curr_line, '=')) != NULL) &&
                    prefix("vol",
                            skip_back_non_ws(skip_back_ws(str_ptr, curr_line),
                                    curr_line))) {
                cut_line = curr_line;
                /* title and nodes */
                title_tok = gettok(&cut_line);
                node1 = gettok(&cut_line);
                node2 = gettok(&cut_line);
                /* Find equation, starts with '{', till end of line */
                str_ptr = strchr(cut_line, '{');
                if (str_ptr == NULL) {
                    fprintf(stderr, "ERROR: mal formed E line: %s\n",
                            curr_line);
                    controlled_exit(EXIT_FAILURE);
                }

                // Exxx  n1 n2 int1 0 1
                ckt_array[0] = tprintf("%s %s %s %s_int1 0 1", title_tok,
                        node1, node2, title_tok);
                // BExxx int1 0 V = {equation}
                ckt_array[1] = tprintf("b%s %s_int1 0 v = %s", title_tok,
                        title_tok, str_ptr);

                // comment out current variable e line
                *(card->line) = '*';
                // insert new B source line immediately after current line
                for (i = 0; i < 2; i++)
                    card = insert_new_line(card, ckt_array[i], 0, 0);

                tfree(title_tok);
                tfree(node1);
                tfree(node2);
            }
        }
        else if (*curr_line == 'g') {
            /* Gxxx n1 n2 VCCS n3 n4 tr --> Gxxx n1 n2 n3 n4 tr
               remove vccs */
            replace_token(curr_line, "vccs", 4, 7);

            /* Gxxx n1 n2 value={equation}
               -->
               Gxxx n1 n2   cur={equation} */
            if ((str_ptr = search_plain_identifier(curr_line, "value")) !=
                    NULL) {
                if (str_ptr[5] == '=')
                    *str_ptr++ = ' ';
                strncpy(str_ptr, " cur=", 5);
            }

            /* Gxxx n1 n2 TABLE {expression} = (x0, y0) (x1, y1) (x2, y2)
               -->
               Gxxx n1 n2 int1 0 1
               BGxxx int1 0 V = pwl (expression, x0-(x2-x0)/2, y0, x0, y0, x1,
               y1, x2, y2, x2+(x2-x0)/2, y2)
            */
            if ((str_ptr = strstr(curr_line, "table")) != NULL) {
                char *expression, *firstno, *ffirstno, *ffirstnof, *secondno,
                        *midline, *lastno, *lastnof, *lastlastno;
                char *m_ptr, *m_token;
                double fnumber, lnumber, delta;
                int nerror;
                cut_line = curr_line;
                /* title and nodes */
                title_tok = gettok(&cut_line);
                node1 = gettok(&cut_line);
                node2 = gettok(&cut_line);
                // Gxxx  n1 n2 int1 0 1
                // or
                // Gxxx  n1 n2 int1 0 m='expr'
                /* find multiplier m at end of line */
                m_ptr = strstr(cut_line, "m=");
                if (m_ptr) {
                    m_token = copy(m_ptr + 2); // get only the expression
                    *m_ptr = '\0';
                }
                else
                    m_token = copy("1");
                ckt_array[0] = tprintf("%s %s %s %s_int1 0 %s", title_tok,
                        node1, node2, title_tok, m_token);
                // skip "table"
                cut_line = skip_ws(cut_line);
                if (!ciprefix("table", cut_line)) {
                    fprintf(stderr, "Error: bad syntax in line %d\n  %s\n",
                            card->linenum_orig, card->line);
                    controlled_exit(EXIT_BAD);
                }
                cut_line += 5;
                // compatibility, allow table = {expr} {pairs}
                if (*cut_line == '=')
                    *cut_line++ = ' ';
                // get the expression
                str_ptr = gettok_char(&cut_line, '{', FALSE, FALSE);
                expression = gettok_char(
                        &cut_line, '}', TRUE, TRUE); /* expression */
                if (!expression || !str_ptr) {
                    fprintf(stderr, "Error: bad syntax in line %d\n  %s\n",
                            card->linenum_orig, card->line);
                    controlled_exit(EXIT_BAD);
                }
                tfree(str_ptr);
                /* remove '{' and '}' from expression */
                if ((str_ptr = strchr(expression, '{')) != NULL)
                    *str_ptr = ' ';
                if ((str_ptr = strchr(expression, '}')) != NULL)
                    *str_ptr = ' ';
                /* cut_line may now have a '=', if yes, it will have '{' and
                   '}' (braces around token after '=') */
                if ((str_ptr = strchr(cut_line, '=')) != NULL)
                    *str_ptr = ' ';
                if ((str_ptr = strchr(cut_line, '{')) != NULL)
                    *str_ptr = ' ';
                if ((str_ptr = strchr(cut_line, '}')) != NULL)
                    *str_ptr = ' ';
                /* get first two numbers to establish extrapolation */
                str_ptr = cut_line;
                ffirstnof = ffirstno = gettok_node(&cut_line);
                if (!ffirstno) {
                    fprintf(stderr, "Error: bad syntax in line %d\n  %s\n",
                            card->linenum_orig, card->line);
                    controlled_exit(EXIT_BAD);
                }
                firstno = copy(ffirstno);
                fnumber = INPevaluate(&ffirstno, &nerror, TRUE);
                secondno = gettok_node(&cut_line);
                midline = cut_line;
                cut_line = strrchr(str_ptr, '(');
                /* replace '(' with ',' and ')' with ' ' */
                for (; *str_ptr; str_ptr++)
                    if (*str_ptr == '(')
                        *str_ptr = ',';
                    else if (*str_ptr == ')')
                        *str_ptr = ' ';
                /* scan for last two numbers */
                lastnof = lastno = gettok_node(&cut_line);
                lnumber = INPevaluate(&lastno, &nerror, FALSE);
                /* check for max-min and take half the difference for delta */
                delta = (lnumber - fnumber) / 2.;
                lastlastno = gettok_node(&cut_line);
                if (!secondno || (*midline == '\0') || (delta <= 0.) ||
                        !lastlastno) {
                    fprintf(stderr, "Error: bad syntax in line %d\n  %s\n",
                            card->linenum_orig, card->line);
                    controlled_exit(EXIT_BAD);
                }
                /* BGxxx int1 0 V = pwl (expression, x0-(x2-x0)/2, y0, x0, y0,
                 * x1, y1, x2, y2, x2+(x2-x0)/2, y2) */
                ckt_array[1] = tprintf("b%s %s_int1 0 v = pwl(%s, %e, %s, "
                                       "%s, %s, %s, %e, %s)",
                        title_tok, title_tok, expression, fnumber - delta,
                        secondno, firstno, secondno, midline, lnumber + delta,
                        lastlastno);

                // comment out current variable e line
                *(card->line) = '*';
                // insert new B source line immediately after current line
                for (i = 0; i < 2; i++)
                    card = insert_new_line(card, ckt_array[i], 0, 0);

                tfree(firstno);
                tfree(ffirstnof);
                tfree(secondno);
                tfree(lastnof);
                tfree(lastlastno);
                tfree(expression);
                tfree(title_tok);
                tfree(node1);
                tfree(node2);
                tfree(m_token);
            }
            /*
              Gxxx n1 n2 CUR = {equation}
              -->
              Gxxx n1 n2 int1 0 1
              BGxxx int1 0 V = {equation}
            */
            /* search for ' cur=' or ' cur =' */
            if (((str_ptr = strchr(curr_line, '=')) != NULL) &&
                    prefix("cur",
                            skip_back_non_ws(skip_back_ws(str_ptr, curr_line),
                                    curr_line))) {
                char *m_ptr, *m_token;
                cut_line = curr_line;
                /* title and nodes */
                title_tok = gettok(&cut_line);
                node1 = gettok(&cut_line);
                node2 = gettok(&cut_line);
                /* Find equation, starts with '{', till end of line */
                str_ptr = strchr(cut_line, '{');
                if (str_ptr == NULL) {
                    fprintf(stderr, "ERROR: mal formed G line: %s\n",
                            curr_line);
                    controlled_exit(EXIT_FAILURE);
                }
                /* find multiplier m at end of line */
                m_ptr = strstr(cut_line, "m=");
                if (m_ptr) {
                    m_token = copy(m_ptr + 2); // get only the expression
                    *m_ptr = '\0';
                }
                else
                    m_token = copy("1");
                // Gxxx  n1 n2 int1 0 1
                // or
                // Gxxx  n1 n2 int1 0 m='expr'
                ckt_array[0] = tprintf("%s %s %s %s_int1 0 %s", title_tok,
                        node1, node2, title_tok, m_token);
                // BGxxx int1 0 V = {equation}
                ckt_array[1] = tprintf("b%s %s_int1 0 v = %s", title_tok,
                        title_tok, str_ptr);

                // comment out current variable g line
                *(card->line) = '*';
                // insert new B source line immediately after current line
                for (i = 0; i < 2; i++)
                    card = insert_new_line(card, ckt_array[i], 0, 0);

                tfree(title_tok);
                tfree(m_token);
                tfree(node1);
                tfree(node2);
            }
        }

        /* F element compatibility */
        else if (*curr_line == 'f') {
            char *equastr, *vnamstr;
            /* Fxxx n1 n2 CCCS vnam gain --> Fxxx n1 n2 vnam gain
               remove cccs */
            replace_token(curr_line, "cccs", 4, 6);

            /* Deal with
               Fxxx n1 n2 vnam {equation}
               if equation contains the 'temper' token */
            if (search_identifier(curr_line, "temper", curr_line)) {
                cut_line = curr_line;
                title_tok = gettok(&cut_line);
                node1 = gettok(&cut_line);
                node2 = gettok(&cut_line);
                vnamstr = gettok(&cut_line);
                equastr = gettok(&cut_line);
                /*
                Fxxx n1 n2 vnam {equation}
                -->
                Fxxx n1 n2 vbFxxx -1
                bFxxx int1 0 i = i(vnam)*{equation}
                vbFxxx int1 0 0
                */
                // Fxxx n1 n2 VBFxxx -1
                ckt_array[0] = tprintf("%s %s %s vb%s -1", title_tok, node1,
                        node2, title_tok);
                // BFxxx BFxxx_int1 0 I = I(vnam)*{equation}
                ckt_array[1] = tprintf("b%s %s_int1 0 i = i(%s) * (%s)",
                        title_tok, title_tok, vnamstr, equastr);
                // VBFxxx int1 0 0
                ckt_array[2] =
                        tprintf("vb%s %s_int1 0 dc 0", title_tok, title_tok);
                // comment out current variable f line
                *(card->line) = '*';
                // insert new three lines immediately after current line
                for (i = 0; i < 3; i++)
                    card = insert_new_line(card, ckt_array[i], 0, 0);

                tfree(title_tok);
                tfree(vnamstr);
                tfree(equastr);
                tfree(node1);
                tfree(node2);
            }
        }
        /* H element compatibility */
        else if (*curr_line == 'h') {
            char *equastr, *vnamstr;
            /* Hxxx n1 n2 CCVS vnam transres --> Hxxx n1 n2 vnam transres
               remove cccs */
            replace_token(curr_line, "ccvs", 4, 6);

            /* Deal with
               Hxxx n1 n2 vnam {equation}
               if equation contains the 'temper' token */
            if (search_identifier(curr_line, "temper", curr_line)) {
                cut_line = curr_line;
                title_tok = gettok(&cut_line);
                node1 = gettok(&cut_line);
                node2 = gettok(&cut_line);
                vnamstr = gettok(&cut_line);
                equastr = gettok(&cut_line);
                /*
                Hxxx n1 n2 vnam {equation}
                -->
                Hxxx n1 n2 vbHxxx -1
                bHxxx int1 0 i = i(vnam)*{equation}
                vbHxxx int1 0 0
                */
                // Hxxx n1 n2 VBHxxx -1
                ckt_array[0] = tprintf("%s %s %s vb%s -1", title_tok, node1,
                        node2, title_tok);
                // BHxxx BHxxx_int1 0 I = I(vnam)*{equation}
                ckt_array[1] = tprintf("b%s %s_int1 0 i = i(%s) * (%s)",
                        title_tok, title_tok, vnamstr, equastr);
                // VBHxxx int1 0 0
                ckt_array[2] =
                        tprintf("vb%s %s_int1 0 dc 0", title_tok, title_tok);
                // comment out current variable h line
                *(card->line) = '*';
                // insert new three lines immediately after current line
                for (i = 0; i < 3; i++)
                    card = insert_new_line(card, ckt_array[i], 0, 0);

                tfree(title_tok);
                tfree(vnamstr);
                tfree(equastr);
                tfree(node1);
                tfree(node2);
            }
        }

        /* Rxxx n1 n2 R = {equation} or Rxxx n1 n2 {equation}
           -->
           BRxxx pos neg I = V(pos, neg)/{equation}
        */
        else if (*curr_line == 'r') {
            cut_line = curr_line;
            /* make BRxxx pos neg I = V(pos, neg)/{equation}*/
            title_tok = gettok(&cut_line);
            node1 = gettok(&cut_line);
            node2 = gettok(&cut_line);
            /* check only after skipping Rname and nodes, either may contain
             * time (e.g. Rtime)*/
            if (!b_transformation_wanted(cut_line)) {
                tfree(title_tok);
                tfree(node1);
                tfree(node2);
                continue;
            }

            /* Find equation, starts with '{', till end of line */
            str_ptr = strchr(cut_line, '{');
            if (str_ptr == NULL) {
                /* if not, equation may start with a '(' */
                str_ptr = strchr(cut_line, '(');
                if (str_ptr == NULL) {
                    fprintf(stderr, "ERROR: mal formed R line: %s\n",
                            curr_line);
                    controlled_exit(EXIT_FAILURE);
                }
                equation = gettok_char(&str_ptr, ')', TRUE, TRUE);
            }
            else
                equation = gettok_char(&str_ptr, '}', TRUE, TRUE);
            str_ptr = strstr(cut_line, "tc1");
            if (str_ptr) {
                /* We need to have 'tc1=something */
                if (str_ptr[3] &&
                        (isspace_c(str_ptr[3]) || (str_ptr[3] == '='))) {
                    tc1_ptr = strchr(str_ptr, '=');
                    if (tc1_ptr) {
                        tc1_ptr++;
                        int error;
                        tc1 = INPevaluate(&tc1_ptr, &error, 1);
                    }
                }
            }
            str_ptr = strstr(cut_line, "tc2");
            if (str_ptr) {
                /* We need to have 'tc2=something */
                if (str_ptr[3] &&
                        (isspace_c(str_ptr[3]) || (str_ptr[3] == '='))) {
                    tc2_ptr = strchr(str_ptr, '=');
                    if (tc2_ptr) {
                        tc2_ptr++;
                        int error;
                        tc2 = INPevaluate(&tc2_ptr, &error, 1);
                    }
                }
            }

            /* white noise model by x2line, x3line, x4line
               if instance parameter noisy=1 (or noise=1) is set */
            bool rnoise = FALSE;
            if (strstr(cut_line, "noisy=1") || strstr(cut_line, "noise=1"))
                rnoise = TRUE;

            if ((tc1_ptr == NULL) && (tc2_ptr == NULL)) {
                xline = tprintf("b%s %s %s i = v(%s, %s)/(%s)", title_tok,
                        node1, node2, node1, node2, equation);
                if (rnoise) {
                    x2line = tprintf("b%s_1 %s %s i = i(v%s_3)/sqrt(%s)",
                            title_tok, node1, node2, title_tok, equation);
                    x3line =
                            tprintf("r%s_2 %s_3 0 1.0", title_tok, title_tok);
                    x4line = tprintf("v%s_3 %s_3 0 0", title_tok, title_tok);
                }
            }
            else if (tc2_ptr == NULL) {
                xline = tprintf("b%s %s %s i = v(%s, %s)/(%s) tc1=%15.8e "
                                "reciproctc=1",
                        title_tok, node1, node2, node1, node2, equation, tc1);
                if (rnoise) {
                    x2line = tprintf("b%s_1 %s %s i = i(v%s_3)/sqrt(%s)",
                            title_tok, node1, node2, title_tok, equation);
                    x3line = tprintf("r%s_2 %s_3 0 1.0 tc1=%15.8e", title_tok,
                            title_tok, tc1);
                    x4line = tprintf("v%s_3 %s_3 0 0", title_tok, title_tok);
                }
            }
            else {
                xline = tprintf("b%s %s %s i = v(%s, %s)/(%s) tc1=%15.8e "
                                "tc2=%15.8e reciproctc=1",
                        title_tok, node1, node2, node1, node2, equation, tc1,
                        tc2);
                if (rnoise) {
                    x2line = tprintf("b%s_1 %s %s i = i(v%s_3)/sqrt(%s)",
                            title_tok, node1, node2, title_tok, equation);
                    x3line = tprintf("r%s_2 %s_3 0 1.0 tc1=%15.8e tc2=%15.8e",
                            title_tok, title_tok, tc1, tc2);
                    x4line = tprintf("v%s_3 %s_3 0 0", title_tok, title_tok);
                }
            }
            tc1_ptr = NULL;
            tc2_ptr = NULL;

            // comment out current old R line
            *(card->line) = '*';
            // insert new B source line immediately after current line
            card = insert_new_line(card, xline, 0, 0);
            if (rnoise) {
                card = insert_new_line(card, x2line, 0, 0);
                card = insert_new_line(card, x3line, 0, 0);
                card = insert_new_line(card, x4line, 0, 0);
            }

            tfree(title_tok);
            tfree(node1);
            tfree(node2);
            tfree(equation);
        }
        /* Cxxx n1 n2 C = {equation} or Cxxx n1 n2 {equation}
           -->
           Exxx  n-aux 0  n1 n2  1
           Cxxx  n-aux 0         1
           Bxxx  n2 n1  I = i(Exxx) * equation
        */
        else if (*curr_line == 'c') {
            cut_line = curr_line;
            title_tok = gettok(&cut_line);
            node1 = gettok(&cut_line);
            node2 = gettok(&cut_line);
            /* check only after skipping Cname and nodes, either may contain
             * time (e.g. Ctime)*/
            if (!b_transformation_wanted(cut_line)) {
                tfree(title_tok);
                tfree(node1);
                tfree(node2);
                continue;
            }

            /* Find equation, starts with '{', till end of line */
            str_ptr = strchr(cut_line, '{');
            if (str_ptr == NULL) {
                /* if not, equation may start with a '(' */
                str_ptr = strchr(cut_line, '(');
                if (str_ptr == NULL) {
                    fprintf(stderr, "ERROR: mal formed C line: %s\n",
                            curr_line);
                    controlled_exit(EXIT_FAILURE);
                }
                equation = gettok_char(&str_ptr, ')', TRUE, TRUE);
            }
            else
                equation = gettok_char(&str_ptr, '}', TRUE, TRUE);
            str_ptr = strstr(cut_line, "tc1");
            if (str_ptr) {
                /* We need to have 'tc1=something */
                if (str_ptr[3] &&
                        (isspace_c(str_ptr[3]) || (str_ptr[3] == '='))) {
                    tc1_ptr = strchr(str_ptr, '=');
                    if (tc1_ptr) {
                        tc1_ptr++;
                        int error;
                        tc1 = INPevaluate(&tc1_ptr, &error, 1);
                    }
                }
            }
            str_ptr = strstr(cut_line, "tc2");
            if (str_ptr) {
                /* We need to have 'tc2=something */
                if (str_ptr[3] &&
                        (isspace_c(str_ptr[3]) || (str_ptr[3] == '='))) {
                    tc2_ptr = strchr(str_ptr, '=');
                    if (tc2_ptr) {
                        tc2_ptr++;
                        int error;
                        tc2 = INPevaluate(&tc2_ptr, &error, 1);
                    }
                }
            }
            // Exxx  n-aux 0  n1 n2  1
            ckt_array[0] = tprintf("e%s %s_int2 0 %s %s 1", title_tok,
                    title_tok, node1, node2);
            // Cxxx  n-aux 0  1
            ckt_array[1] = tprintf("c%s %s_int2 0 1", title_tok, title_tok);
            // Bxxx  n2 n1  I = i(Exxx) * equation
            if ((tc1_ptr == NULL) && (tc2_ptr == NULL)) {
                ckt_array[2] = tprintf("b%s %s %s i = i(e%s) * (%s)",
                        title_tok, node2, node1, title_tok, equation);
            }
            else if (tc2_ptr == NULL) {
                ckt_array[2] = tprintf(
                        "b%s %s %s i = i(e%s) * (%s) tc1=%15.8e reciproctc=1",
                        title_tok, node2, node1, title_tok, equation, tc1);
            }
            else {
                ckt_array[2] = tprintf("b%s %s %s i = i(e%s) * (%s) "
                                       "tc1=%15.8e tc2=%15.8e reciproctc=1",
                        title_tok, node2, node1, title_tok, equation, tc1,
                        tc2);
            }
            tc1_ptr = NULL;
            tc2_ptr = NULL;
            // comment out current variable capacitor line
            *(card->line) = '*';
            // insert new B source line immediately after current line
            for (i = 0; i < 3; i++)
                card = insert_new_line(card, ckt_array[i], 0, 0);

            tfree(title_tok);
            tfree(node1);
            tfree(node2);
            tfree(equation);
        }

        /* Lxxx n1 n2 L = {equation} or Lxxx n1 n2 {equation}
           -->
           Fxxx n-aux 0  Bxxx -1
           Lxxx n-aux 0      1
           Bxxx n1 n2 V = v(n-aux) * equation
        */
        else if (*curr_line == 'l') {
            cut_line = curr_line;
            /* title and nodes */
            title_tok = gettok(&cut_line);
            node1 = gettok(&cut_line);
            node2 = gettok(&cut_line);
            if (!b_transformation_wanted(cut_line)) {
                tfree(title_tok);
                tfree(node1);
                tfree(node2);
                continue;
            }

            /* Find equation, starts with '{', till end of line */
            str_ptr = strchr(cut_line, '{');
            if (str_ptr == NULL) {
                /* if not, equation may start with a '(' */
                str_ptr = strchr(cut_line, '(');
                if (str_ptr == NULL) {
                    fprintf(stderr, "ERROR: mal formed L line: %s\n",
                            curr_line);
                    controlled_exit(EXIT_FAILURE);
                }
                equation = gettok_char(&str_ptr, ')', TRUE, TRUE);
            }
            else
                equation = gettok_char(&str_ptr, '}', TRUE, TRUE);
            str_ptr = strstr(cut_line, "tc1");
            if (str_ptr) {
                /* We need to have 'tc1=something */
                if (str_ptr[3] &&
                        (isspace_c(str_ptr[3]) || (str_ptr[3] == '='))) {
                    tc1_ptr = strchr(str_ptr, '=');
                    if (tc1_ptr) {
                        tc1_ptr++;
                        int error;
                        tc1 = INPevaluate(&tc1_ptr, &error, 1);
                    }
                }
            }
            str_ptr = strstr(cut_line, "tc2");
            if (str_ptr) {
                /* We need to have 'tc2=something */
                if (str_ptr[3] &&
                        (isspace_c(str_ptr[3]) || (str_ptr[3] == '='))) {
                    tc2_ptr = strchr(str_ptr, '=');
                    if (tc2_ptr) {
                        tc2_ptr++;
                        int error;
                        tc2 = INPevaluate(&tc2_ptr, &error, 1);
                    }
                }
            }
            // Fxxx  n-aux 0  Bxxx  1
            ckt_array[0] = tprintf(
                    "f%s %s_int2 0 b%s -1", title_tok, title_tok, title_tok);
            // Lxxx  n-aux 0  1
            ckt_array[1] = tprintf("l%s %s_int2 0 1", title_tok, title_tok);
            // Bxxx  n1 n2  V = v(n-aux) * equation
            if ((tc1_ptr == NULL) && (tc2_ptr == NULL)) {
                ckt_array[2] = tprintf("b%s %s %s v = v(%s_int2) * (%s)",
                        title_tok, node1, node2, title_tok, equation);
            }
            else if (tc2_ptr == NULL) {
                ckt_array[2] = tprintf("b%s %s %s v = v(%s_int2) * (%s) "
                                       "tc1=%15.8e reciproctc=0",
                        title_tok, node2, node1, title_tok, equation, tc1);
            }
            else {
                ckt_array[2] = tprintf("b%s %s %s v = v(%s_int2) * (%s) "
                                       "tc1=%15.8e tc2=%15.8e reciproctc=0",
                        title_tok, node2, node1, title_tok, equation, tc1,
                        tc2);
            }
            tc1_ptr = NULL;
            tc2_ptr = NULL;
            // comment out current variable inductor line
            *(card->line) = '*';
            // insert new B source line immediately after current line
            for (i = 0; i < 3; i++)
                card = insert_new_line(card, ckt_array[i], 0, 0);

            tfree(title_tok);
            tfree(node1);
            tfree(node2);
            tfree(equation);
        }
        /* .probe -> .save
           .print, .plot, .save, .four,
           An ouput vector may be replaced by the following:
           myoutput=par('expression')
           .meas
           A vector out_variable may be replaced by
           par('expression')
        */
        else if (*curr_line == '.') {
            // replace .probe by .save
            if ((str_ptr = strstr(curr_line, ".probe")) != NULL)
                memcpy(str_ptr, ".save ", 6);

            /* Various formats for measure statement:
             * .MEASURE {DC|AC|TRAN} result WHEN out_variable=val
             * + <TD=td> <FROM=val> <TO=val>
             * + <CROSS=# | CROSS=LAST> <RISE=#|RISE=LAST> <FALL=#|FALL=LAST>
             *
             * .MEASURE {DC|AC|TRAN} result WHEN out_variable=out_variable2
             * + <TD=td> <FROM=val> <TO=val>
             * + <CROSS=# | CROSS=LAST> <RISE=#|RISE=LAST> <FALL=#|FALL=LAST>
             *
             * .MEASURE {DC|AC|TRAN} result FIND out_variable WHEN
             out_variable2=val
             * + <TD=td> <FROM=val> <TO=val>
             * + <CROSS=# | CROSS=LAST> <RISE=#|RISE=LAST> <FALL=#|FALL=LAST>
             *
             * .MEASURE {DC|AC|TRAN} result FIND out_variable WHEN
             out_variable2=out_variable3
             * + <TD=td>
             * + <CROSS=# | CROSS=LAST> <RISE=#|RISE=LAST> <FALL=#|FALL=LAST>
             *
             * .MEASURE {DC|AC|TRAN} result FIND out_variable AT=val
             * + <FROM=val> <TO=val>
             *
             * .MEASURE {DC|AC|TRAN} result {AVG|MIN|MAX|MIN_AT|MAX_AT|PP|RMS}
             out_variable
             * + <TD=td> <FROM=val> <TO=val>
             *
             * .MEASURE {DC|AC|TRAN} result INTEG<RAL> out_variable
             * + <TD=td> <FROM=val> <TO=val>
             *
             * .MEASURE {DC|AC|TRAN} result DERIV<ATIVE> out_variable AT=val
             *
             * .MEASURE {DC|AC|TRAN} result DERIV<ATIVE> out_variable WHEN
             out_variable2=val
             * + <TD=td>
             * + <CROSS=# | CROSS=LAST> <RISE=#|RISE=LAST> <FALL=#|FALL=LAST>
             *
             * .MEASURE {DC|AC|TRAN} result DERIV<ATIVE> out_variable WHEN
             out_variable2=out_variable3
             * + <TD=td>
             * + <CROSS=# | CROSS=LAST> <RISE=#|RISE=LAST> <FALL=#|FALL=LAST>

             The user may set any out_variable to par(' expr ').
             We have to replace this by v(pa_xx) and generate a B source line.

             *
             -----------------------------------------------------------------
           */
            if (ciprefix(".meas", curr_line)) {
                if (strstr(curr_line, "par(") == NULL)
                    continue;
                cut_line = curr_line;
                // search for 'par('
                while ((str_ptr = strstr(cut_line, "par(")) != NULL) {
                    if (pai > 99) {
                        fprintf(stderr,
                                "ERROR: More than 99 function calls to "
                                "par()\n");
                        fprintf(stderr, "  Limited to 99 per input file\n");
                        controlled_exit(EXIT_FAILURE);
                    }

                    // we have ' par({ ... })', the right delimeter is a ' '
                    // or '='
                    if (ciprefix(" par({", (str_ptr - 1))) {
                        // find expression
                        beg_ptr = end_ptr = str_ptr + 5;
                        while ((*end_ptr != ' ') && (*end_ptr != '=') &&
                                (*end_ptr != '\0'))
                            end_ptr++;
                        exp_ptr = copy_substring(beg_ptr, end_ptr - 2);
                        cut_line = str_ptr;
                        // generate node
                        out_ptr = tprintf("pa_%02d", (int) pai);
                        // Bout_ptr  out_ptr 0  V = v(expr_ptr)
                        ckt_array[pai] = tprintf(
                                "b%s %s 0 v = %s", out_ptr, out_ptr, exp_ptr);
                        ckt_array[++pai] = NULL;
                        // length of the replacement V(out_ptr)
                        del_ptr = copy_ptr = tprintf("v(%s)", out_ptr);
                        // length of the replacement part in original line
                        xlen = strlen(exp_ptr) + 7;
                        // copy the replacement without trailing '\0'
                        for (ii = 0; ii < xlen; ii++)
                            if (*copy_ptr)
                                *cut_line++ = *copy_ptr++;
                            else
                                *cut_line++ = ' ';

                        tfree(del_ptr);
                        tfree(exp_ptr);
                        tfree(out_ptr);
                    }
                    // or we have '={par({ ... })}', the right delimeter is a
                    // ' '
                    else if (ciprefix("={par({", (str_ptr - 2))) {
                        // find expression
                        beg_ptr = end_ptr = str_ptr + 5;
                        while ((*end_ptr != ' ') && (*end_ptr != '\0'))
                            end_ptr++;
                        exp_ptr = copy_substring(beg_ptr, end_ptr - 3);
                        // generate node
                        out_ptr = tprintf("pa_%02d", (int) pai);
                        // Bout_ptr  out_ptr 0  V = v(expr_ptr)
                        ckt_array[pai] = tprintf(
                                "b%s %s 0 v = %s", out_ptr, out_ptr, exp_ptr);
                        ckt_array[++pai] = NULL;
                        // length of the replacement V(out_ptr)
                        del_ptr = copy_ptr = tprintf("v(%s)", out_ptr);
                        // length of the replacement part in original line
                        xlen = strlen(exp_ptr) + 9;
                        // skip '='
                        cut_line++;
                        // copy the replacement without trailing '\0'
                        for (ii = 0; ii < xlen; ii++)
                            if (*copy_ptr)
                                *cut_line++ = *copy_ptr++;
                            else
                                *cut_line++ = ' ';

                        tfree(del_ptr);
                        tfree(exp_ptr);
                        tfree(out_ptr);
                    }
                    else {
                        // nothing to replace
                        cut_line = str_ptr + 1;
                        continue;
                    }

                } // while 'par'
                // no replacement done, go to next line
                if (pai == paui)
                    continue;
                // remove white spaces
                card->line = inp_remove_ws(curr_line);
                // insert new B source line immediately after current line
                for (ii = paui; ii < pai; ii++)
                    card = insert_new_line(card, ckt_array[ii], 0, 0);

                paui = pai;
            }
            else if ((ciprefix(".save", curr_line)) ||
                    (ciprefix(".four", curr_line)) ||
                    (ciprefix(".print", curr_line)) ||
                    (ciprefix(".plot", curr_line))) {
                if (strstr(curr_line, "par(") == NULL)
                    continue;
                cut_line = curr_line;
                // search for 'par('
                while ((str_ptr = strstr(cut_line, "par(")) != NULL) {
                    if (pai > 99) {
                        fprintf(stderr,
                                "ERROR: More than 99 function calls to "
                                "par()\n");
                        fprintf(stderr, "  Limited to 99 per input file\n");
                        controlled_exit(EXIT_FAILURE);
                    }

                    // we have ' par({ ... })'
                    if (ciprefix(" par({", (str_ptr - 1))) {

                        // find expression
                        beg_ptr = end_ptr = str_ptr + 5;
                        while ((*end_ptr != ' ') && (*end_ptr != '\0'))
                            end_ptr++;
                        exp_ptr = copy_substring(beg_ptr, end_ptr - 2);
                        cut_line = str_ptr;
                        // generate node
                        out_ptr = tprintf("pa_%02d", (int) pai);
                        // Bout_ptr  out_ptr 0  V = v(expr_ptr)
                        ckt_array[pai] = tprintf(
                                "b%s %s 0 v = %s", out_ptr, out_ptr, exp_ptr);
                        ckt_array[++pai] = NULL;
                        // length of the replacement V(out_ptr)
                        del_ptr = copy_ptr = tprintf("%s", out_ptr);
                        // length of the replacement part in original line
                        xlen = strlen(exp_ptr) + 7;
                        // copy the replacement without trailing '\0'
                        for (ii = 0; ii < xlen; ii++)
                            if (*copy_ptr)
                                *cut_line++ = *copy_ptr++;
                            else
                                *cut_line++ = ' ';

                        tfree(del_ptr);
                        tfree(exp_ptr);
                        tfree(out_ptr);
                    }
                    // or we have '={par({ ... })}'
                    else if (ciprefix("={par({", (str_ptr - 2))) {

                        // find myoutput
                        beg_ptr = end_ptr = str_ptr - 2;
                        while (*beg_ptr != ' ')
                            beg_ptr--;
                        out_ptr = copy_substring(beg_ptr + 1, end_ptr);
                        cut_line = beg_ptr + 1;
                        // find expression
                        beg_ptr = end_ptr = str_ptr + 5;
                        while ((*end_ptr != ' ') && (*end_ptr != '\0'))
                            end_ptr++;
                        exp_ptr = copy_substring(beg_ptr, end_ptr - 3);
                        // Bout_ptr  out_ptr 0  V = v(expr_ptr)
                        ckt_array[pai] = tprintf(
                                "b%s %s 0 v = %s", out_ptr, out_ptr, exp_ptr);
                        ckt_array[++pai] = NULL;
                        // length of the replacement V(out_ptr)
                        del_ptr = copy_ptr = tprintf("%s", out_ptr);
                        // length of the replacement part in original line
                        xlen = strlen(out_ptr) + strlen(exp_ptr) + 10;
                        // copy the replacement without trailing '\0'
                        for (ii = 0; ii < xlen; ii++)
                            if (*copy_ptr)
                                *cut_line++ = *copy_ptr++;
                            else
                                *cut_line++ = ' ';

                        tfree(del_ptr);
                        tfree(exp_ptr);
                        tfree(out_ptr);
                    }
                    // nothing to replace
                    else
                        cut_line = str_ptr + 1;
                } // while 'par('
                // no replacement done, go to next line
                if (pai == paui)
                    continue;
                // remove white spaces
                card->line = inp_remove_ws(curr_line);
                // comment out current variable capacitor line
                // *(ckt_array[0]) = '*';
                // insert new B source line immediately after current line
                for (ii = paui; ii < pai; ii++)
                    card = insert_new_line(card, ckt_array[ii], 0, 0);

                paui = pai;
                // continue;
            } // if .print etc.
        } // if ('.')
    }
}


/* replace a token (length 4 char) in string by spaces, if it is found
   at the correct position and the total number of tokens is o.k. */

static void replace_token(
        char *string, char *token, int wherereplace, int total)
{
    int count = 0, i;
    char *actstring = string;

    /* token to be replaced not in string */
    if (strstr(string, token) == NULL)
        return;

    /* get total number of tokens */
    while (*actstring) {
        actstring = nexttok(actstring);
        count++;
    }
    /* If total number of tokens correct */
    if (count == total) {
        actstring = string;
        for (i = 1; i < wherereplace; i++)
            actstring = nexttok(actstring);
        /* If token to be replaced at right position */
        if (ciprefix(token, actstring)) {
            actstring[0] = ' ';
            actstring[1] = ' ';
            actstring[2] = ' ';
            actstring[3] = ' ';
        }
    }
}


/* lines for B sources (except for pwl lines): no parsing in numparam code,
   just replacement of parameters. pwl lines are still handled in numparam.
   Parsing for all other B source lines are done in the B source parser.
   To achive this, do the following:
   Remove all '{' and '}' --> no parsing of equations in numparam
   Place '{' and '}' directly around all potential parameters,
   but skip function names like exp (search for 'exp(' to detect fcn name),
   functions containing nodes like v(node), v(node1, node2), i(branch)
   and other keywords like TEMPER. --> Only parameter replacement in numparam
*/

static void inp_bsource_compat(struct card *card)
{
    char *equal_ptr, *str_ptr, *new_str, *final_str;
    int skip_control = 0;

    for (; card; card = card->nextcard) {

        char *curr_line = card->line;

        /* exclude any command inside .control ... .endc */
        if (ciprefix(".control", curr_line)) {
            skip_control++;
            continue;
        }
        else if (ciprefix(".endc", curr_line)) {
            skip_control--;
            continue;
        }
        else if (skip_control > 0) {
            continue;
        }

        if (*curr_line == 'b') {
            /* remove white spaces of everything inside {}*/
            card->line = inp_remove_ws(card->line);
            curr_line = card->line;
            /* exclude special pwl lines */
            if (strstr(curr_line, "=pwl("))
                continue;
            /* store starting point for later parsing, beginning of
             * {expression} */
            equal_ptr = strchr(curr_line, '=');
            /* check for errors */
            if (equal_ptr == NULL) {
                fprintf(stderr, "ERROR: mal formed B line: %s\n", curr_line);
                controlled_exit(EXIT_FAILURE);
            }
            /* find the m={m} token and remove it */
            if ((str_ptr = strstr(curr_line, "m={m}")) != NULL)
                memcpy(str_ptr, "     ", 5);
            new_str = inp_modify_exp(equal_ptr + 1);
            final_str = tprintf("%.*s %s", (int) (equal_ptr + 1 - curr_line),
                    curr_line, new_str);

            // comment out current line (old B source line)
            *(card->line) = '*';
            // insert new B source line immediately after current line
            /* Copy old line numbers into new B source line */
            card = insert_new_line(
                    card, final_str, card->linenum, card->linenum_orig);

            tfree(new_str);
        } /* end of if 'b' */
    } /* end of for loop */
}


/* Find all expressions containing the keyword 'temper',
 * except for B lines and some other exclusions. Prepare
 * these expressions by calling inp_modify_exp() and return
 * a modified card->line
 */

static bool inp_temper_compat(struct card *card)
{
    int skip_control = 0;
    char *beg_str, *end_str, *beg_tstr, *end_tstr, *exp_str;

    bool with_temper = FALSE;
    for (; card; card = card->nextcard) {

        char *new_str = NULL;
        char *curr_line = card->line;

        if (curr_line == NULL)
            continue;
        /* exclude any command inside .control ... .endc */
        if (ciprefix(".control", curr_line)) {
            skip_control++;
            continue;
        }
        else if (ciprefix(".endc", curr_line)) {
            skip_control--;
            continue;
        }
        else if (skip_control > 0) {
            continue;
        }
        /* exclude some elements */
        if (strchr("*vbiegfh", curr_line[0]))
            continue;
        /* exclude all dot commands except .model */
        if (curr_line[0] == '.' && !prefix(".model", curr_line))
            continue;
        /* exclude lines not containing 'temper' */
        if (!strstr(curr_line, "temper"))
            continue;
        /* now start processing of the remaining lines containing 'temper' */
        /* remove white spaces of everything inside {}*/
        card->line = inp_remove_ws(card->line);
        curr_line = card->line;

        beg_str = beg_tstr = curr_line;
        while ((beg_tstr = search_identifier(
                        beg_tstr, "temper", curr_line)) != NULL) {
            char *modified_exp;
            /* set the global variable */
            with_temper = TRUE;
            /* find the expression: first go back to the opening '{',
               then find the closing '}' */
            while ((*beg_tstr) != '{')
                beg_tstr--;
            end_str = end_tstr = beg_tstr;
            exp_str = gettok_char(&end_tstr, '}', TRUE, TRUE);
            /* modify the expression string */
            modified_exp = inp_modify_exp(exp_str);
            tfree(exp_str);
            /* add the intermediate string between previous and next
             * expression to the new line */
            new_str =
                    INPstrCat(new_str, ' ', copy_substring(beg_str, end_str));
            /* add the modified expression string to the new line */
            new_str = INPstrCat(new_str, ' ', modified_exp);
            new_str = INPstrCat(new_str, ' ', copy(" "));
            /* move on to the next intermediate string */
            beg_str = beg_tstr = end_tstr;
        }
        if (*beg_str)
            new_str = INPstrCat(new_str, ' ', copy(beg_str));
        tfree(card->line);
        card->line = inp_remove_ws(new_str);
    }
    return with_temper;
}


/* lines containing expressions with keyword 'temper':
 * no parsing in numparam code, just replacement of parameters.
 * Parsing done with B source parser in function inp_parse_temper
 * in inp.c. Evaluation is the done with fcn inp_evaluate_temper
 * from inp.c, taking the actual temperature into account.
 * To achive this, do the following here:
 * Remove all '{' and '}' --> no parsing of equations in numparam
 * Place '{' and '}' directly around all potential parameters,
 * but skip function names like exp (search for 'exp(' to detect fcn name),
 * functions containing nodes like v(node), v(node1, node2), i(branch)
 * and other keywords like TEMPER. --> Only parameter replacement in numparam
 */

static char *inp_modify_exp(char *expr)
{
    char *s;
    wordlist *wl = NULL, *wlist = NULL;

    /* scan the expression and remove all '{' and '}' */
    for (s = expr; *s; s++)
        if ((*s == '{') || (*s == '}'))
            *s = ' ';

    /* scan the expression */
    s = expr;
    while (*(s = skip_ws(s))) {

        static bool c_arith_prev = FALSE;
        bool c_arith = FALSE;
        char c_prev = '\0';
        char c = *s;

        wl_append_word(&wlist, &wl, NULL);

        if ((c == ',') || (c == '(') || (c == ')') || (c == '*') ||
                (c == '/') || (c == '^') || (c == '+') || (c == '?') ||
                (c == ':') || (c == '-')) {
            if ((c == '*') && (s[1] == '*')) {
                wl->wl_word = tprintf("**");
                s += 2;
            }
            else if (c == '-' && c_arith_prev && c_prev != ')') {
                /* enter whole number string if '-' is a sign */
                int error1;
                /* allow 100p, 5MEG etc. */
                double dvalue = INPevaluate(&s, &error1, 0);
                if (error1) {
                    wl->wl_word = tprintf("%c", c);
                    s++;
                }
                else {
                    wl->wl_word = tprintf("%18.10e", dvalue);
                    /* skip the `unit', FIXME INPevaluate() should do this */
                    while (isalpha_c(*s))
                        s++;
                }
            }
            else {
                wl->wl_word = tprintf("%c", c);
                s++;
            }
            c_arith = TRUE;
        }
        else if ((c == '>') || (c == '<') || (c == '!') || (c == '=')) {
            /* >=, <=, !=, ==, <>, ... */
            char *beg = s++;
            if ((*s == '=') || (*s == '<') || (*s == '>'))
                s++;
            wl->wl_word = copy_substring(beg, s);
        }
        else if ((c == '|') || (c == '&')) {
            char *beg = s++;
            if ((*s == '|') || (*s == '&'))
                s++;
            wl->wl_word = copy_substring(beg, s);
        }
        else if (isalpha_c(c) || c == '_') {

            char buf[512];
            int i = 0;

            if (((c == 'v') || (c == 'i')) && (s[1] == '(')) {
                while (*s != ')')
                    buf[i++] = *s++;
                buf[i++] = *s++;
                buf[i] = '\0';
                wl->wl_word = copy(buf);
            }
            else {
                while (isalnum_c(*s) || (*s == '!') || (*s == '#') ||
                        (*s == '$') || (*s == '%') || (*s == '_') ||
                        (*s == '[') || (*s == ']')) {
                    buf[i++] = *s++;
                }
                buf[i] = '\0';
                /* no parens {} around time, hertz, temper, the constants
                   pi and e which are defined in inpptree.c, around pwl and
                   temp. coeffs */
                if ((*s == '(') || cieq(buf, "hertz") ||
                        cieq(buf, "temper") || cieq(buf, "time") ||
                        cieq(buf, "pi") || cieq(buf, "e") ||
                        cieq(buf, "pwl")) {
                    wl->wl_word = copy(buf);
                }
                else if (cieq(buf, "tc1") || cieq(buf, "tc2") ||
                        cieq(buf, "reciproctc")) {
                    s = skip_ws(s);
                    /* no {} around tc1 = or tc2 = , these are temp coeffs. */
                    if (s[0] == '=' && s[1] != '=') {
                        buf[i++] = '=';
                        buf[i] = '\0';
                        s++;
                        wl->wl_word = copy(buf);
                    }
                    else {
                        wl->wl_word = tprintf("({%s})", buf);
                    }
                    /* '-' following the '=' is attached to number as its sign
                     */
                    c_arith = TRUE;
                }
                else {
                    /* {} around all other tokens */
                    wl->wl_word = tprintf("({%s})", buf);
                }
            }
        }
        else if (isdigit_c(c) || (c == '.')) { /* allow .5 format too */
            int error1;
            /* allow 100p, 5MEG etc. */
            double dvalue = INPevaluate(&s, &error1, 0);
            wl->wl_word = tprintf("%18.10e", dvalue);
            /* skip the `unit', FIXME INPevaluate() should do this */
            while (isalpha_c(*s))
                s++;
        }
        else { /* strange char */
            printf("Preparing expression for numparam\nWhat is this?\n%s\n",
                    s);
            wl->wl_word = tprintf("%c", *s++);
        }
        c_prev = c;
        c_arith_prev = c_arith;
    }

    expr = wl_flatten(wlist);
    wl_free(wlist);

    return expr;
}


/*
 * destructively fetch a token from the input string
 *   token is either quoted, or a plain nonwhitespace sequence
 * function will return the place from where to continue
 */

static char *get_quoted_token(char *string, char **token)
{
    char *s = skip_ws(string);

    if (!*s) /* nothing found */
        return string;

    if (isquote(*s)) {

        char *t = ++s;

        while (*t && !isquote(*t))
            t++;

        if (!*t) { /* teriminator quote not found */
            *token = NULL;
            return string;
        }

        *t++ = '\0';

        *token = s;
        return t;
    }
    else {

        char *t = skip_non_ws(s);

        if (t == s) { /* nothing found */
            *token = NULL;
            return string;
        }

        if (*t)
            *t++ = '\0';

        *token = s;
        return t;
    }
}


/* Option RSERIES=rval
 * Lxxx n1 n2 Lval
 * -->
 * Lxxx n1 n2_intern__ Lval
 * RLxxx_n2_intern__ n2_intern__ n2 rval
 */

static void inp_add_series_resistor(struct card *deck)
{
    int skip_control = 0;
    struct card *card;
    char *rval = NULL;

    for (card = deck; card; card = card->nextcard) {
        char *curr_line = card->line;
        if (*curr_line != '*' && strstr(curr_line, "option")) {
            char *t = strstr(curr_line, "rseries");
            if (t) {
                tfree(rval);

                t += 7;
                if (*t++ == '=')
                    rval = gettok(&t);

                /* default to "1e-3" if no value given */
                if (!rval)
                    rval = copy("1e-3");
            }
        }
    }

    if (!rval)
        return;

    fprintf(stdout,
            "\nOption rseries given: \n"
            "resistor %s Ohms added in series to each inductor L\n\n",
            rval);

    for (card = deck; card; card = card->nextcard) {
        char *cut_line = card->line;

        /* exclude any command inside .control ... .endc */
        if (ciprefix(".control", cut_line)) {
            skip_control++;
            continue;
        }
        else if (ciprefix(".endc", cut_line)) {
            skip_control--;
            continue;
        }
        else if (skip_control > 0) {
            continue;
        }

        if (ciprefix("l", cut_line)) {

            char *title_tok = gettok(&cut_line);
            char *node1 = gettok(&cut_line);
            char *node2 = gettok(&cut_line);

            /* new L line and new R line */
            char *newL = tprintf("%s %s %s_intern__ %s", title_tok, node1,
                    title_tok, cut_line);
            char *newR = tprintf("R%s_intern__ %s_intern__ %s %s", title_tok,
                    title_tok, node2, rval);

            // comment out current L line
            *(card->line) = '*';

            // insert new new L and R lines immediately after current line
            card = insert_new_line(card, newL, 0, 0);
            card = insert_new_line(card, newR, 0, 0);

            tfree(title_tok);
            tfree(node1);
            tfree(node2);
        }
    }

    tfree(rval);
}


/*
 * rewrite
 *   .subckt node1 node2 node3 name params: l={x} w={y}
 * to
 *   .subckt node1 node2 node3 name
 *   .param l={x} w={y}
 */

static void subckt_params_to_param(struct card *card)
{
    for (; card; card = card->nextcard) {
        char *curr_line = card->line;
        if (ciprefix(".subckt", curr_line)) {
            char *cut_line, *new_line;
            cut_line = strstr(curr_line, "params:");
            if (!cut_line)
                continue;
            /* new_line starts with "params: " */
            new_line = copy(cut_line);
            /* replace "params:" by ".param " */
            memcpy(new_line, ".param ", 7);
            /* card->line ends with subcircuit name */
            cut_line[-1] = '\0';
            /* insert new_line after card->line */
            insert_new_line(card, new_line, card->linenum + 1, 0);
        }
    }
}


/* If XSPICE option is not selected, run this function to alert and exit
   if the 'poly' option is found in e, g, f, or h controlled sources. */

#ifndef XSPICE

static void inp_poly_err(struct card *card)
{
    size_t skip_control = 0;

    for (; card; card = card->nextcard) {

        char *curr_line = card->line;

        if (*curr_line == '*')
            continue;

        /* exclude any command inside .control ... .endc */
        if (ciprefix(".control", curr_line)) {
            skip_control++;
            continue;
        }
        else if (ciprefix(".endc", curr_line)) {
            skip_control--;
            continue;
        }
        else if (skip_control > 0) {
            continue;
        }

        /* get the fourth token in a controlled source line and exit,
           if it is 'poly' */
        if ((ciprefix("e", curr_line)) || (ciprefix("g", curr_line)) ||
                (ciprefix("f", curr_line)) || (ciprefix("h", curr_line))) {
            curr_line = nexttok(curr_line);
            curr_line = nexttok(curr_line);
            curr_line = nexttok(curr_line);
            if (ciprefix("poly", curr_line)) {
                fprintf(stderr,
                        "\nError: XSPICE is required to run the 'poly' "
                        "option in line %d\n",
                        card->linenum_orig);
                fprintf(stderr, "  %s\n", card->line);
                fprintf(stderr,
                        "\nSee manual chapt. 31 for installation "
                        "instructions\n");
                controlled_exit(EXIT_BAD);
            }
        }
    }
}

#endif


/* Used for debugging. You may add
 *   tprint(working);
 * somewhere in function inp_readall() of this file to have
 *   a printout of the actual deck written to file "tprint-out.txt" */
void tprint(struct card *t)
{
    struct card *tmp;

    /*debug: print into file*/
    FILE *fd = fopen("tprint-out.txt", "w");
    for (tmp = t; tmp; tmp = tmp->nextcard)
        if (*(tmp->line) != '*')
            fprintf(fd, "%6d  %6d  %s\n", tmp->linenum_orig, tmp->linenum,
                    tmp->line);
    fprintf(fd,
            "\n**************************************************************"
            "*******************\n");
    fprintf(fd,
            "****************************************************************"
            "*****************\n");
    fprintf(fd,
            "****************************************************************"
            "*****************\n\n");
    for (tmp = t; tmp; tmp = tmp->nextcard)
        fprintf(fd, "%6d  %6d  %s\n", tmp->linenum_orig, tmp->linenum,
                tmp->line);
    fprintf(fd,
            "\n**************************************************************"
            "*******************\n");
    fprintf(fd,
            "****************************************************************"
            "*****************\n");
    fprintf(fd,
            "****************************************************************"
            "*****************\n\n");
    for (tmp = t; tmp; tmp = tmp->nextcard)
        if (*(tmp->line) != '*')
            fprintf(fd, "%s\n", tmp->line);
    fclose(fd);
}


/* prepare .if and .elseif for numparam
   .if(expression) --> .if{expression} */

static void inp_dot_if(struct card *card)
{
    for (; card; card = card->nextcard) {

        char *curr_line = card->line;

        if (*curr_line == '*')
            continue;

        if (ciprefix(".if", curr_line) || ciprefix(".elseif", curr_line)) {
            char *firstbr = strchr(curr_line, '(');
            char *lastbr = strrchr(curr_line, ')');
            if ((!firstbr) || (!lastbr)) {
                fprintf(cp_err, "Error in netlist line %d\n",
                        card->linenum_orig);
                fprintf(cp_err, "   Bad syntax: %s\n\n", curr_line);
                controlled_exit(EXIT_BAD);
            }
            *firstbr = '{';
            *lastbr = '}';
        }
    }
}


/* Convert .param lines containing keyword 'temper' into .func lines:
 * .param xxx1 = 'temper + 25'  --->  .func xxx1() 'temper + 25'
 * Add info about the functions (name, subcircuit depth, number of
 * subckt) to linked list new_func.
 * Then scan new_func, for each xxx1 scan all lines of deck,
 * find all xxx1 and convert them to a function:
 * xxx1   --->  xxx1()
 * If this happens to be in another .param line, convert it to .func,
 * add info to end of new_func and continue scanning.
 */

static char *inp_functionalise_identifier(char *curr_line, char *identifier);

static void inp_fix_temper_in_param(struct card *deck)
{
    int skip_control = 0, subckt_depth = 0, j, *sub_count;
    char *funcbody, *funcname;
    struct func_temper *f, *funcs = NULL, **funcs_tail_ptr = &funcs;
    struct card *card;

    sub_count = TMALLOC(int, 16);
    for (j = 0; j < 16; j++)
        sub_count[j] = 0;

    /* first pass: determine all .param with temper inside and replace by
       .func .param xxx1 = 'temper + 25' will become .func xxx1() 'temper +
       25'
    */
    card = deck;
    for (; card; card = card->nextcard) {

        char *curr_line = card->line;

        if (*curr_line == '*')
            continue;

        /* determine nested depths of subcircuits */
        if (ciprefix(".subckt", curr_line)) {
            subckt_depth++;
            sub_count[subckt_depth]++;
            continue;
        }
        else if (ciprefix(".ends", curr_line)) {
            subckt_depth--;
            continue;
        }

        /* exclude any command inside .control ... .endc */
        if (ciprefix(".control", curr_line)) {
            skip_control++;
            continue;
        }
        else if (ciprefix(".endc", curr_line)) {
            skip_control--;
            continue;
        }
        else if (skip_control > 0) {
            continue;
        }

        if (ciprefix(".para", curr_line)) {

            char *p, *temper, *equal_ptr, *lhs_b, *lhs_e;

            temper = search_identifier(curr_line, "temper", curr_line);

            if (!temper)
                continue;

            equal_ptr = find_assignment(curr_line);

            if (!equal_ptr) {
                fprintf(stderr,
                        "ERROR: could not find '=' on parameter line '%s'!\n",
                        curr_line);
                controlled_exit(EXIT_FAILURE);
            }

            /* .param lines with `,' separated multiple parameters
             *    must have been split in inp_split_multi_param_lines()
             */

            if (find_assignment(equal_ptr + 1)) {
                fprintf(stderr, "ERROR: internal error on line '%s'!\n",
                        curr_line);
                controlled_exit(EXIT_FAILURE);
            }

            lhs_b = skip_non_ws(curr_line); // eat .param
            lhs_b = skip_ws(lhs_b);

            lhs_e = skip_back_ws(equal_ptr, curr_line);

            /* skip if this is a function already */
            p = strpbrk(lhs_b, "(,)");
            if (p && p < lhs_e)
                continue;

            if (temper < equal_ptr) {
                fprintf(stderr,
                        "Error: you cannot assign a value to TEMPER\n"
                        "  Line no. %d, %s\n",
                        card->linenum, curr_line);
                controlled_exit(EXIT_BAD);
            }

            funcname = copy_substring(lhs_b, lhs_e);
            funcbody = copy(equal_ptr + 1);

            *funcs_tail_ptr = inp_new_func(
                    funcname, funcbody, card, sub_count, subckt_depth);
            funcs_tail_ptr = &(*funcs_tail_ptr)->next;

            tfree(funcbody);
        }
    }

    /* second pass */
    /* for each .func entry in `funcs' start the insertion operation:
       search each line from the deck which has the suitable subcircuit
       nesting data. for tokens xxx equalling the funcname, replace xxx by
       xxx(). if the replacement is done in a .param line then convert it to a
       .func line and append an entry to `funcs'. Continue up to the very end
       of `funcs'.
     */

    for (f = funcs; f; f = f->next) {

        for (j = 0; j < 16; j++)
            sub_count[j] = 0;

        card = deck;
        for (; card; card = card->nextcard) {

            char *new_str = NULL; /* string we assemble here */
            char *curr_line = card->line;
            char *firsttok_str;

            if (*curr_line == '*')
                continue;

            /* determine nested depths of subcircuits */
            if (ciprefix(".subckt", curr_line)) {
                subckt_depth++;
                sub_count[subckt_depth]++;
                continue;
            }
            else if (ciprefix(".ends", curr_line)) {
                subckt_depth--;
                continue;
            }

            /* exclude any command inside .control ... .endc */
            if (ciprefix(".control", curr_line)) {
                skip_control++;
                continue;
            }
            else if (ciprefix(".endc", curr_line)) {
                skip_control--;
                continue;
            }
            else if (skip_control > 0) {
                continue;
            }

            /* exclude lines which do not have the same subcircuit
               nesting depth and number as found in f */
            if (subckt_depth != f->subckt_depth)
                continue;
            if (sub_count[subckt_depth] != f->subckt_count)
                continue;

            /* remove first token, ignore it here, restore it later */
            firsttok_str = gettok(&curr_line);
            if (*curr_line == '\0') {
                tfree(firsttok_str);
                continue;
            }

            new_str = inp_functionalise_identifier(curr_line, f->funcname);

            if (new_str == curr_line) {
                tfree(firsttok_str);
                continue;
            }

            /* restore first part of the line */
            new_str = INPstrCat(firsttok_str, ' ', new_str);
            new_str = inp_remove_ws(new_str);

            /* if we have inserted into a .param line, convert to .func */
            if (prefix(".para", new_str)) {
                char *new_tmp_str = new_str;
                new_tmp_str = nexttok(new_tmp_str);
                funcname = gettok_char(&new_tmp_str, '=', FALSE, FALSE);
                funcbody = copy(new_tmp_str + 1);
                *funcs_tail_ptr = inp_new_func(
                        funcname, funcbody, card, sub_count, subckt_depth);
                funcs_tail_ptr = &(*funcs_tail_ptr)->next;
                tfree(new_str);
                tfree(funcbody);
            }
            else {
                /* Or just enter new line into deck */
                insert_new_line(card, new_str, 0, card->linenum);
                *card->line = '*';
            }
        }
    }

    /* final memory clearance */
    tfree(sub_count);
    inp_delete_funcs(funcs);
}


/* Convert .param lines containing function 'agauss' and others
 *  (function name handed over by *fcn),  into .func lines:
 * .param xxx1 = 'aunif()'  --->  .func xxx1() 'aunif()'
 * Add info about the functions (name, subcircuit depth, number of
 * subckt) to linked list new_func.
 * Then scan new_func, for each xxx1 scan all lines of deck,
 * find all xxx1 and convert them to a function:
 * xxx1   --->  xxx1()
 *
 * In a second step, after subcircuits have been expanded, all occurencies
 * of agauss in a b-line are replaced by their suitable value (function
 * eval_agauss() in inp.c).
 */

static void inp_fix_agauss_in_param(struct card *deck, char *fcn)
{
    int skip_control = 0, subckt_depth = 0, j, *sub_count;
    char *funcbody, *funcname;
    struct func_temper *f, *funcs = NULL, **funcs_tail_ptr = &funcs;
    struct card *card;

    sub_count = TMALLOC(int, 16);
    for (j = 0; j < 16; j++)
        sub_count[j] = 0;

    /* first pass:
     *   determine all .param with agauss inside and replace by .func
     *   convert
     *     .param xxx1 = 'agauss(x,y,z) * 25'
     *   to
     *     .func xxx1() 'agauss(x,y,z) * 25'
     */
    card = deck;
    for (; card; card = card->nextcard) {

        char *curr_line = card->line;

        if (*curr_line == '*')
            continue;

        /* determine nested depths of subcircuits */
        if (ciprefix(".subckt", curr_line)) {
            subckt_depth++;
            sub_count[subckt_depth]++;
            continue;
        }
        else if (ciprefix(".ends", curr_line)) {
            subckt_depth--;
            continue;
        }

        /* exclude any command inside .control ... .endc */
        if (ciprefix(".control", curr_line)) {
            skip_control++;
            continue;
        }
        else if (ciprefix(".endc", curr_line)) {
            skip_control--;
            continue;
        }
        else if (skip_control > 0) {
            continue;
        }

        if (ciprefix(".para", curr_line)) {

            char *p, *temper, *equal_ptr, *lhs_b, *lhs_e;

            temper = search_identifier(curr_line, fcn, curr_line);

            if (!temper)
                continue;

            equal_ptr = find_assignment(curr_line);

            if (!equal_ptr) {
                fprintf(stderr,
                        "ERROR: could not find '=' on parameter line '%s'!\n",
                        curr_line);
                controlled_exit(EXIT_FAILURE);
            }

            /* .param lines with `,' separated multiple parameters
             *   must have been split in inp_split_multi_param_lines()
             */

            if (find_assignment(equal_ptr + 1)) {
                fprintf(stderr, "ERROR: internal error on line '%s'!\n",
                        curr_line);
                controlled_exit(EXIT_FAILURE);
            }

            lhs_b = skip_non_ws(curr_line); // eat .param
            lhs_b = skip_ws(lhs_b);

            lhs_e = skip_back_ws(equal_ptr, curr_line);

            /* skip if this is a function already */
            p = strpbrk(lhs_b, "(,)");
            if (p && p < lhs_e)
                continue;

            if (temper < equal_ptr) {
                fprintf(stderr,
                        "Error: you cannot assign a value to %s\n"
                        "  Line no. %d, %s\n",
                        fcn, card->linenum, curr_line);
                controlled_exit(EXIT_BAD);
            }

            funcname = copy_substring(lhs_b, lhs_e);
            funcbody = copy(equal_ptr + 1);

            *funcs_tail_ptr = inp_new_func(
                    funcname, funcbody, card, sub_count, subckt_depth);
            funcs_tail_ptr = &(*funcs_tail_ptr)->next;

            tfree(funcbody);
        }
    }

    /* second pass:
     *   for each .func entry in `funcs' start the insertion operation:
     *      search each line from the deck which has the suitable
     *      subcircuit nesting data.
     *   for tokens xxx equalling the funcname, replace xxx by xxx().
     */

    for (f = funcs; f; f = f->next) {

        for (j = 0; j < 16; j++)
            sub_count[j] = 0;

        card = deck;
        for (; card; card = card->nextcard) {

            char *new_str = NULL; /* string we assemble here */
            char *curr_line = card->line;
            char *firsttok_str;

            if (*curr_line == '*')
                continue;

            /* determine nested depths of subcircuits */
            if (ciprefix(".subckt", curr_line)) {
                subckt_depth++;
                sub_count[subckt_depth]++;
                continue;
            }
            else if (ciprefix(".ends", curr_line)) {
                subckt_depth--;
                continue;
            }

            /* exclude any command inside .control ... .endc */
            if (ciprefix(".control", curr_line)) {
                skip_control++;
                continue;
            }
            else if (ciprefix(".endc", curr_line)) {
                skip_control--;
                continue;
            }
            else if (skip_control > 0) {
                continue;
            }

            /* if function is not at top level,
               exclude lines which do not have the same subcircuit
               nesting depth and number as found in f */
            if (f->subckt_depth > 0) {
                if (subckt_depth != f->subckt_depth)
                    continue;
                if (sub_count[subckt_depth] != f->subckt_count)
                    continue;
            }

            /* remove first token, ignore it here, restore it later */
            firsttok_str = gettok(&curr_line);
            if (*curr_line == '\0') {
                tfree(firsttok_str);
                continue;
            }

            new_str = inp_functionalise_identifier(curr_line, f->funcname);

            if (new_str == curr_line) {
                tfree(firsttok_str);
                continue;
            }

            /* restore first part of the line */
            new_str = INPstrCat(firsttok_str, ' ', new_str);
            new_str = inp_remove_ws(new_str);

            *card->line = '*';
            /* Enter new line into deck */
            insert_new_line(card, new_str, 0, card->linenum);
        }
    }
    /* final memory clearance */
    tfree(sub_count);
    inp_delete_funcs(funcs);
}


/* append "()" to each 'identifier' in 'curr_line',
 *   unless already there */
static char *inp_functionalise_identifier(char *curr_line, char *identifier)
{
    size_t len = strlen(identifier);
    char *p, *str = curr_line;

    for (p = str; (p = search_identifier(p, identifier, str)) != NULL;)
        if (p[len] != '(') {
            int prefix_len = (int) (p + len - str);
            char *x = str;
            str = tprintf("%.*s()%s", prefix_len, str, str + prefix_len);
            if (x != curr_line)
                tfree(x);
            p = str + prefix_len + 2;
        }
        else {
            p++;
        }

    return str;
}


/* enter function name, nested .subckt depths, and
 * number of .subckt at given level into struct new_func
 * and add line to deck
 */

static struct func_temper *inp_new_func(char *funcname, char *funcbody,
        struct card *card, int *sub_count, int subckt_depth)
{
    struct func_temper *f;
    char *new_str;

    f = TMALLOC(struct func_temper, 1);
    f->funcname = funcname;
    f->next = NULL;
    f->subckt_depth = subckt_depth;
    f->subckt_count = sub_count[subckt_depth];

    /* replace line in deck */
    new_str = tprintf(".func %s() %s", funcname, funcbody);

    *card->line = '*';
    insert_new_line(card, new_str, 0, card->linenum);

    return f;
}


static void inp_delete_funcs(struct func_temper *f)
{
    while (f) {
        struct func_temper *f_next = f->next;
        tfree(f->funcname);
        tfree(f);
        f = f_next;
    }
}


/* look for unquoted parameters and quote them */
/* FIXME, this function seems to be useless and/or buggy and/or naive */
static void inp_quote_params(struct card *c, struct card *end_c,
        struct dependency *deps, int num_params)
{
    bool in_control = FALSE;

    for (; c && c != end_c; c = c->nextcard) {

        int i, j, num_terminals;

        char *curr_line = c->line;

        if (ciprefix(".control", curr_line)) {
            in_control = TRUE;
            continue;
        }

        if (ciprefix(".endc", curr_line)) {
            in_control = FALSE;
            continue;
        }

        if (in_control || curr_line[0] == '.' || curr_line[0] == '*')
            continue;

        num_terminals = get_number_terminals(curr_line);

        if (num_terminals <= 0)
            continue;

        /* There are devices that should not get quotes around tokens
           following after the terminals. See bug 384 */
        if (curr_line[0] == 'f' || curr_line[0] == 'h')
            num_terminals++;

        for (i = 0; i < num_params; i++) {

            char *s = curr_line;

            for (j = 0; j < num_terminals + 1; j++) {
                s = skip_non_ws(s);
                s = skip_ws(s);
            }

            while ((s = ya_search_identifier(
                            s, deps[i].param_name, curr_line)) != NULL) {

                char *rest = s + strlen(deps[i].param_name);

                if (s > curr_line && (isspace_c(s[-1]) || s[-1] == '=') &&
                        (isspace_c(*rest) || *rest == '\0' || *rest == ')')) {
                    int prefix_len;

                    if (isspace_c(s[-1])) {
                        s = skip_back_ws(s, curr_line);
                        if (s > curr_line && s[-1] == '{')
                            s--;
                    }

                    if (isspace_c(*rest)) {
                        /* possible case: "{  length }" -> {length} */
                        rest = skip_ws(rest);
                        if (*rest == '}')
                            rest++;
                        else
                            rest--;
                    }

                    prefix_len = (int) (s - curr_line);

                    curr_line = tprintf("%.*s {%s}%s", prefix_len, curr_line,
                            deps[i].param_name, rest);
                    s = curr_line + prefix_len + strlen(deps[i].param_name) +
                            3;

                    tfree(c->line);
                    c->line = curr_line;
                }
                else {
                    s += strlen(deps[i].param_name);
                }
            }
        }
    }
}


/* VDMOS special:
   Check for 'vdmos' in .model line.
   check if 'pchan', then add p to vdmos and ignore 'pchan'.
   If no 'pchan' is found, add n to vdmos.
   Ignore annotations on Vds, Ron, Qg, and mfg.
   Assemble all other tokens in a wordlist, and flatten it
   to become the new .model line.
*/
static void inp_vdmos_model(struct card *deck)
{
    struct card *card;
    for (card = deck; card; card = card->nextcard) {

        char *curr_line, *cut_line, *token, *new_line;
        wordlist *wl = NULL, *wlb;

        curr_line = cut_line = card->line;

        if (ciprefix(".model", curr_line) && strstr(curr_line, "vdmos")) {
            cut_line = strstr(curr_line, "vdmos");
            wl_append_word(&wl, &wl, copy_substring(curr_line, cut_line));
            wlb = wl;
            if (strstr(cut_line, "pchan")) {
                wl_append_word(NULL, &wl, "vdmosp (");
            }
            else {
                wl_append_word(NULL, &wl, "vdmosn (");
            }
            cut_line = cut_line + 5;

            cut_line = skip_ws(cut_line);
            if (*cut_line == '(')
                cut_line = cut_line + 1;
            new_line = NULL;
            while (cut_line && *cut_line) {
                token = gettok_model(&cut_line);
                if (!ciprefix("pchan", token) && !ciprefix("ron=", token) &&
                        !ciprefix("vds=", token) && !ciprefix("qg=", token) &&
                        !ciprefix("mfg=", token) && !ciprefix("nchan", token))
                    wl_append_word(NULL, &wl, token);
                if (*cut_line == ')') {
                    wl_append_word(NULL, &wl, ")");
                    break;
                }
            }
            new_line = wl_flatten(wlb);
            tfree(card->line);
            card->line = new_line;
        }
    }
}


/* storage for devices which get voltage source added */
struct replace_currm {
    struct card *s_start;
    struct card *cline;
    char *rtoken;
    struct replace_currm *next;
};

/* check if fourth token of sname starts with POLY */
static bool is_poly_source(char *sname)
{
    char *nstr = nexttok(sname);
    nstr = nexttok(nstr);
    nstr = nexttok(nstr);
    if (ciprefix("POLY", nstr))
        return TRUE;
    else
        return FALSE;
}

/* Measure current in node 1 of all devices, e.g. I, B, F, G.
   I(V...) will be ignored, I(E...) and I(H...) will be undone if
   they are simple linear sources, however E nonlinear voltage
   source will be converted later to B source,
   therefore we need to add current measurement here.
   First find all ocurrencies of i(XYZ), store their cards, then
   search for XYZ, but only within respective subcircuit, or if
   all happens at top level. Other hierarchy is ignored for now.
   Replace I(XYZ) bx I(V_XYZ), add voltage source V_XYZ with
   suitable extra nodes.
*/
static void inp_meas_current(struct card *deck)
{
    struct card *card, *subc_start = NULL, *subc_prev = NULL;
    struct replace_currm *new_rep, *act_rep = NULL, *rep = NULL;
    char *s, *t, *u, *v, *w;
    int skip_control = 0, subs = 0, sn = 0;

    /* scan through deck and find i(xyz), replace by i(v_xyz) */
    for (card = deck; card; card = card->nextcard) {

        char *curr_line = card->line;

        /* exclude any command inside .control ... .endc */
        if (ciprefix(".control", curr_line)) {
            skip_control++;
            continue;
        }
        else if (ciprefix(".endc", curr_line)) {
            skip_control--;
            continue;
        }
        else if (skip_control > 0) {
            continue;
        }

        if (*curr_line == '*')
            continue;

        if (*curr_line == '.') {
            if (ciprefix(".subckt", curr_line)) {
                subs++;
                subc_prev = subc_start;
                subc_start = card;
            }
            else if (ciprefix(".ends", curr_line)) {
                subs--;
                subc_start = subc_prev;
            }
            else
                continue;
        }

        if (!strstr(curr_line, "i("))
            continue;

        s = v = w = stripWhiteSpacesInsideParens(curr_line);
        while (s) {
            /* i( may occur more than once in a line */
            s = u = strstr(s, "i(");
            /* we have found it, but not (in error) at the beginning of the
             * line */
            if (s && s > v) {
                /* '{' if at beginning of expression, '=' possible in B-line
                 */
                if (is_arith_char(s[-1]) || s[-1] == '{' || s[-1] == '=' ||
                        isspace_c(s[-1])) {
                    s += 2;
                    if (*s == 'v') {
                        // printf("i(v...) found in\n%s\n not converted!\n\n",
                        // curr_line);
                        continue;
                    }
                    else {
                        char *beg_str, *new_str;
                        get_r_paren(&u);
                        /* token containing name of devices to be measured */
                        t = copy_substring(s, --u);
                        if (ft_ngdebug)
                            printf("i(%s) found in\n%s\n\n", t, v);

                        /* new entry to the end of struct rep */
                        new_rep = TMALLOC(struct replace_currm, 1);
                        new_rep->s_start = subc_start;
                        new_rep->next = NULL;
                        new_rep->cline = card;
                        new_rep->rtoken = t;
                        if (act_rep) {
                            act_rep->next = new_rep;
                            act_rep = act_rep->next;
                        }
                        else
                            rep = act_rep = new_rep;
                        /* change line, convert i(XXX) to i(v_XXX) */
                        beg_str = copy_substring(v, s);
                        new_str = tprintf("%s%s%s", beg_str, "v_", s);
                        if (ft_ngdebug)
                            printf("converted to\n%s\n\n", new_str);
                        tfree(card->line);
                        card->line = s = v = new_str;
                        s++;
                        tfree(beg_str);
                    }
                }
                else
                    s++;
            }
        }
        tfree(w);
    }

    /* return if we did not find any i( */
    if (rep == NULL)
        return;

    /* scan through all the devices, search for xyz, modify node 1 by adding
       _vmeas, add a line with zero voltage v_xyz, having original node 1 and
       modified node 1. Do this within the top level or the same level of
       subcircuit only. */
    new_rep = rep;
    for (; rep; rep = rep->next) {
        card = rep->s_start;
        subs = 0;
        if (card)
            card = card->nextcard;
        else
            card = deck;
        for (; card; card = card->nextcard) {
            char *tok, *new_tok, *node1, *new_line;
            char *curr_line = card->line;
            /* exclude any command inside .control ... .endc */
            if (ciprefix(".control", curr_line)) {
                skip_control++;
                continue;
            }
            else if (ciprefix(".endc", curr_line)) {
                skip_control--;
                continue;
            }
            else if (skip_control > 0) {
                continue;
            }

            if (*curr_line == '*')
                continue;

            if (*curr_line == '.') {
                if (ciprefix(".subckt", curr_line))
                    subs++;
                else if (ciprefix(".ends", curr_line))
                    subs--;
                else
                    continue;
            }
            if (subs > 0)
                continue;
            /* We are at now top level or in top level of subcircuit
               where i(xyz) has been found */
            tok = gettok(&curr_line);
            /* done when end of subcircuit is reached */
            if (eq(".ends", tok) && rep->s_start) {
                tfree(tok);
                break;
            }
            if (eq(rep->rtoken, tok)) {
                /* special treatment if we have an e (VCVS) or h (CCVS)
                source: check if it is a simple linear source, if yes, don't
                do a replacement, instead undo the already done name
                conversion */
                if (((tok[0] == 'e') || (tok[0] == 'h')) &&
                        !strchr(curr_line, '=') &&
                        !is_poly_source(card->line)) {
                    /* simple linear e source */
                    char *searchstr = tprintf("i(v_%s)", tok);
                    char *thisline = rep->cline->line;
                    char *findstr = strstr(thisline, searchstr);
                    while (findstr) {
                        if (prefix(searchstr, findstr))
                            memcpy(findstr, "  i(", 4);
                        findstr = strstr(thisline, searchstr);
                        if (ft_ngdebug)
                            printf("i(%s) moved back to i(%s) in\n%s\n\n",
                                    searchstr, tok, rep->cline->line);
                    }
                    tfree(searchstr);
                    tfree(tok);
                    continue;
                }
                node1 = gettok(&curr_line);
                /* Add _vmeas only once to first device node.
                   Continue if we already have modified device "tok" */
                if (!strstr(node1, "_vmeas")) {
                    new_line = tprintf(
                            "%s %s_vmeas_%d %s", tok, node1, sn, curr_line);
                    tfree(card->line);
                    card->line = new_line;
                }

                new_tok = tprintf("v_%s", tok);
                /* We have already added a line v_xyz to the deck */
                if (!ciprefix(new_tok, card->nextcard->line)) {
                    /* add new line */
                    new_line = tprintf(
                            "%s %s %s_vmeas_%d 0", new_tok, node1, node1, sn);
                    /* insert new_line after card->line */
                    insert_new_line(card, new_line, card->linenum + 1, 0);
                }
                sn++;
                tfree(new_tok);
                tfree(node1);
            }
            tfree(tok);
        }
    }

    /* free rep */
    while (new_rep) {
        struct replace_currm *repn = new_rep->next;
        tfree(new_rep->rtoken);
        tfree(new_rep);
        new_rep = repn;
    }
}

/* replace the E source TABLE function by a B source pwl
   (used by ST OpAmps and comparators).
   E_RO_3 VB_3 VB_4  VALUE={ TABLE( V(VCCP,VCCN), 2 , 35 , 3.3 , 15 , 5 , 10
   )*I(VreadIo)} will become BE_RO_3_1 TABLE_NEW_1 0 v = pwl( V(VCCP,VCCN), 2
   , 35 , 3.3 , 15 , 5 , 10 ) E_RO_3 VB_3 VB_4  VALUE={
   V(TABLE_NEW_1)*I(VreadIo)}
*/
static void replace_table(struct card *startcard)
{
    struct card *card;
    static int numb = 0;
    for (card = startcard; card; card = card->nextcard) {
        char *cut_line = card->line;
        if (*cut_line == 'e') {
            char *valp = strstr(cut_line, "value={");
            if (valp) {
                char *ftablebeg = strstr(cut_line, "table(");
                while (ftablebeg) {
                    /* get the beginning of the line */
                    char *begline = copy_substring(cut_line, ftablebeg);
                    /* get the table function */
                    char *tabfun = gettok_char(&ftablebeg, ')', TRUE, TRUE);
                    /* the new e line */
                    char *neweline = tprintf(
                            "%s v(table_new_%d)%s", begline, numb, ftablebeg);
                    char *newbline =
                            tprintf("btable_new_%d table_new_%d 0 v=pwl%s",
                                    numb, numb, tabfun + 5);
                    numb++;
                    tfree(tabfun);
                    tfree(begline);
                    tfree(card->line);
                    card->line = cut_line = neweline;
                    insert_new_line(card, newbline, 0, 0);
                    /* read next TABLE function in cut_line */
                    ftablebeg = strstr(cut_line, "table(");
                }
                continue;
            }
        }
    }
}

/* find the model requested by ako:model and do the replacement */
static struct card *find_model(struct card *startcard,
        struct card *changecard, char *searchname, char *newmname,
        char *newmtype, char *endstr)
{
    struct card *nomod, *returncard = changecard;
    char *origmname, *origmtype;
    char *beginline = startcard->line;
    if (ciprefix(".subckt", beginline))
        startcard = startcard->nextcard;

    int nesting2 = 0;
    for (nomod = startcard; nomod; nomod = nomod->nextcard) {
        char *origmodline = nomod->line;
        if (ciprefix(".subckt", origmodline))
            nesting2++;
        if (ciprefix(".ends", origmodline))
            nesting2--;
        /* skip any subcircuit */
        if (nesting2 > 0)
            continue;
        if (nesting2 == -1) {
            returncard = changecard;
            break;
        }
        if (ciprefix(".model", origmodline)) {
            origmodline = nexttok(origmodline);
            origmname = gettok(&origmodline);
            origmtype = gettok_noparens(&origmodline);
            if (cieq(origmname, searchname)) {
                if (!eq(origmtype, newmtype)) {
                    fprintf(stderr,
                            "Error: Original (%s) and new (%s) type for AKO "
                            "model disagree\n",
                            origmtype, newmtype);
                    controlled_exit(1);
                }
                /* we have got it */
                char *newmodcard = tprintf(".model %s %s %s%s", newmname,
                        newmtype, origmodline, endstr);
                char *tmpstr = strstr(newmodcard, ")(");
                if (tmpstr) {
                    tmpstr[0] = ' ';
                    tmpstr[1] = ' ';
                }
                tfree(changecard->line);
                changecard->line = newmodcard;
                tfree(origmname);
                tfree(origmtype);
                returncard = NULL;
                break;
            }
            tfree(origmname);
            tfree(origmtype);
        }
        else
            returncard = changecard;
    }
    return returncard;
}

/* do the .model replacement required by ako (a kind of)
 * PSPICE does not support ested .subckt definitions, so
 * a simple structure is needed: search for ako:modelname,
 * then for modelname in the subcircuit or in the top level.
 * .model qorig npn (BF=48 IS=2e-7)
 * .model qbip1 ako:qorig NPN (BF=60 IKF=45m)
 * after the replacement we have
 * .model qbip1 NPN (BF=48 IS=2e-7 BF=60 IKF=45m)
 * and we benefit from the fact that if parameters have
 * doubled, the last entry of a parameter (e.g. BF=60)
 * overwrites the previous one (BF=48).
 */
static struct card *ako_model(struct card *startcard)
{
    char *newmname, *newmtype;
    struct card *card, *returncard = NULL, *subcktcard = NULL;
    for (card = startcard; card; card = card->nextcard) {
        char *akostr, *searchname;
        char *cut_line = card->line;
        if (ciprefix(".subckt", cut_line))
            subcktcard = card;
        else if (ciprefix(".ends", cut_line))
            subcktcard = NULL;
        if (ciprefix(".model", cut_line) &&
                ((akostr = strstr(cut_line, "ako:")) != NULL) &&
                isspace_c(akostr[-1])) {
            akostr += 4;
            searchname = gettok(&akostr);
            cut_line = nexttok(cut_line);
            newmname = gettok(&cut_line);
            newmtype = gettok_noparens(&akostr);
            /* find the model and do the replacement */
            if (subcktcard)
                returncard = find_model(subcktcard, card, searchname,
                        newmname, newmtype, akostr);
            if (returncard || !subcktcard)
                returncard = find_model(startcard, card, searchname, newmname,
                        newmtype, akostr);
            tfree(searchname);
            tfree(newmname);
            tfree(newmtype);
            /* replacement not possible, bail out */
            if (returncard)
                break;
        }
    }
    return returncard;
}

/* in       out
   von      cntl_on
   voff     cntl_off
   ron      r_on
   roff     r_off
*/
static int rep_spar(char *inpar[4])
{
    int i;
    for (i = 0; i < 4; i++) {
        char *t, *strend;
        char *tok = inpar[i];
        if ((t = strstr(tok, "von")) != NULL) {
            strend = copy(t + 1);
            tfree(inpar[i]);
            inpar[i] = tprintf("cntl_%s", strend);
            tfree(strend);
        }
        else if ((t = strstr(tok, "voff")) != NULL) {
            strend = copy(t + 1);
            tfree(inpar[i]);
            inpar[i] = tprintf("cntl_%s", strend);
            tfree(strend);
        }
        else if ((t = strstr(tok, "ron")) != NULL) {
            strend = copy(t + 1);
            tfree(inpar[i]);
            inpar[i] = tprintf("r_%s", strend);
            tfree(strend);
        }
        else if ((t = strstr(tok, "roff")) != NULL) {
            strend = copy(t + 1);
            tfree(inpar[i]);
            inpar[i] = tprintf("r_%s", strend);
            tfree(strend);
        }
        else {
            fprintf(stderr, "Bad vswitch parameter %s\n", tok);
            return 1;
        }
    }
    return 0;
}

struct vsmodels {
    char *modelname;
    char *subcktline;
    struct vsmodels *nextmodel;
};

/* insert a new model, just behind the given model */
static struct vsmodels *insert_new_model(
        struct vsmodels *vsmodel, char *name, char *subcktline)
{
    struct vsmodels *x = TMALLOC(struct vsmodels, 1);

    x->nextmodel = vsmodel ? vsmodel->nextmodel : NULL;
    x->modelname = copy(name);
    x->subcktline = copy(subcktline);
    if (vsmodel)
        vsmodel->nextmodel = x;
    else
        vsmodel = x;

    return vsmodel;
}

/* find the model */
static bool find_a_model(
        struct vsmodels *vsmodel, char *name, char *subcktline)
{
    struct vsmodels *x;
    for (x = vsmodel; vsmodel; vsmodel = vsmodel->nextmodel)
        if (eq(vsmodel->modelname, name) &&
                eq(vsmodel->subcktline, subcktline))
            return TRUE;
    return FALSE;
}

/* delete the vsmodels list */
static bool del_models(struct vsmodels *vsmodel)
{
    struct vsmodels *x;

    if (!vsmodel)
        return FALSE;

    while (vsmodel) {
        x = vsmodel->nextmodel;
        tfree(vsmodel->modelname);
        tfree(vsmodel->subcktline);
        tfree(vsmodel);
        vsmodel = x;
    }

    return TRUE;
}

/**** PSPICE to ngspice **************
* .model replacement in ako (a kind of) model descriptions
* replace the E source TABLE function by a B source pwl
* add predefined params TEMP, VT, GMIN to beginning of deck
* add predefined params TEMP, VT, GMIN to beginning of each .subckt call
* add .functions limit, pwr, pwrs, stp, if, int
* replace
  S1 D S DG GND SWN
 .MODEL SWN VSWITCH(VON = { 0.55 } VOFF = { 0.49 } RON = { 1 / (2 * M*(W /
LE)*(KPN / 2) * 10) }  ROFF = { 1G })
* by
  as1 %vd(DG GND) % gd(D S) aswn
  .model aswn aswitch(cntl_off={0.49} cntl_on={0.55} r_off={1G}
  + r_on={ 1 / (2 * M*(W / LE)*(KPN / 2) * 10) } log = TRUE)
* replace & by &&
* replace | by ||
* in R instance, replace TC = xx1, xx2 by TC1=xx1 TC2=xx2
* replace T_ABS by temp and T_REL_GLOBAL by dtemp in .model cards
* get the area factor for diodes and bipolar devices */
static struct card *pspice_compat(struct card *oldcard)
{
    struct card *card, *newcard, *nextcard;
    struct vsmodels *modelsfound = NULL;
    int skip_control = 0;

    /* .model replacement in ako (a kind of) model descriptions
     * in first .subckt and top level only */
    struct card *errcard;
    if ((errcard = ako_model(oldcard)) != NULL) {
        fprintf(stderr, "Error: no model found for %s\n", errcard->line);
        controlled_exit(1);
    }

    /* replace TABLE function in E source */
    replace_table(oldcard);

    /* add predefined params TEMP, VT, GMIN to beginning of deck */
    char *new_str = copy(".param temp = 'temper'");
    newcard = insert_new_line(NULL, new_str, 1, 0);
    new_str = copy(".param vt = '(temper + 273.15) * 8.6173303e-5'");
    nextcard = insert_new_line(newcard, new_str, 2, 0);
    new_str = copy(".param gmin = 1e-12");
    nextcard = insert_new_line(nextcard, new_str, 3, 0);
    /* add funcs limit, pwr, pwrs, stp, if, int */
    new_str = copy(".func limit(x, a, b) { min(max(x, a), b) }");
    nextcard = insert_new_line(nextcard, new_str, 4, 0);
    new_str = copy(".func pwr(x, a) { abs(x) ** a }");
    nextcard = insert_new_line(nextcard, new_str, 5, 0);
    new_str = copy(".func pwrs(x, a) { sgn(x) * pwr(x, a) }");
    nextcard = insert_new_line(nextcard, new_str, 6, 0);
    new_str = copy(".func stp(x) { u(x) }");
    nextcard = insert_new_line(nextcard, new_str, 7, 0);
    new_str = copy(".func if(a, b, c) {ternary_fcn( a , b , c )}");
    nextcard = insert_new_line(nextcard, new_str, 8, 0);
    new_str = copy(".func int(x) { sign(x)*floor(abs(x)) }");
    nextcard = insert_new_line(nextcard, new_str, 9, 0);
    nextcard->nextcard = oldcard;

    /* add predefined parameters TEMP, VT after each subckt call */
    /* FIXME: This should not be necessary if we had a better sense of
    hierarchy during the evaluation of TEMPER */
    for (card = newcard; card; card = card->nextcard) {
        char *cut_line = card->line;
        if (ciprefix(".subckt", cut_line)) {
            new_str = copy(".param temp = 'temper'");
            nextcard = insert_new_line(card, new_str, 0, 0);
            new_str = copy(".param vt = '(temper + 273.15) * 8.6173303e-5'");
            nextcard = insert_new_line(nextcard, new_str, 1, 0);
        }
    }

    /* in R instance, replace TC = xx1, xx2 by TC1=xx1 TC2=xx2 */
    for (card = newcard; card; card = card->nextcard) {
        char *cut_line = card->line;

        /* exclude any command inside .control ... .endc */
        if (ciprefix(".control", cut_line)) {
            skip_control++;
            continue;
        }
        else if (ciprefix(".endc", cut_line)) {
            skip_control--;
            continue;
        }
        else if (skip_control > 0) {
            continue;
        }

        if (*cut_line == 'r' || *cut_line == 'l' || *cut_line == 'c') {
            /* Skip name and two nodes */
            char *ntok = nexttok(cut_line);
            ntok = nexttok(ntok);
            ntok = nexttok(ntok);
            if (!ntok){
                fprintf(stderr, "Error: Missing token in line %d:\n%s\n", card->linenum, cut_line);
                fprintf(stderr, "    Please correct the input file\n");
                controlled_exit(1);
            }
            char *tctok = search_plain_identifier(ntok, "tc");
            if (tctok) {
                char *tctok1 = strchr(tctok, '=');
                if (tctok1)
                    /* skip '=' */
                    tctok1 += 1;
                else
                    /* no '=' found, skip 'tc' */
                    tctok1 = tctok + 2;
                char *tc1 = gettok_node(&tctok1);
                char *tc2 = gettok_node(&tctok1);
                tctok[-1] = '\0';
                char *newstring;
                if (tc1 && tc2)
                    newstring =
                            tprintf("%s tc1=%s tc2=%s", cut_line, tc1, tc2);
                else if (tc1)
                    newstring = tprintf("%s tc1=%s", cut_line, tc1);
                else {
                    fprintf(stderr,
                            "Warning: tc without parameters removed in line "
                            "\n   %s\n",
                            cut_line);
                    continue;
                }
                tfree(card->line);
                card->line = newstring;
                tfree(tc1);
                tfree(tc2);
            }
        }
    }

    /* replace & with && and | with || and *# with * # */
    for (card = newcard; card; card = card->nextcard) {
        char *t;
        char *cut_line = card->line;

        /* we don't have command lines in a PSPICE model */
        if (ciprefix("*#", cut_line)) {
            char *tmpstr = tprintf("* #%s", cut_line + 2);
            tfree(card->line);
            card->line = tmpstr;
            continue;
        }

        if (*cut_line == '*')
            continue;
        /* exclude any command inside .control ... .endc */
        if (ciprefix(".control", cut_line)) {
            skip_control++;
            continue;
        }
        else if (ciprefix(".endc", cut_line)) {
            skip_control--;
            continue;
        }
        else if (skip_control > 0) {
            continue;
        }
        if ((t = strstr(card->line, "&")) != NULL) {
            while (t && (t[1] != '&')) {
                char *tt = NULL;
                char *tn = copy(t + 1); /*skip |*/
                char *strbeg = copy_substring(card->line, t);
                tfree(card->line);
                card->line = tprintf("%s&&%s", strbeg, tn);
                tfree(strbeg);
                tfree(tn);
                t = card->line;
                while ((t = strstr(t, "&&")) != NULL)
                    tt = t = t + 2;
                if (!tt)
                    break;
                else
                    t = strstr(tt, "&");
            }
        }
        if ((t = strstr(card->line, "|")) != NULL) {
            while (t && (t[1] != '|')) {
                char *tt = NULL;
                char *tn = copy(t + 1); /*skip |*/
                char *strbeg = copy_substring(card->line, t);
                tfree(card->line);
                card->line = tprintf("%s||%s", strbeg, tn);
                tfree(strbeg);
                tfree(tn);
                t = card->line;
                while ((t = strstr(t, "||")) != NULL)
                    tt = t = t + 2;
                if (!tt)
                    break;
                else
                    t = strstr(tt, "|");
            }
        }
    }

    /* replace T_ABS by temp, T_REL_GLOBAL by dtemp, and T_MEASURED by TNOM
    in .model cards. What about T_REL_LOCAL ? T_REL_LOCAL is used in conjunction with AKO
    and is not yet implemented.  */
    for (card = newcard; card; card = card->nextcard) {
        char *cut_line = card->line;
        if (ciprefix(".model", cut_line)) {
            char *t_str;
            if ((t_str = strstr(cut_line, "t_abs")) != NULL)
                memcpy(t_str, " temp", 5);
            else if ((t_str = strstr(cut_line, "t_rel_global")) != NULL)
                memcpy(t_str, "       dtemp", 12);
            else if ((t_str = strstr(cut_line, "t_measured")) != NULL)
                memcpy(t_str, "      tnom", 10);
        }
    }

    /* get the area factor for diodes and bipolar devices
    d1 n1 n2 dmod 7 --> d1 n1 n2 dmod area=7
    q2 n1 n2 n3 [n4] bjtmod 1.35 --> q2 n1 n2 n3 n4 bjtmod area=1.35
    q3 1 2 3 4 bjtmod 1.45 --> q2 1 2 3 4 bjtmod area=1.45
    */
    for (card = newcard; card; card = card->nextcard) {
        char *cut_line = card->line;
        if (*cut_line == '*')
            continue;
        // exclude any command inside .control ... .endc
        if (ciprefix(".control", cut_line)) {
            skip_control++;
            continue;
        }
        else if (ciprefix(".endc", cut_line)) {
            skip_control--;
            continue;
        }
        else if (skip_control > 0) {
            continue;
        }
        if (*cut_line == 'q') {
            cut_line = nexttok(cut_line); //.model
            cut_line = nexttok(cut_line); // node1
            cut_line = nexttok(cut_line); // node2
            cut_line = nexttok(cut_line); // node3
            if (!cut_line) {
                fprintf(stderr, "Line no. %d, %s missing tokens\n",
                        card->linenum_orig, card->line);
                continue;
            }
            if (*cut_line == '[') { // node4 not a number
                *cut_line = ' ';
                cut_line = strchr(cut_line, ']');
                *cut_line = ' ';
                cut_line = skip_ws(cut_line);
                cut_line = nexttok(cut_line); // model name
            }
            else { // if an integer number, it is node4
                bool is_node4 = TRUE;
                while (*cut_line && !isspace(*cut_line))
                    if (!isdigit(*cut_line++))
                        is_node4 = FALSE; // already model name
                if (is_node4)
                    cut_line = nexttok(cut_line); // model name
            }
            if (*cut_line &&
                    atof(cut_line) > 0.0) { // size of area is a real number
                char *tmpstr1 = copy_substring(card->line, cut_line);
                char *tmpstr2 = tprintf("%s area=%s", tmpstr1, cut_line);
                tfree(tmpstr1);
                tfree(card->line);
                card->line = tmpstr2;
            }
            else if (*cut_line &&
                    *(skip_ws(cut_line)) ==
                            '{') { // size of area is parametrized inside {}
                char *tmpstr1 = copy_substring(card->line, cut_line);
                char *tmpstr2 = gettok_char(&cut_line, '}', TRUE, TRUE);
                char *tmpstr3 =
                        tprintf("%s area=%s %s", tmpstr1, tmpstr2, cut_line);
                tfree(tmpstr1);
                tfree(tmpstr2);
                tfree(card->line);
                card->line = tmpstr3;
            }
        }
        else if (*cut_line == 'd') {
            cut_line = nexttok(cut_line); //.model
            cut_line = nexttok(cut_line); // node1
            cut_line = nexttok(cut_line); // node2
            if (!cut_line) {
                fprintf(stderr, "Line no. %d, %s missing tokens\n",
                        card->linenum_orig, card->line);
                continue;
            }
            cut_line = nexttok(cut_line); // model name
            if (*cut_line && atof(cut_line) > 0.0) { // size of area
                char *tmpstr1 = copy_substring(card->line, cut_line);
                char *tmpstr2 = tprintf("%s area=%s", tmpstr1, cut_line);
                tfree(tmpstr1);
                tfree(card->line);
                card->line = tmpstr2;
            }
        }
    }

    /* replace
    * S1 D S DG GND SWN
    * .MODEL SWN VSWITCH ( VON = {0.55} VOFF = {0.49}
    RON={1/(2*M*(W/LE)*(KPN/2)*10)}  ROFF={1G} )
    * by
    * a1 %v(DG) %gd(D S) swa
    * .MODEL SWA aswitch(cntl_off=0.49 cntl_on=0.55 r_off=1G
    r_on={1/(2*M*(W/LE)*(KPN/2)*10)} log=TRUE)

    * simple hierachy, as nested subcircuits are not allowed in PSPICE */

    /* first scan: find the vswitch models, transform them and put them into a
     * list */
    for (card = newcard; card; card = card->nextcard) {
        char *str;
        static struct card *subcktline = NULL;
        static int nesting = 0;
        char *cut_line = card->line;
        if (ciprefix(".subckt", cut_line)) {
            subcktline = card;
            nesting++;
        }
        if (ciprefix(".ends", cut_line))
            nesting--;

        if (ciprefix(".model", card->line) && strstr(card->line, "vswitch")) {
            char *modpar[4];
            char *modname;
            int i;

            card->line = str = inp_remove_ws(card->line);
            str = nexttok(str); /* throw away '.model' */
            INPgetNetTok(&str, &modname, 0); /* model name */
            if (!ciprefix("vswitch", str)) {
                tfree(modname);
                continue;
            }
            /* we have to find 4 parameters, identified by '=', separated by
             * spaces */
            char *equalptr[4];
            equalptr[0] = strstr(str, "=");
            if (!equalptr[0]) {
                fprintf(stderr,
                        "Error: not enough parameters in vswitch model\n   "
                        "%s\n",
                        card->line);
                controlled_exit(1);
            }
            for (i = 1; i < 4; i++) {
                equalptr[i] = strstr(equalptr[i - 1] + 1, "=");
                if (!equalptr[i]) {
                    fprintf(stderr,
                            "Error: not enough parameters in vswitch model\n "
                            "  %s\n",
                            card->line);
                    controlled_exit(1);
                }
            }
            for (i = 0; i < 4; i++) {
                equalptr[i] = skip_back_ws(equalptr[i], str);
                while (*(equalptr[i]) != '(' && !isspace_c(*(equalptr[i])) &&
                        *(equalptr[i]) != ',')
                    (equalptr[i])--;
                (equalptr[i])++;
            }
            for (i = 0; i < 3; i++)
                modpar[i] = copy_substring(equalptr[i], equalptr[i + 1] - 1);
            if (strrchr(equalptr[3], ')'))
                modpar[3] = copy_substring(
                        equalptr[3], strrchr(equalptr[3], ')'));
            else
                /* vswitch defined without parens */
                modpar[3] = copy(equalptr[3]);
            tfree(card->line);
            /* replace VON by cntl_on, VOFF by cntl_off, RON by r_on, and ROFF
             * by r_off */
            rep_spar(modpar);
            card->line = tprintf(".model a%s aswitch(%s %s %s %s  log=TRUE)",
                    modname, modpar[0], modpar[1], modpar[2], modpar[3]);
            for (i = 0; i < 4; i++)
                tfree(modpar[i]);
            if (nesting > 0)
                modelsfound = insert_new_model(
                        modelsfound, modname, subcktline->line);
            else
                modelsfound = insert_new_model(modelsfound, modname, "top");
            tfree(modname);
        }
    }

    /* no need to continue if no vswitch is found */
    if (!modelsfound)
        return newcard;

    /* second scan: find the switch instances s calling a vswitch model and
     * transform them */
    for (card = newcard; card; card = card->nextcard) {
        static struct card *subcktline = NULL;
        static int nesting = 0;
        char *cut_line = card->line;
        if (*cut_line == '*')
            continue;
        // exclude any command inside .control ... .endc
        if (ciprefix(".control", cut_line)) {
            skip_control++;
            continue;
        }
        else if (ciprefix(".endc", cut_line)) {
            skip_control--;
            continue;
        }
        else if (skip_control > 0) {
            continue;
        }
        if (ciprefix(".subckt", cut_line)) {
            subcktline = card;
            nesting++;
        }
        if (ciprefix(".ends", cut_line))
            nesting--;

        if (ciprefix("s", cut_line)) {
            /* check for the model name */
            int i;
            char *stoks[6];
            for (i = 0; i < 6; i++)
                stoks[i] = gettok_node(&cut_line);
            /* rewrite s line and replace it if a model is found */
            if ((nesting > 0) &&
                    find_a_model(modelsfound, stoks[5], subcktline->line)) {
                tfree(card->line);
                card->line = tprintf("a%s %%vd(%s %s) %%gd(%s %s) a%s",
                        stoks[0], stoks[3], stoks[4], stoks[1], stoks[2],
                        stoks[5]);
            }
            /* if model is not within same subcircuit, search at top level */
            else if (find_a_model(modelsfound, stoks[5], "top")) {
                tfree(card->line);
                card->line = tprintf("a%s %%vd(%s %s) %%gd(%s %s) a%s",
                        stoks[0], stoks[3], stoks[4], stoks[1], stoks[2],
                        stoks[5]);
            }
            for (i = 0; i < 6; i++)
                tfree(stoks[i]);
        }
    }
    del_models(modelsfound);

    return newcard;
}

/* do not modify oldcard address, insert everything after first line only */
static void pspice_compat_a(struct card *oldcard)
{
    oldcard->nextcard = pspice_compat(oldcard->nextcard);
}


/**** LTSPICE to ngspice **************
 * add functions uplim, dnlim
 * Replace
 * D1 A K SDMOD
 * .MODEL SDMOD D (Roff=1000 Ron=0.7  Rrev=0.2  Vfwd=1  Vrev=10 Revepsilon=0.2
 * Epsilon=0.2 Ilimit=7 Revilimit=7) by ad1 a k asdmod .model asdmod
 * sidiode(Roff=1000 Ron=0.7  Rrev=0.2  Vfwd=1  Vrev=10 Revepsilon=0.2
 * Epsilon=0.2 Ilimit=7 Revilimit=7)
 */
struct card *ltspice_compat(struct card *oldcard)
{
    struct card *card, *newcard, *nextcard;
    struct vsmodels *modelsfound = NULL;
    int skip_control = 0;

    /* add funcs uplim, dnlim to beginning of deck */
    char *new_str =
            copy(".func uplim(x, pos, z) { min(x, pos - z) + (1 - "
                 "(min(max(0, x - pos + z), 2 * z) / 2 / z - 1)**2)*z }");
    newcard = insert_new_line(NULL, new_str, 1, 0);
    new_str = copy(".func dnlim(x, neg, z) { max(x, neg + z) - (1 - "
                   "(min(max(0, -x + neg + z), 2 * z) / 2 / z - 1)**2)*z }");
    nextcard = insert_new_line(newcard, new_str, 2, 0);
    new_str = copy(".func uplim_tanh(x, pos, z) { min(x, pos - z) + "
                   "tanh(max(0, x - pos + z) / z)*z }");
    nextcard = insert_new_line(nextcard, new_str, 3, 0);
    new_str = copy(".func dnlim_tanh(x, neg, z) { max(x, neg + z) - "
                   "tanh(max(0, neg + z - x) / z)*z }");
    nextcard = insert_new_line(nextcard, new_str, 4, 0);
    nextcard->nextcard = oldcard;

    /* replace
   * D1 A K SDMOD
   * .MODEL SDMOD D (Roff=1000 Ron=0.7  Rrev=0.2  Vfwd=1  Vrev=10
   Revepsilon=0.2 Epsilon=0.2 Ilimit=7 Revilimit=7)
   * by
   * a1 a k SDMOD
   * .model SDMOD sidiode(Roff=1000 Ron=0.7  Rrev=0.2  Vfwd=1  Vrev=10
   Revepsilon=0.2 Epsilon=0.2 Ilimit=7 Revilimit=7)
   * Do this if one of the parameters, which are uncommon to standard diode
   model, has been found.

   * simple hierachy, as nested subcircuits are not allowed in PSPICE */

    /* first scan: find the d models, transform them and put them into a list
     */
    for (card = nextcard; card; card = card->nextcard) {
        char *str;
        static struct card *subcktline = NULL;
        static int nesting = 0;
        char *cut_line = card->line;
        if (ciprefix(".subckt", cut_line)) {
            subcktline = card;
            nesting++;
        }
        if (ciprefix(".ends", cut_line))
            nesting--;

        if (ciprefix(".model", card->line) &&
                search_plain_identifier(card->line, "d")) {
            if (search_plain_identifier(card->line, "roff") ||
                    search_plain_identifier(card->line, "ron") ||
                    search_plain_identifier(card->line, "rrev") ||
                    search_plain_identifier(card->line, "vfwd") ||
                    search_plain_identifier(card->line, "vrev") ||
                    search_plain_identifier(card->line, "revepsilon") ||
                    search_plain_identifier(card->line, "epsilon") ||
                    search_plain_identifier(card->line, "revilimit") ||
                    search_plain_identifier(card->line, "ilimit")) {
                char *modname;

                card->line = str = inp_remove_ws(card->line);
                str = nexttok(str); /* throw away '.model' */
                INPgetNetTok(&str, &modname, 0); /* model name */
                if (!ciprefix("d", str)) {
                    tfree(modname);
                    continue;
                }
                /* skip d */
                str++;
                /* we take all the existing parameters */
                char *newstr = copy(str);
                tfree(card->line);
                card->line = tprintf(".model a%s sidiode%s", modname, newstr);
                if (nesting > 0)
                    modelsfound = insert_new_model(
                            modelsfound, modname, subcktline->line);
                else
                    modelsfound =
                            insert_new_model(modelsfound, modname, "top");
                tfree(modname);
                tfree(newstr);
            }
        }
        else
            continue;
    }

    /* no need to continue if no d is found */
    if (!modelsfound)
        return newcard;

    /* second scan: find the diode instances d calling a simple diode model
     * and transform them */
    for (card = nextcard; card; card = card->nextcard) {
        static struct card *subcktline = NULL;
        static int nesting = 0;
        char *cut_line = card->line;
        if (*cut_line == '*')
            continue;
        // exclude any command inside .control ... .endc
        if (ciprefix(".control", cut_line)) {
            skip_control++;
            continue;
        }
        else if (ciprefix(".endc", cut_line)) {
            skip_control--;
            continue;
        }
        else if (skip_control > 0) {
            continue;
        }
        if (ciprefix(".subckt", cut_line)) {
            subcktline = card;
            nesting++;
        }
        if (ciprefix(".ends", cut_line))
            nesting--;

        if (ciprefix("d", cut_line)) {
            /* check for the model name */
            int i;
            char *stoks[4];
            for (i = 0; i < 4; i++)
                stoks[i] = gettok_node(&cut_line);
            /* rewrite d line and replace it if a model is found */
            if ((nesting > 0) &&
                    find_a_model(modelsfound, stoks[3], subcktline->line)) {
                tfree(card->line);
                card->line = tprintf("a%s %s %s a%s", stoks[0], stoks[1],
                        stoks[2], stoks[3]);
            }
            /* if model is not within same subcircuit, search at top level */
            else if (find_a_model(modelsfound, stoks[3], "top")) {
                tfree(card->line);
                card->line = tprintf("a%s %s %s a%s", stoks[0], stoks[1],
                        stoks[2], stoks[3]);
            }
            for (i = 0; i < 4; i++)
                tfree(stoks[i]);
        }
    }
    del_models(modelsfound);

    return newcard;
}

/* do not modify oldcard address, insert everything after first line only */
static void ltspice_compat_a(struct card *oldcard)
{
    oldcard->nextcard = ltspice_compat(oldcard->nextcard);
}

/* syntax check:
   Check if we have a .control ... .endc pair,
   a .if ... .endif pair, a .suckt ... .ends pair */
static void inp_check_syntax(struct card *deck)
{
    struct card *card;
    int check_control = 0, check_subs = 0, check_if = 0;

    /* will lead to crash in inp.c, fcn inp_spsource */
    if (ciprefix(".param", deck->line) || ciprefix(".meas", deck->line)) {
        fprintf(cp_err, "\nError: title line is missing!\n\n");
        controlled_exit(EXIT_BAD);
    }

    for (card = deck; card; card = card->nextcard) {
        char *cut_line = card->line;
        if (*cut_line == '*')
            continue;
        // check for .control ... .endc
        if (ciprefix(".control", cut_line)) {
            if (check_control > 0) {
                fprintf(cp_err,
                        "\nError: Nesting of .control statements is not "
                        "allowed!\n\n");
                controlled_exit(EXIT_BAD);
            }
            check_control++;
            continue;
        }
        else if (ciprefix(".endc", cut_line)) {
            check_control--;
            continue;
        }
        // check for .subckt ... .ends
        else if (ciprefix(".subckt", cut_line)) {
            // nesting may be critical if params are involved
            if (check_subs > 0 && strchr(cut_line, '='))
                fprintf(cp_err,
                        "\nWarning: Nesting of subcircuits with parameters "
                        "is only marginally supported!\n\n");
            check_subs++;
            continue;
        }
        else if (ciprefix(".ends", cut_line)) {
            check_subs--;
            continue;
        }
        // check for .if ... .endif
        if (ciprefix(".if", cut_line)) {
            check_if++;
            has_if = TRUE;
            continue;
        }
        else if (ciprefix(".endif", cut_line)) {
            check_if--;
            continue;
        }
    }

    if (check_control > 0) {
        fprintf(cp_err, "\nWarning: Missing .endc statement!\n");
        fprintf(cp_err, "    This may cause subsequent errors.\n\n");
    }
    if (check_control < 0) {
        fprintf(cp_err, "\nWarning: Missing .control statement!\n");
        fprintf(cp_err, "    This may cause subsequent errors.\n\n");
    }
    if (check_subs != 0) {
        fprintf(cp_err,
                "\nError: Mismatch of .subckt ... .ends statements!\n");
        fprintf(cp_err, "    This will cause subsequent errors.\n\n");
        controlled_exit(EXIT_BAD);
    }
    if (check_if != 0) {
        fprintf(cp_err, "\nError: Mismatch of .if ... .endif statements!\n");
        fprintf(cp_err, "    This may cause subsequent errors.\n\n");
    }
}

/* remove the mfg=mfgname entry from the .model cards */
static void rem_mfg_from_models(struct card *deck)
{
    struct card *card;
    for (card = deck; card; card = card->nextcard) {

        char *curr_line, *end, *start;

        curr_line = start = card->line;
        /* remove mfg=name */
        if (ciprefix(".model", curr_line)) {
            start = strstr(curr_line, "mfg=");
            if (start) {
                end = nexttok(start);
                if (*end == '\0')
                    *start = '\0';
                else
                    while (start < end) {
                        *start = ' ';
                        start++;
                    }
            }
        }
    }
}

/* model type as input, element identifier as output */
static char inp_get_elem_ident(char *type)
{
    if (cieq(type, "r"))
        return 'r';
    else if (cieq(type, "c"))
        return 'c';
    else if (cieq(type, "l"))
        return 'l';
    else if (cieq(type, "nmos"))
        return 'm';
    else if (cieq(type, "pmos"))
        return 'm';
    else if (cieq(type, "numos"))
        return 'm';
    else if (cieq(type, "d"))
        return 'd';
    else if (cieq(type, "numd"))
        return 'd';
    else if (cieq(type, "numd2"))
        return 'd';
    else if (cieq(type, "npn"))
        return 'q';
    else if (cieq(type, "pnp"))
        return 'q';
    else if (cieq(type, "nbjt"))
        return 'q';
    else if (cieq(type, "nbjt2"))
        return 'q';
    else if (cieq(type, "njf"))
        return 'j';
    else if (cieq(type, "pjf"))
        return 'j';
    else if (cieq(type, "nmf"))
        return 'z';
    else if (cieq(type, "pmf"))
        return 'z';
    else if (cieq(type, "nhfet"))
        return 'z';
    else if (cieq(type, "phfet"))
        return 'z';
    else if (cieq(type, "sw"))
        return 's';
    else if (cieq(type, "csw"))
        return 'w';
    else if (cieq(type, "txl"))
        return 'y';
    else if (cieq(type, "cpl"))
        return 'p';
    else if (cieq(type, "ltra"))
        return 'o';
    else if (cieq(type, "urc"))
        return 'u';
    else if (ciprefix("vdmos", type))
        return 'm';
    if (cieq(type, "res"))
        return 'r';
    /* xspice code models do not have unique type names */
    else
        return 'a';
}


static struct card_assoc *find_subckt_1(
        struct nscope *scope, const char *name)
{
    struct card_assoc *p = scope->subckts;
    for (; p; p = p->next)
        if (eq(name, p->name))
            break;
    return p;
}


static struct card_assoc *find_subckt(struct nscope *scope, const char *name)
{
    for (; scope; scope = scope->next) {
        struct card_assoc *p = find_subckt_1(scope, name);
        if (p)
            return p;
    }
    return NULL;
}


static void add_subckt(struct nscope *scope, struct card *subckt_line)
{
    char *n = skip_ws(skip_non_ws(subckt_line->line));
    char *name = copy_substring(n, skip_non_ws(n));
    if (find_subckt_1(scope, name)) {
        fprintf(stderr, "Warning: redefinition of .subckt %s, ignored\n", name);
        /* rename the redefined subcircuit */
        *n = '_';
    }
    struct card_assoc *entry = TMALLOC(struct card_assoc, 1);
    entry->name = name;
    entry->line = subckt_line;
    entry->next = scope->subckts;
    scope->subckts = entry;
}


/* linked list of models, includes use info */
struct modellist {
    struct card *model;
    char *modelname;
    bool used;
    char elemb;
    struct modellist *next;
};


static struct modellist *inp_find_model_1(struct nscope *scope, const char *name)
{
    struct modellist *p = scope->models;
    for (; p; p = p->next)
        if (model_name_match(name, p->modelname))
            break;
    return p;
}


static struct modellist *inp_find_model(struct nscope *scope, const char *name)
{
    for (; scope; scope = scope->next) {
        struct modellist *p = inp_find_model_1(scope, name);
        if (p)
            return p;
    }
    return NULL;
}

/* scan through deck and add level information to all struct card
 * depending on nested subcircuits */
static struct nscope *inp_add_levels(struct card *deck)
{
    struct card *card;
    int skip_control = 0;

    struct nscope *root = TMALLOC(struct nscope, 1);
    root->next = NULL;
    root->subckts = NULL;
    root->models = NULL;

    struct nscope *lvl = root;

    for (card = deck; card; card = card->nextcard) {

        char *curr_line = card->line;

        /* exclude any command inside .control ... .endc */
        if (ciprefix(".control", curr_line)) {
            skip_control++;
            continue;
        }
        else if (ciprefix(".endc", curr_line)) {
            skip_control--;
            continue;
        }
        else if (skip_control > 0) {
            continue;
        }

        if (*curr_line == '.') {
            if (ciprefix(".subckt", curr_line)) {
                add_subckt(lvl, card);
                struct nscope *scope = TMALLOC(struct nscope, 1);
                // lvl->name = ..., or just point to the deck
                scope->next = lvl;
                scope->subckts = NULL;
                scope->models = NULL;
                lvl = card->level = scope;
            }
            else if (ciprefix(".ends", curr_line)) {
                if (lvl == root) {
                    fprintf(stderr, ".subckt/.ends not balanced\n");
                    controlled_exit(1);
                }
                card->level = lvl;
                lvl = lvl->next;
            }
            else {
                card->level = lvl;
            }
        }
        else {
            card->level = lvl;
        }
    }

    if (lvl != root)
        fprintf(stderr, "nesting error\n");

    return root;
}

/* remove the level and subckts entries */
static void inp_rem_levels(struct nscope *root)
{
    struct card_assoc *p = root->subckts;
    while (p) {
        inp_rem_levels(p->line->level);
        tfree(p->name);
        struct card_assoc *pn = p->next;
        tfree(p);
        p = pn;
    }
    tfree(root);
}

static void rem_unused_xxx(struct nscope *level)
{
    struct modellist *m = level->models;
    while (m) {
        struct modellist *next_m = m->next;
        if (!m->used)
            m->model->line[0] = '*';
        tfree(m->modelname);
        tfree(m);
        m = next_m;
    }
    level->models = NULL;

    struct card_assoc *p = level->subckts;
    for (; p; p = p->next)
        rem_unused_xxx(p->line->level);
}


static void mark_all_binned(struct nscope *scope, char *name)
{
    struct modellist *p = scope->models;

    for (; p; p = p->next)
        if (model_name_match(name, p->modelname))
            p->used = TRUE;
}


static void inp_rem_unused_models(struct nscope *root, struct card *deck)
{
    struct card *card;
    int skip_control = 0;

    /* create a list of .model */
    for (card = deck; card; card = card->nextcard) {

        char *curr_line = card->line;

        /* exclude any command inside .control ... .endc */
        if (ciprefix(".control", curr_line)) {
            skip_control++;
            continue;
        }
        else if (ciprefix(".endc", curr_line)) {
            skip_control--;
            continue;
        }
        else if (skip_control > 0) {
            continue;
        }

        if (*curr_line == '*')
            continue;

        if (ciprefix(".model", curr_line)) {
            struct modellist *modl_new;
            modl_new = TMALLOC(struct modellist, 1);
            char *model_type = get_model_type(curr_line);
            modl_new->elemb = inp_get_elem_ident(model_type);
            modl_new->modelname = get_subckt_model_name(curr_line);
            modl_new->model = card;
            modl_new->used = FALSE;
            modl_new->next = card->level->models;
            card->level->models = modl_new;
            tfree(model_type);
        }
    }

    /* scan through all element lines  that require or may need a model */
    for (card = deck; card; card = card->nextcard) {

        char *curr_line = card->line;

        /* exclude any command inside .control ... .endc */
        if (ciprefix(".control", curr_line)) {
            skip_control++;
            continue;
        }
        else if (ciprefix(".endc", curr_line)) {
            skip_control--;
            continue;
        }
        else if (skip_control > 0) {
            continue;
        }

        switch (*curr_line) {
            case '*':
            case '.':
            case 'v':
            case 'i':
            case 'b':
            case 'x':
            case 'e':
            case 'h':
            case 'g':
            case 'f':
            case 'k':
            case 't':
                continue;
                break;
            default:
                break;
        }

        /* check if correct model name */
        int num_terminals = get_number_terminals(curr_line);
        /* num_terminals may be 0 for a elements */
        if ((num_terminals != 0) || (*curr_line == 'a')) {
            char *elem_model_name;
            if (*curr_line == 'a')
                elem_model_name = get_adevice_model_name(curr_line);
            else
                elem_model_name = get_model_name(curr_line, num_terminals);

            /* ignore certain cases, for example
             *    C5 node1 node2 42.0
             */
            if (is_a_modelname(elem_model_name)) {

                struct modellist *m =
                        inp_find_model(card->level, elem_model_name);
                if (m) {
                    if (*curr_line != m->elemb)
                        fprintf(stderr,
                                "warning, model type mismatch in line\n    "
                                "%s\n",
                                curr_line);
                    mark_all_binned(m->model->level, elem_model_name);
                }
                else {
                    fprintf(stderr, "warning, can't find model %s\n",
                            elem_model_name);
                }
            }

            tfree(elem_model_name);
        }
    }

    // disable unused .model lines, and free the models assoc lists
    rem_unused_xxx(root);
}
