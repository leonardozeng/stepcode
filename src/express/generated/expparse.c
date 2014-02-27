/* Driver template for the LEMON parser generator.
** The author disclaims copyright to this source code.
*/
/* First off, code is included that follows the "include" declaration
** in the input grammar file. */
#include <stdio.h>
#line 2 "expparse.y"

#include <assert.h>
#include "token_type.h"
#include "parse_data.h"

int yyerrstatus = 0;
#define yyerrok (yyerrstatus = 0)

YYSTYPE yylval;

    /*
     * YACC grammar for Express parser.
     *
     * This software was developed by U.S. Government employees as part of
     * their official duties and is not subject to copyright.
     *
     * $Log: expparse.y,v $
     * Revision 1.23  1997/11/14 17:09:04  libes
     * allow multiple group references
     *
     * Revision 1.22  1997/01/21 19:35:43  dar
     * made C++ compatible
     *
     * Revision 1.21  1995/06/08  22:59:59  clark
     * bug fixes
     *
     * Revision 1.20  1995/04/08  21:06:07  clark
     * WHERE rule resolution bug fixes, take 2
     *
     * Revision 1.19  1995/04/08  20:54:18  clark
     * WHERE rule resolution bug fixes
     *
     * Revision 1.19  1995/04/08  20:49:08  clark
     * WHERE
     *
     * Revision 1.18  1995/04/05  13:55:40  clark
     * CADDETC preval
     *
     * Revision 1.17  1995/03/09  18:43:47  clark
     * various fixes for caddetc - before interface clause changes
     *
     * Revision 1.16  1994/11/29  20:55:58  clark
     * fix inline comment bug
     *
     * Revision 1.15  1994/11/22  18:32:39  clark
     * Part 11 IS; group reference
     *
     * Revision 1.14  1994/11/10  19:20:03  clark
     * Update to IS
     *
     * Revision 1.13  1994/05/11  19:50:00  libes
     * numerous fixes
     *
     * Revision 1.12  1993/10/15  18:47:26  libes
     * CADDETC certified
     *
     * Revision 1.10  1993/03/19  20:53:57  libes
     * one more, with feeling
     *
     * Revision 1.9  1993/03/19  20:39:51  libes
     * added unique to parameter types
     *
     * Revision 1.8  1993/02/16  03:17:22  libes
     * reorg'd alg bodies to not force artificial begin/ends
     * added flag to differentiate parameters in scopes
     * rewrote query to fix scope handling
     * added support for Where type
     *
     * Revision 1.7  1993/01/19  22:44:17  libes
     * *** empty log message ***
     *
     * Revision 1.6  1992/08/27  23:36:35  libes
     * created fifo for new schemas that are parsed
     * connected entity list to create of oneof
     *
     * Revision 1.5  1992/08/18  17:11:36  libes
     * rm'd extraneous error messages
     *
     * Revision 1.4  1992/06/08  18:05:20  libes
     * prettied up interface to print_objects_when_running
     *
     * Revision 1.3  1992/05/31  23:31:13  libes
     * implemented ALIAS resolution
     *
     * Revision 1.2  1992/05/31  08:30:54  libes
     * multiple files
     *
     * Revision 1.1  1992/05/28  03:52:25  libes
     * Initial revision
     */

#include "express/symbol.h"
#include "express/linklist.h"
#include "stack.h"
#include "express/express.h"
#include "express/schema.h"
#include "express/entity.h"
#include "express/resolve.h"
#include "expscan.h"
#include <float.h>

    extern int print_objects_while_running;

    int tag_count;    /**< use this to count tagged GENERIC types in the formal
                         * argument lists.  Gross, but much easier to do it this
                         * way then with the 'help' of yacc. Set it to -1 to
                         * indicate that tags cannot be defined, only used
                         * (outside of formal parameter list, i.e. for return
                         * types). Hey, as long as there's a gross hack sitting
                         * around, we might as well milk it for all it's worth!
                         *   - snc
                         */

    int local_var_count; /**< used to keep LOCAL variables in order
                            * used in combination with Variable.offset
                            */

    Express yyexpresult;    /* hook to everything built by parser */

    Symbol *interface_schema;    /* schema of interest in use/ref clauses */
    void (*interface_func)();    /* func to attach rename clauses */

    /* record schemas found in a single parse here, allowing them to be */
    /* differentiated from other schemas parsed earlier */
    Linked_List PARSEnew_schemas;

    void SCANskip_to_end_schema(perplex_t scanner);

    int yylineno;

    bool yyeof = false;

#define MAX_SCOPE_DEPTH    20    /* max number of scopes that can be nested */

    static struct scope {
        struct Scope_ *this_;
        char type;    /* one of OBJ_XXX */
        struct scope *pscope;    /* pointer back to most recent scope */
        /* that has a printable name - for better */
        /* error messages */
    } scopes[MAX_SCOPE_DEPTH], *scope;
#define CURRENT_SCOPE (scope->this_)
#define PREVIOUS_SCOPE ((scope-1)->this_)
#define CURRENT_SCHEMA (scope->this_->u.schema)
#define CURRENT_SCOPE_NAME        (OBJget_symbol(scope->pscope->this_,scope->pscope->type)->name)
#define CURRENT_SCOPE_TYPE_PRINTABLE    (OBJget_type(scope->pscope->type))

    /* ths = new scope to enter */
    /* sym = name of scope to enter into parent.  Some scopes (i.e., increment) */
    /*       are not named, in which case sym should be 0 */
    /*     This is useful for when a diagnostic is printed, an earlier named */
    /*      scoped can be used */
    /* typ = type of scope */
#define PUSH_SCOPE(ths,sym,typ) \
    if (sym) DICTdefine(scope->this_->symbol_table,(sym)->name,(Generic)ths,sym,typ);\
    ths->superscope = scope->this_; \
    scope++;        \
    scope->type = typ;    \
    scope->pscope = (sym?scope:(scope-1)->pscope); \
    scope->this_ = ths; \
    if (sym) { \
        ths->symbol = *(sym); \
    }
#define POP_SCOPE() scope--

    /* PUSH_SCOPE_DUMMY just pushes the scope stack with nothing actually on it */
    /* Necessary for situations when a POP_SCOPE is unnecessary but inevitable */
#define PUSH_SCOPE_DUMMY() scope++

    /* normally the superscope is added by PUSH_SCOPE, but some things (types) */
    /* bother to get pushed so fix them this way */
#define SCOPEadd_super(ths) ths->superscope = scope->this_;

#define ERROR(code)    ERRORreport(code, yylineno)

void parserInitState()
{
    scope = scopes;
    /* no need to define scope->this */
    scope->this_ = yyexpresult;
    scope->pscope = scope;
    scope->type = OBJ_EXPRESS;
    yyexpresult->symbol.name = yyexpresult->u.express->filename;
    yyexpresult->symbol.filename = yyexpresult->u.express->filename;
    yyexpresult->symbol.line = 1;
}
#line 195 "expparse.c"
/* Next is all token values, in a form suitable for use by makeheaders.
** This section will be null unless lemon is run with the -m switch.
*/
/* 
** These constants (all generated automatically by the parser generator)
** specify the various kinds of tokens (terminals) that the parser
** understands. 
**
** Each symbol here is a terminal symbol in the grammar.
*/
/* Make sure the INTERFACE macro is defined.
*/
#ifndef INTERFACE
# define INTERFACE 1
#endif
/* The next thing included is series of defines which control
** various aspects of the generated parser.
**    YYCODETYPE         is the data type used for storing terminal
**                       and nonterminal numbers.  "unsigned char" is
**                       used if there are fewer than 250 terminals
**                       and nonterminals.  "int" is used otherwise.
**    YYNOCODE           is a number of type YYCODETYPE which corresponds
**                       to no legal terminal or nonterminal number.  This
**                       number is used to fill in empty slots of the hash 
**                       table.
**    YYFALLBACK         If defined, this indicates that one or more tokens
**                       have fall-back values which should be used if the
**                       original value of the token will not parse.
**    YYACTIONTYPE       is the data type used for storing terminal
**                       and nonterminal numbers.  "unsigned char" is
**                       used if there are fewer than 250 rules and
**                       states combined.  "int" is used otherwise.
**    ParseTOKENTYPE     is the data type used for minor tokens given 
**                       directly to the parser from the tokenizer.
**    YYMINORTYPE        is the data type used for all minor tokens.
**                       This is typically a union of many types, one of
**                       which is ParseTOKENTYPE.  The entry in the union
**                       for base tokens is called "yy0".
**    YYSTACKDEPTH       is the maximum depth of the parser's stack.  If
**                       zero the stack is dynamically sized using realloc()
**    ParseARG_SDECL     A static variable declaration for the %extra_argument
**    ParseARG_PDECL     A parameter declaration for the %extra_argument
**    ParseARG_STORE     Code to store %extra_argument into yypParser
**    ParseARG_FETCH     Code to extract %extra_argument from yypParser
**    YYNSTATE           the combined number of states.
**    YYNRULE            the number of rules in the grammar
**    YYERRORSYMBOL      is the code number of the error symbol.  If not
**                       defined, then do no error processing.
*/
#define YYCODETYPE unsigned short int
#define YYNOCODE 280
#define YYACTIONTYPE unsigned short int
#define ParseTOKENTYPE  YYSTYPE 
typedef union {
  int yyinit;
  ParseTOKENTYPE yy0;
  struct qualifier yy46;
  Variable yy91;
  Op_Code yy126;
  struct entity_body yy176;
  Where yy234;
  struct subsuper_decl yy242;
  struct type_flags yy252;
  struct upper_lower yy253;
  Symbol* yy275;
  Type yy297;
  Case_Item yy321;
  Statement yy332;
  Linked_List yy371;
  struct type_either yy378;
  struct subtypes yy385;
  Expression yy401;
  TypeBody yy477;
  Integer yy507;
} YYMINORTYPE;
#ifndef YYSTACKDEPTH
#define YYSTACKDEPTH 0
#endif
#define ParseARG_SDECL  parse_data_t parseData ;
#define ParseARG_PDECL , parse_data_t parseData 
#define ParseARG_FETCH  parse_data_t parseData  = yypParser->parseData 
#define ParseARG_STORE yypParser->parseData  = parseData 
#define YYNSTATE 645
#define YYNRULE 332
#define YY_NO_ACTION      (YYNSTATE+YYNRULE+2)
#define YY_ACCEPT_ACTION  (YYNSTATE+YYNRULE+1)
#define YY_ERROR_ACTION   (YYNSTATE+YYNRULE)

/* The yyzerominor constant is used to initialize instances of
** YYMINORTYPE objects to zero. */
static const YYMINORTYPE yyzerominor = { 0 };

/* Define the yytestcase() macro to be a no-op if is not already defined
** otherwise.
**
** Applications can choose to define yytestcase() in the %include section
** to a macro that can assist in verifying code coverage.  For production
** code the yytestcase() macro should be turned off.  But it is useful
** for testing.
*/
#ifndef yytestcase
# define yytestcase(X)
#endif


/* Next are the tables used to determine what action to take based on the
** current state and lookahead token.  These tables are used to implement
** functions that take a state number and lookahead value and return an
** action integer.  
**
** Suppose the action integer is N.  Then the action is determined as
** follows
**
**   0 <= N < YYNSTATE                  Shift N.  That is, push the lookahead
**                                      token onto the stack and goto state N.
**
**   YYNSTATE <= N < YYNSTATE+YYNRULE   Reduce by rule N-YYNSTATE.
**
**   N == YYNSTATE+YYNRULE              A syntax error has occurred.
**
**   N == YYNSTATE+YYNRULE+1            The parser accepts its input.
**
**   N == YYNSTATE+YYNRULE+2            No such action.  Denotes unused
**                                      slots in the yy_action[] table.
**
** The action table is constructed as a single large table named yy_action[].
** Given state S and lookahead X, the action is computed as
**
**      yy_action[ yy_shift_ofst[S] + X ]
**
** If the index value yy_shift_ofst[S]+X is out of range or if the value
** yy_lookahead[yy_shift_ofst[S]+X] is not equal to X or if yy_shift_ofst[S]
** is equal to YY_SHIFT_USE_DFLT, it means that the action is not in the table
** and that yy_default[S] should be used instead.  
**
** The formula above is for computing the action when the lookahead is
** a terminal symbol.  If the lookahead is a non-terminal (as occurs after
** a reduce action) then the yy_reduce_ofst[] array is used in place of
** the yy_shift_ofst[] array and YY_REDUCE_USE_DFLT is used in place of
** YY_SHIFT_USE_DFLT.
**
** The following are the tables generated in this section:
**
**  yy_action[]        A single table containing all actions.
**  yy_lookahead[]     A table containing the lookahead for each entry in
**                     yy_action.  Used to detect hash collisions.
**  yy_shift_ofst[]    For each state, the offset into yy_action for
**                     shifting terminals.
**  yy_reduce_ofst[]   For each state, the offset into yy_action for
**                     shifting non-terminals after a reduce.
**  yy_default[]       Default action for each state.
*/
#define YY_ACTTAB_COUNT (2659)
static const YYACTIONTYPE yy_action[] = {
 /*     0 */    77,   78,  614,   67,   68,   45,  380,   71,   69,   70,
 /*    10 */    72,  248,   79,   74,   73,   16,   42,  583,  396,  395,
 /*    20 */    75,  483,  482,  388,  368,  599,   57,   56,  450,  602,
 /*    30 */   268,  597,   60,   35,  596,  379,  594,  598,   66,   89,
 /*    40 */   593,   44,  153,  158,  559,  619,  618,  113,  112,  569,
 /*    50 */    77,   78,  203,  550,  612,  168,  523,  249,  110,  613,
 /*    60 */   306,   15,   79,  611,  108,   16,   42,  175,  621,  606,
 /*    70 */   449,  525,  159,  388,  301,  378,  608,  607,  605,  604,
 /*    80 */   603,  405,  408,  183,  409,  179,  407,  169,   66,  387,
 /*    90 */    87,  978,  118,  403,  203,  619,  618,  113,  112,  401,
 /*   100 */    77,   78,  612,  544,  612,   60,  518,  244,  535,  613,
 /*   110 */   170,  611,   79,  611,  144,   16,   42,  549,   39,  606,
 /*   120 */   396,  395,   75,  388,  301,   82,  608,  607,  605,  604,
 /*   130 */   603,  524,  467,  454,  466,  469,  864,  465,   66,  387,
 /*   140 */   350,  468,  526,  234,  633,  619,  618,  396,  348,   75,
 /*   150 */    77,   78,   73,  132,  612,  467,  470,  466,  469,  613,
 /*   160 */   465,  311,   79,  611,  468,   16,   42,  346,  616,  606,
 /*   170 */   447,  122,  333,  388,  247,  224,  608,  607,  605,  604,
 /*   180 */   603,  467,  464,  466,  469,  550,  465,  550,   66,  387,
 /*   190 */   468,  227,  167,  113,  112,  619,  618,  113,  112,  557,
 /*   200 */    77,   78,  516,  346,  612,  467,  463,  466,  469,  613,
 /*   210 */   465,   39,   79,  611,  468,   16,   42,  359,  237,  606,
 /*   220 */   864,  122,  442,  312,  445,  615,  608,  607,  605,  604,
 /*   230 */   603,  367,  365,  361,  402,  731,  111,  545,   66,  387,
 /*   240 */   405,  209,  171,  373,  170,  619,  618,  337,  154,  549,
 /*   250 */   102,  549,  644,  251,  612,  777,  510,  334,   36,  613,
 /*   260 */    67,   68,  250,  611,   71,   69,   70,   72,  479,  606,
 /*   270 */    74,   73,  137,  144,  114,  344,  608,  607,  605,  604,
 /*   280 */   603,  589,  587,  590,  131,  585,  584,  588,  591,  387,
 /*   290 */   586,   67,   68,  514,  130,   71,   69,   70,   72,  609,
 /*   300 */   125,   74,   73,  777,  115,  154,  222,  620,  510,   23,
 /*   310 */   114,  473,  386,  510,  402,  496,  495,  494,  493,  492,
 /*   320 */   491,  490,  489,  488,  487,    2,  302,  512,  569,  322,
 /*   330 */   129,  318,  165,  373,  163,  623,  245,  243,  576,  575,
 /*   340 */   242,  240,  543,  527,  315,  451,  223,   29,  154,  215,
 /*   350 */   356,  236,  625,   19,   26,  626,  510,    3,  627,  632,
 /*   360 */   631,  521,  630,  629,  642,  162,  161,  343,  218,    5,
 /*   370 */   385,  286,  496,  495,  494,  493,  492,  491,  490,  489,
 /*   380 */   488,  487,    2,   71,   69,   70,   72,  129,   43,   74,
 /*   390 */    73,  431,  154,  355,  432,  430,  525,  433,  632,  631,
 /*   400 */   510,  630,  629,   41,  428,   39,   14,  204,   12,  134,
 /*   410 */   517,   13,   84,  107,    3,  496,  495,  494,  493,  492,
 /*   420 */   491,  490,  489,  488,  487,    2,  550,  612,  642,  429,
 /*   430 */   129,  642,  542,  520,   67,   68,  611,  304,   71,   69,
 /*   440 */    70,   72,  154,  298,   74,   73,  103,  335,  521,   40,
 /*   450 */   510,   39,  581,   63,  190,  521,  216,    3,  232,  496,
 /*   460 */   495,  494,  493,  492,  491,  490,  489,  488,  487,    2,
 /*   470 */   435,   67,   68,  335,  129,   71,   69,   70,   72,   91,
 /*   480 */   335,   74,   73,  434,   90,  154,  223,  354,  421,  580,
 /*   490 */   548,  640,  316,  510,  563,  559,  362,  641,  639,  638,
 /*   500 */    39,    3,  637,  636,  635,  634,  117,  229,  238,  496,
 /*   510 */   495,  494,  493,  492,  491,  490,  489,  488,  487,    2,
 /*   520 */   522,  121,   85,  521,  129,  185,  378,  519,  186,  154,
 /*   530 */   352,  401,   39,  309,  569,  331,  503,  510,  246,  164,
 /*   540 */   174,  623,  245,  243,  576,  575,  242,  240,   10,  349,
 /*   550 */   562,    3,  496,  495,  494,  493,  492,  491,  490,  489,
 /*   560 */   488,  487,    2,  330,  308,  551,  556,  129,   39,  628,
 /*   570 */   625,  173,  172,  626,  486,  100,  627,  632,  631,  154,
 /*   580 */   630,  629,  413,  362,   39,  182,   39,  510,  551,  425,
 /*   590 */   362,  202,  310,   98,    3,  520,  496,  495,  494,  493,
 /*   600 */   492,  491,  490,  489,  488,  487,    2,  135,   76,  377,
 /*   610 */   329,  129,  467,  462,  466,  469,  299,  465,  415,  297,
 /*   620 */   199,  468,  154,  376,  485,  375,   21,  558,  624,  625,
 /*   630 */   510,  560,  626,  529,  374,  627,  632,  631,    3,  630,
 /*   640 */   629,  120,   24,  126,  369,  140,  496,  495,  494,  493,
 /*   650 */   492,  491,  490,  489,  488,  487,    2,  529,  362,   14,
 /*   660 */   204,  129,  228,  353,   13,  378,  327,  351,  231,   53,
 /*   670 */    51,   54,   47,   49,   48,   52,   55,   46,   50,   14,
 /*   680 */   204,   57,   56,  230,   13,  402,  332,   60,    3,  496,
 /*   690 */   495,  494,  493,  492,  491,  490,  489,  488,  487,    2,
 /*   700 */   467,  461,  466,  469,  129,  465,   14,  204,  225,  468,
 /*   710 */   642,   13,  366,  188,  642,  315,  363,  444,  617,  364,
 /*   720 */    53,   51,   54,   47,   49,   48,   52,   55,   46,   50,
 /*   730 */   109,    3,   57,   56,  104,  360,  541,  106,   60,  515,
 /*   740 */   357,  221,    9,   20,  478,  477,  476,  601,  370,   27,
 /*   750 */   116,  220,  217,  212,   32,  637,  636,  635,  634,  117,
 /*   760 */   207,   18,    9,   20,  478,  477,  476,  347,  866,  206,
 /*   770 */    80,   25,  205,  342,   97,  637,  636,  635,  634,  117,
 /*   780 */   460,  201,   95,  160,   92,  336,   93,  198,  331,    9,
 /*   790 */    20,  478,  477,  476,  453,  197,  193,  192,  136,  426,
 /*   800 */   324,  187,  637,  636,  635,  634,  117,  189,  331,  323,
 /*   810 */    53,   51,   54,   47,   49,   48,   52,   55,   46,   50,
 /*   820 */   418,  321,   57,   56,  467,  459,  466,  469,   60,  465,
 /*   830 */   184,  416,  177,  468,  319,  331,  180,  176,  123,   58,
 /*   840 */   317,   53,   51,   54,   47,   49,   48,   52,   55,   46,
 /*   850 */    50,  441,    8,   57,   56,  196,   11,  643,  642,   60,
 /*   860 */   143,   53,   51,   54,   47,   49,   48,   52,   55,   46,
 /*   870 */    50,  325,  400,   57,   56,   39,   67,   68,   61,   60,
 /*   880 */    71,   69,   70,   72,  582,  622,   74,   73,  595,   21,
 /*   890 */    53,   51,   54,   47,   49,   48,   52,   55,   46,   50,
 /*   900 */   577,  574,   57,   56,  467,  458,  466,  469,   60,  465,
 /*   910 */   241,  573,  572,  468,   59,  239,  566,  578,  235,   53,
 /*   920 */    51,   54,   47,   49,   48,   52,   55,   46,   50,   37,
 /*   930 */    86,   57,   56,  119,   83,  569,  561,   60,  467,  457,
 /*   940 */   466,  469,  600,  465,  233,  141,  540,  468,   38,  105,
 /*   950 */    53,   51,   54,   47,   49,   48,   52,   55,   46,   50,
 /*   960 */    81,  533,   57,   56,  530,  115,  536,  539,   60,  359,
 /*   970 */   467,  456,  466,  469,  538,  465,  445,  537,  571,  468,
 /*   980 */    53,   51,   54,   47,   49,   48,   52,   55,   46,   50,
 /*   990 */   384,  599,   57,   56,  532,  602,  278,  597,   60,  253,
 /*  1000 */   596,  367,  594,  598,  531,  251,  593,   44,  153,  158,
 /*  1010 */    28,  528,  142,  254,   53,   51,   54,   47,   49,   48,
 /*  1020 */    52,   55,   46,   50,  137,  513,   57,   56,  508,  507,
 /*  1030 */   214,  506,   60,   27,   53,   51,   54,   47,   49,   48,
 /*  1040 */    52,   55,   46,   50,  505,  504,   57,   56,    4,  213,
 /*  1050 */   500,  498,   60,  497,   53,   51,   54,   47,   49,   48,
 /*  1060 */    52,   55,   46,   50,  208,    1,   57,   56,  484,   14,
 /*  1070 */   204,  480,   60,  244,   13,  467,  455,  466,  469,  472,
 /*  1080 */   465,  211,  446,  139,  468,  303,  452,  448,   99,    6,
 /*  1090 */    96,   53,   51,   54,   47,   49,   48,   52,   55,   46,
 /*  1100 */    50,  438,  195,   57,   56,  443,  252,  440,  194,   60,
 /*  1110 */   124,  439,  437,  436,   31,  191,   53,   51,   54,   47,
 /*  1120 */    49,   48,   52,   55,   46,   50,  599,  300,   57,   56,
 /*  1130 */   602,  427,  597,  326,   60,  596,  424,  594,  598,   17,
 /*  1140 */   423,  593,   44,  154,  157,  422,  420,  419,  417,  133,
 /*  1150 */   181,  510,  475,   20,  478,  477,  476,  414,  320,  412,
 /*  1160 */   411,  410,  406,   30,  154,  637,  636,  635,  634,  117,
 /*  1170 */   599,  178,  510,  521,  602,  151,  597,   88,  399,  596,
 /*  1180 */   404,  594,  598,  285,  547,  593,   44,  153,  158,  382,
 /*  1190 */   381,  546,  372,  371,  467,  200,  466,  469,  331,  465,
 /*  1200 */   534,  642,  338,  468,  499,  101,   22,  339,  244,   94,
 /*  1210 */   496,  495,  494,  493,  492,  491,  490,  489,  488,  487,
 /*  1220 */   509,  467,  127,  466,  469,  129,  465,  340,  341,  138,
 /*  1230 */   468,  496,  495,  494,  493,  492,  491,  490,  489,  488,
 /*  1240 */   487,  481,   62,  383,  599,  520,  129,  610,  602,  278,
 /*  1250 */   597,  592,  244,  596,  512,  594,  598,  501,  471,  593,
 /*  1260 */    44,  153,  158,  555,   53,   51,   54,   47,   49,   48,
 /*  1270 */    52,   55,   46,   50,  553,  314,   57,   56,  554,  599,
 /*  1280 */    65,  511,   60,  602,  151,  597,  474,   64,  596,  328,
 /*  1290 */   594,  598,  979,  979,  593,   44,  153,  158,  599,  305,
 /*  1300 */   358,  521,  602,  276,  597,  979,  979,  596,  362,  594,
 /*  1310 */   598,  307,  979,  593,   44,  153,  158,   34,  979,    7,
 /*  1320 */   979,  979,  979,  979,  979,  979,  244,  979,  979,  219,
 /*  1330 */   612,  979,  979,  979,  570,  625,  979,  313,  626,  611,
 /*  1340 */    33,  627,  632,  631,  569,  630,  629,  979,  246,  979,
 /*  1350 */   174,  623,  245,  243,  576,  575,  242,  240,  979,  979,
 /*  1360 */   979,  244,  979,  979,  502,  979,  979,  979,  128,  979,
 /*  1370 */   166,  979,  979,  520,  979,  979,  642,  210,  979,  979,
 /*  1380 */   244,  173,  172,  552,  599,  979,  979,  979,  602,  261,
 /*  1390 */   597,  979,  979,  596,  979,  594,  598,  979,  979,  593,
 /*  1400 */    44,  153,  158,  599,  979,  979,  521,  602,  579,  597,
 /*  1410 */   979,  979,  596,  979,  594,  598,  284,  979,  593,   44,
 /*  1420 */   153,  158,  599,  979,  979,  979,  602,  266,  597,  979,
 /*  1430 */   979,  596,  979,  594,  598,  979,  362,  593,   44,  153,
 /*  1440 */   158,  599,  979,  979,  979,  602,  265,  597,  979,  979,
 /*  1450 */   596,  979,  594,  598,  979,  979,  593,   44,  153,  158,
 /*  1460 */   979,  979,  979,  979,  979,  599,  244,  979,  979,  602,
 /*  1470 */   398,  597,  979,  979,  596,  979,  594,  598,  520,  979,
 /*  1480 */   593,   44,  153,  158,  599,  244,  979,  979,  602,  397,
 /*  1490 */   597,  979,  979,  596,  979,  594,  598,  979,  979,  593,
 /*  1500 */    44,  153,  158,  979,  244,  979,  979,  979,  979,  599,
 /*  1510 */   979,  979,  979,  602,  296,  597,  979,  979,  596,  979,
 /*  1520 */   594,  598,  979,  244,  593,   44,  153,  158,  599,  979,
 /*  1530 */   979,  979,  602,  295,  597,  979,  979,  596,  979,  594,
 /*  1540 */   598,  362,  979,  593,   44,  153,  158,  244,  979,  979,
 /*  1550 */   979,  979,  979,  979,  979,  979,  599,  979,  979,  979,
 /*  1560 */   602,  294,  597,  979,  979,  596,  244,  594,  598,  979,
 /*  1570 */   979,  593,   44,  153,  158,  979,  979,  979,  599,  979,
 /*  1580 */   979,  979,  602,  293,  597,  979,  979,  596,  979,  594,
 /*  1590 */   598,  244,  979,  593,   44,  153,  158,  599,  979,  979,
 /*  1600 */   979,  602,  292,  597,  979,  979,  596,  979,  594,  598,
 /*  1610 */   244,  979,  593,   44,  153,  158,  599,  979,  979,  979,
 /*  1620 */   602,  291,  597,  979,  979,  596,  979,  594,  598,  979,
 /*  1630 */   979,  593,   44,  153,  158,  979,  979,  599,  244,  979,
 /*  1640 */   979,  602,  290,  597,  979,  979,  596,  979,  594,  598,
 /*  1650 */   979,  979,  593,   44,  153,  158,  979,  979,  979,  979,
 /*  1660 */   244,  979,  979,  979,  979,  599,  979,  979,  979,  602,
 /*  1670 */   289,  597,  979,  979,  596,  979,  594,  598,  979,  244,
 /*  1680 */   593,   44,  153,  158,  599,  979,  979,  979,  602,  288,
 /*  1690 */   597,  979,  979,  596,  979,  594,  598,  979,  244,  593,
 /*  1700 */    44,  153,  158,  979,  979,  979,  599,  979,  979,  979,
 /*  1710 */   602,  287,  597,  979,  979,  596,  979,  594,  598,  244,
 /*  1720 */   979,  593,   44,  153,  158,  599,  979,  979,  979,  602,
 /*  1730 */   277,  597,  979,  979,  596,  979,  594,  598,  979,  979,
 /*  1740 */   593,   44,  153,  158,  599,  979,  979,  244,  602,  264,
 /*  1750 */   597,  979,  979,  596,  979,  594,  598,  979,  979,  593,
 /*  1760 */    44,  153,  158,  979,  979,  599,  244,  979,  979,  602,
 /*  1770 */   263,  597,  979,  979,  596,  979,  594,  598,  979,  979,
 /*  1780 */   593,   44,  153,  158,  979,  979,  979,  979,  244,  979,
 /*  1790 */   979,  979,  979,  599,  979,  979,  979,  602,  262,  597,
 /*  1800 */   979,  979,  596,  979,  594,  598,  979,  244,  593,   44,
 /*  1810 */   153,  158,  599,  979,  979,  979,  602,  275,  597,  979,
 /*  1820 */   979,  596,  979,  594,  598,  979,  244,  593,   44,  153,
 /*  1830 */   158,  979,  979,  979,  599,  979,  979,  979,  602,  274,
 /*  1840 */   597,  979,  979,  596,  979,  594,  598,  244,  979,  593,
 /*  1850 */    44,  153,  158,  599,  979,  979,  979,  602,  260,  597,
 /*  1860 */   979,  979,  596,  979,  594,  598,  979,  979,  593,   44,
 /*  1870 */   153,  158,  599,  979,  979,  244,  602,  259,  597,  979,
 /*  1880 */   979,  596,  979,  594,  598,  979,  979,  593,   44,  153,
 /*  1890 */   158,  979,  979,  599,  244,  979,  979,  602,  273,  597,
 /*  1900 */   979,  979,  596,  979,  594,  598,  979,  979,  593,   44,
 /*  1910 */   153,  158,  979,  979,  979,  979,  244,  979,  979,  979,
 /*  1920 */   979,  599,  979,  979,  979,  602,  150,  597,  979,  979,
 /*  1930 */   596,  979,  594,  598,  979,  244,  593,   44,  153,  158,
 /*  1940 */   599,  979,  979,  979,  602,  149,  597,  979,  979,  596,
 /*  1950 */   979,  594,  598,  979,  244,  593,   44,  153,  158,  979,
 /*  1960 */   979,  979,  599,  979,  979,  979,  602,  258,  597,  979,
 /*  1970 */   979,  596,  979,  594,  598,  244,  979,  593,   44,  153,
 /*  1980 */   158,  599,  979,  979,  979,  602,  257,  597,  979,  979,
 /*  1990 */   596,  979,  594,  598,  979,  979,  593,   44,  153,  158,
 /*  2000 */   599,  979,  979,  244,  602,  256,  597,  979,  979,  596,
 /*  2010 */   979,  594,  598,  979,  979,  593,   44,  153,  158,  979,
 /*  2020 */   979,  599,  244,  979,  979,  602,  148,  597,  979,  979,
 /*  2030 */   596,  979,  594,  598,  979,  979,  593,   44,  153,  158,
 /*  2040 */   979,  979,  979,  979,  244,  979,  979,  979,  979,  599,
 /*  2050 */   979,  979,  979,  602,  272,  597,  979,  979,  596,  979,
 /*  2060 */   594,  598,  979,  244,  593,   44,  153,  158,  599,  979,
 /*  2070 */   979,  979,  602,  255,  597,  979,  979,  596,  979,  594,
 /*  2080 */   598,  979,  244,  593,   44,  153,  158,  979,  979,  979,
 /*  2090 */   599,  979,  979,  979,  602,  271,  597,  979,  979,  596,
 /*  2100 */   979,  594,  598,  244,  979,  593,   44,  153,  158,  599,
 /*  2110 */   979,  979,  979,  602,  270,  597,  979,  979,  596,  979,
 /*  2120 */   594,  598,  979,  979,  593,   44,  153,  158,  599,  979,
 /*  2130 */   979,  244,  602,  269,  597,  979,  979,  596,  979,  594,
 /*  2140 */   598,  979,  979,  593,   44,  153,  158,  979,  979,  599,
 /*  2150 */   244,  979,  979,  602,  147,  597,  979,  979,  596,  979,
 /*  2160 */   594,  598,  979,  979,  593,   44,  153,  158,  979,  979,
 /*  2170 */   979,  979,  244,  979,  979,  979,  979,  599,  305,  358,
 /*  2180 */   979,  602,  267,  597,  979,  979,  596,  979,  594,  598,
 /*  2190 */   979,  244,  593,   44,  153,  158,   34,  979,    7,  979,
 /*  2200 */   979,  979,  979,  979,  979,  979,  979,  979,  219,  612,
 /*  2210 */   244,  599,  979,  979,  979,  602,  979,  597,  611,   33,
 /*  2220 */   596,  979,  594,  598,  979,  979,  593,   44,  279,  158,
 /*  2230 */   979,  244,  979,  979,  979,  979,  979,  979,  979,  979,
 /*  2240 */   979,  979,  979,  502,  979,  979,  599,  128,  979,  166,
 /*  2250 */   602,  979,  597,  979,  979,  596,  210,  594,  598,  244,
 /*  2260 */   979,  593,   44,  394,  158,  599,  979,  979,  979,  602,
 /*  2270 */   979,  597,  979,  979,  596,  979,  594,  598,  979,  979,
 /*  2280 */   593,   44,  393,  158,  979,  979,  599,  979,  979,  979,
 /*  2290 */   602,  979,  597,  244,  979,  596,  979,  594,  598,  979,
 /*  2300 */   979,  593,   44,  392,  158,  599,  979,  979,  979,  602,
 /*  2310 */   979,  597,  979,  979,  596,  979,  594,  598,  979,  979,
 /*  2320 */   593,   44,  391,  158,  979,  979,  599,  979,  244,  979,
 /*  2330 */   602,  979,  597,  979,  979,  596,  979,  594,  598,  979,
 /*  2340 */   979,  593,   44,  390,  158,  599,  979,  244,  979,  602,
 /*  2350 */   979,  597,  979,  979,  596,  979,  594,  598,  979,  979,
 /*  2360 */   593,   44,  389,  158,  979,  979,  979,  979,  244,  979,
 /*  2370 */   979,  979,  979,  979,  599,  979,  979,  979,  602,  979,
 /*  2380 */   597,  979,  979,  596,  979,  594,  598,  244,  979,  593,
 /*  2390 */    44,  283,  158,  979,  979,  568,  625,  979,  979,  626,
 /*  2400 */   979,  979,  627,  632,  631,  599,  630,  629,  244,  602,
 /*  2410 */   979,  597,  979,  979,  596,  979,  594,  598,  979,  979,
 /*  2420 */   593,   44,  282,  158,  979,  599,  979,  244,  979,  602,
 /*  2430 */   979,  597,  979,  979,  596,  979,  594,  598,  979,  979,
 /*  2440 */   593,   44,  146,  158,  979,  979,  979,  979,  599,  979,
 /*  2450 */   979,  979,  602,  979,  597,  979,  244,  596,  979,  594,
 /*  2460 */   598,  979,  979,  593,   44,  145,  158,  979,  979,  979,
 /*  2470 */   979,  979,  979,  599,  979,  979,  979,  602,  979,  597,
 /*  2480 */   979,  979,  596,  979,  594,  598,  979,  244,  593,   44,
 /*  2490 */   152,  158,  567,  625,  979,  979,  626,  979,  979,  627,
 /*  2500 */   632,  631,  599,  630,  629,  979,  602,  244,  597,  979,
 /*  2510 */   979,  596,  979,  594,  598,  979,  979,  593,   44,  280,
 /*  2520 */   158,  979,  979,  979,  979,  979,  979,  979,  979,  979,
 /*  2530 */   244,  979,  979,  599,  979,  979,  979,  602,  979,  597,
 /*  2540 */   979,  979,  596,  979,  594,  598,  979,  979,  593,   44,
 /*  2550 */   281,  158,  979,  599,  979,  244,  979,  602,  979,  597,
 /*  2560 */   979,  979,  596,  979,  594,  598,  979,  979,  593,   44,
 /*  2570 */   599,  156,  979,  979,  602,  979,  597,  979,  979,  596,
 /*  2580 */   979,  594,  598,  979,  244,  593,   44,  979,  155,  979,
 /*  2590 */   565,  625,  979,  979,  626,  979,  979,  627,  632,  631,
 /*  2600 */   979,  630,  629,  979,  979,  979,  979,  979,  979,  979,
 /*  2610 */   979,  979,  979,  979,  979,  244,  564,  625,  979,  979,
 /*  2620 */   626,  979,  979,  627,  632,  631,  979,  630,  629,  226,
 /*  2630 */   625,  979,  979,  626,  979,  244,  627,  632,  631,  979,
 /*  2640 */   630,  629,  979,  979,  979,  979,  345,  625,  979,  979,
 /*  2650 */   626,  979,  244,  627,  632,  631,  979,  630,  629,
};
static const YYCODETYPE yy_lookahead[] = {
 /*     0 */    11,   12,   28,   11,   12,   31,   66,   15,   16,   17,
 /*    10 */    18,   80,   23,   21,   22,   26,   27,   28,   24,   25,
 /*    20 */    26,  123,  124,   34,  125,  127,   13,   14,  167,  131,
 /*    30 */   132,  133,   19,   39,  136,   95,  138,  139,   49,   30,
 /*    40 */   142,  143,  144,  145,   34,   56,   57,   19,   20,   34,
 /*    50 */    11,   12,  191,  129,   65,   40,   28,  235,  159,   70,
 /*    60 */   162,  239,   23,   74,   27,   26,   27,   33,   29,   80,
 /*    70 */   167,   34,  169,   34,  170,   65,   87,   88,   89,   90,
 /*    80 */    91,  241,  260,  261,  262,  263,  264,   72,   49,  100,
 /*    90 */    33,  251,  252,  253,  191,   56,   57,   19,   20,  129,
 /*   100 */    11,   12,   65,  179,   65,   19,   28,  209,  129,   70,
 /*   110 */   186,   74,   23,   74,  274,   26,   27,  193,   26,   80,
 /*   120 */    24,   25,   26,   34,  170,   33,   87,   88,   89,   90,
 /*   130 */    91,   94,  212,  213,  214,  215,   27,  217,   49,  100,
 /*   140 */    51,  221,  174,  164,  165,   56,   57,   24,   25,   26,
 /*   150 */    11,   12,   22,  183,   65,  212,  213,  214,  215,   70,
 /*   160 */   217,  182,   23,   74,  221,   26,   27,  136,   34,   80,
 /*   170 */   168,  267,  268,   34,  206,  207,   87,   88,   89,   90,
 /*   180 */    91,  212,  213,  214,  215,  129,  217,  129,   49,  100,
 /*   190 */   221,   30,   31,   19,   20,   56,   57,   19,   20,  229,
 /*   200 */    11,   12,   28,  136,   65,  212,  213,  214,  215,   70,
 /*   210 */   217,   26,   23,   74,  221,   26,   27,   62,   33,   80,
 /*   220 */   111,  267,  268,   34,   69,   34,   87,   88,   89,   90,
 /*   230 */    91,  113,  114,  115,   79,    0,  178,  179,   49,  100,
 /*   240 */   241,  148,  186,  129,  186,   56,   57,   30,  128,  193,
 /*   250 */    33,  193,  253,   98,   65,   27,  136,  255,   30,   70,
 /*   260 */    11,   12,  107,   74,   15,   16,   17,   18,  135,   80,
 /*   270 */    21,   22,  117,  274,  243,  244,   87,   88,   89,   90,
 /*   280 */    91,    1,    2,    3,   31,    5,    6,    7,    8,  100,
 /*   290 */    10,   11,   12,  173,  180,   15,   16,   17,   18,   50,
 /*   300 */   128,   21,   22,   27,   58,  128,  134,   29,  136,   31,
 /*   310 */   243,  244,   27,  136,   79,  195,  196,  197,  198,  199,
 /*   320 */   200,  201,  202,  203,  204,  205,   32,  194,   34,   83,
 /*   330 */   210,   85,   38,  129,   40,   41,   42,   43,   44,   45,
 /*   340 */    46,   47,  228,   28,  109,   28,   31,   27,  128,  256,
 /*   350 */   173,  211,  212,   30,   31,  215,  136,  237,  218,  219,
 /*   360 */   220,  136,  222,  223,  111,   71,   72,   73,   77,   78,
 /*   370 */    34,  146,  195,  196,  197,  198,  199,  200,  201,  202,
 /*   380 */   203,  204,  205,   15,   16,   17,   18,  210,  101,   21,
 /*   390 */    22,  212,  128,  173,  215,  216,   34,  218,  219,  220,
 /*   400 */   136,  222,  223,   30,  225,   26,  149,  150,  151,  152,
 /*   410 */    28,  154,   33,   31,  237,  195,  196,  197,  198,  199,
 /*   420 */   200,  201,  202,  203,  204,  205,  129,   65,  111,  250,
 /*   430 */   210,  111,  228,  208,   11,   12,   74,  173,   15,   16,
 /*   440 */    17,   18,  128,  185,   21,   22,   30,   31,  136,   30,
 /*   450 */   136,   26,   29,   30,  275,  136,  157,  237,   33,  195,
 /*   460 */   196,  197,  198,  199,  200,  201,  202,  203,  204,  205,
 /*   470 */    28,   11,   12,   31,  210,   15,   16,   17,   18,   30,
 /*   480 */    31,   21,   22,   28,  265,  128,   31,  173,  230,   29,
 /*   490 */   193,  234,  273,  136,   95,   34,  271,  240,  241,  242,
 /*   500 */    26,  237,  245,  246,  247,  248,  249,   33,   33,  195,
 /*   510 */   196,  197,  198,  199,  200,  201,  202,  203,  204,  205,
 /*   520 */   208,   60,   33,  136,  210,   28,   65,  208,   31,  128,
 /*   530 */   173,  129,   26,  146,   34,  278,  237,  136,   38,   33,
 /*   540 */    40,   41,   42,   43,   44,   45,   46,   47,  160,  161,
 /*   550 */    66,  237,  195,  196,  197,  198,  199,  200,  201,  202,
 /*   560 */   203,  204,  205,   63,  177,  175,  176,  210,   26,  211,
 /*   570 */   212,   71,   72,  215,  173,   33,  218,  219,  220,  128,
 /*   580 */   222,  223,   28,  271,   26,   31,   26,  136,  175,  176,
 /*   590 */   271,   33,  171,   33,  237,  208,  195,  196,  197,  198,
 /*   600 */   199,  200,  201,  202,  203,  204,  205,  276,  277,   25,
 /*   610 */   110,  210,  212,  213,  214,  215,  171,  217,  257,  258,
 /*   620 */   140,  221,  128,   34,  173,   24,   27,   34,  211,  212,
 /*   630 */   136,  229,  215,  212,   34,  218,  219,  220,  237,  222,
 /*   640 */   223,   30,   39,   30,   36,   27,  195,  196,  197,  198,
 /*   650 */   199,  200,  201,  202,  203,  204,  205,  212,  271,  149,
 /*   660 */   150,  210,   34,   34,  154,   65,  156,  173,   33,    1,
 /*   670 */     2,    3,    4,    5,    6,    7,    8,    9,   10,  149,
 /*   680 */   150,   13,   14,   33,  154,   79,  156,   19,  237,  195,
 /*   690 */   196,  197,  198,  199,  200,  201,  202,  203,  204,  205,
 /*   700 */   212,  213,  214,  215,  210,  217,  149,  150,   61,  221,
 /*   710 */   111,  154,   33,  156,  111,  109,   33,  237,   50,  115,
 /*   720 */     1,    2,    3,    4,    5,    6,    7,    8,    9,   10,
 /*   730 */    27,  237,   13,   14,   27,   33,  212,   27,   19,   34,
 /*   740 */    34,   37,  232,  233,  234,  235,  236,   28,  224,  120,
 /*   750 */    36,   55,   77,  104,   39,  245,  246,  247,  248,  249,
 /*   760 */   104,   30,  232,  233,  234,  235,  236,   34,  111,   53,
 /*   770 */    30,   39,   59,   30,   33,  245,  246,  247,  248,  249,
 /*   780 */    34,   33,   33,   33,   30,   34,   33,   93,  278,  232,
 /*   790 */   233,  234,  235,  236,   34,   97,  116,   33,   27,    1,
 /*   800 */    34,  106,  245,  246,  247,  248,  249,   68,  278,   36,
 /*   810 */     1,    2,    3,    4,    5,    6,    7,    8,    9,   10,
 /*   820 */    27,   84,   13,   14,  212,  213,  214,  215,   19,  217,
 /*   830 */    34,   34,  108,  221,   82,  278,   34,   34,  269,   30,
 /*   840 */    84,    1,    2,    3,    4,    5,    6,    7,    8,    9,
 /*   850 */    10,  270,  238,   13,   14,  155,  239,  237,  111,   19,
 /*   860 */   237,    1,    2,    3,    4,    5,    6,    7,    8,    9,
 /*   870 */    10,  153,  227,   13,   14,   26,   11,   12,   27,   19,
 /*   880 */    15,   16,   17,   18,  157,  141,   21,   22,   28,   27,
 /*   890 */     1,    2,    3,    4,    5,    6,    7,    8,    9,   10,
 /*   900 */   141,  189,   13,   14,  212,  213,  214,  215,   19,  217,
 /*   910 */   141,   96,  189,  221,   49,  141,   95,   28,  137,    1,
 /*   920 */     2,    3,    4,    5,    6,    7,    8,    9,   10,   39,
 /*   930 */   192,   13,   14,   86,  192,   34,  237,   19,  212,  213,
 /*   940 */   214,  215,  102,  217,  181,  184,  212,  221,   30,   95,
 /*   950 */     1,    2,    3,    4,    5,    6,    7,    8,    9,   10,
 /*   960 */   190,   66,   13,   14,  174,   58,  237,  212,   19,   62,
 /*   970 */   212,  213,  214,  215,  212,  217,   69,  212,   29,  221,
 /*   980 */     1,    2,    3,    4,    5,    6,    7,    8,    9,   10,
 /*   990 */   126,  127,   13,   14,  237,  131,  132,  133,   19,   92,
 /*  1000 */   136,  113,  138,  139,  237,   98,  142,  143,  144,  145,
 /*  1010 */   118,  212,   33,  237,    1,    2,    3,    4,    5,    6,
 /*  1020 */     7,    8,    9,   10,  117,  237,   13,   14,  237,  237,
 /*  1030 */   148,  237,   19,  120,    1,    2,    3,    4,    5,    6,
 /*  1040 */     7,    8,    9,   10,  237,  237,   13,   14,  237,  147,
 /*  1050 */   237,  237,   19,  237,    1,    2,    3,    4,    5,    6,
 /*  1060 */     7,    8,    9,   10,  147,  237,   13,   14,  237,  149,
 /*  1070 */   150,  237,   19,  209,  154,  212,  213,  214,  215,  237,
 /*  1080 */   217,   28,   34,  254,  221,  170,   34,  237,  192,   76,
 /*  1090 */   192,    1,    2,    3,    4,    5,    6,    7,    8,    9,
 /*  1100 */    10,   34,  272,   13,   14,  237,  237,  237,  168,   19,
 /*  1110 */    27,  237,  237,  172,   81,   27,    1,    2,    3,    4,
 /*  1120 */     5,    6,    7,    8,    9,   10,  127,  170,   13,   14,
 /*  1130 */   131,  237,  133,  175,   19,  136,  237,  138,  139,  119,
 /*  1140 */   237,  142,  143,  128,  145,   34,  230,  237,  237,   27,
 /*  1150 */   259,  136,  232,  233,  234,  235,  236,  257,   34,  237,
 /*  1160 */   237,  237,  237,   48,  128,  245,  246,  247,  248,  249,
 /*  1170 */   127,  259,  136,  136,  131,  132,  133,  188,  227,  136,
 /*  1180 */   237,  138,  139,  146,  237,  142,  143,  144,  145,  227,
 /*  1190 */   227,  237,  227,  227,  212,  213,  214,  215,  278,  217,
 /*  1200 */   129,  111,  227,  221,  237,  188,  163,  227,  209,  188,
 /*  1210 */   195,  196,  197,  198,  199,  200,  201,  202,  203,  204,
 /*  1220 */   205,  212,  213,  214,  215,  210,  217,  227,  227,  237,
 /*  1230 */   221,  195,  196,  197,  198,  199,  200,  201,  202,  203,
 /*  1240 */   204,  205,  226,  126,  127,  208,  210,  266,  131,  132,
 /*  1250 */   133,  194,  209,  136,  194,  138,  139,  130,   67,  142,
 /*  1260 */   143,  144,  145,  237,    1,    2,    3,    4,    5,    6,
 /*  1270 */     7,    8,    9,   10,  231,  158,   13,   14,  237,  127,
 /*  1280 */   187,  237,   19,  131,  132,  133,  237,  187,  136,   34,
 /*  1290 */   138,  139,  279,  279,  142,  143,  144,  145,  127,   34,
 /*  1300 */    35,  136,  131,  132,  133,  279,  279,  136,  271,  138,
 /*  1310 */   139,  146,  279,  142,  143,  144,  145,   52,  279,   54,
 /*  1320 */   279,  279,  279,  279,  279,  279,  209,  279,  279,   64,
 /*  1330 */    65,  279,  279,  279,  211,  212,  279,  166,  215,   74,
 /*  1340 */    75,  218,  219,  220,   34,  222,  223,  279,   38,  279,
 /*  1350 */    40,   41,   42,   43,   44,   45,   46,   47,  279,  279,
 /*  1360 */   279,  209,  279,  279,   99,  279,  279,  279,  103,  279,
 /*  1370 */   105,  279,  279,  208,  279,  279,  111,  112,  279,  279,
 /*  1380 */   209,   71,   72,  231,  127,  279,  279,  279,  131,  132,
 /*  1390 */   133,  279,  279,  136,  279,  138,  139,  279,  279,  142,
 /*  1400 */   143,  144,  145,  127,  279,  279,  136,  131,  132,  133,
 /*  1410 */   279,  279,  136,  279,  138,  139,  146,  279,  142,  143,
 /*  1420 */   144,  145,  127,  279,  279,  279,  131,  132,  133,  279,
 /*  1430 */   279,  136,  279,  138,  139,  279,  271,  142,  143,  144,
 /*  1440 */   145,  127,  279,  279,  279,  131,  132,  133,  279,  279,
 /*  1450 */   136,  279,  138,  139,  279,  279,  142,  143,  144,  145,
 /*  1460 */   279,  279,  279,  279,  279,  127,  209,  279,  279,  131,
 /*  1470 */   132,  133,  279,  279,  136,  279,  138,  139,  208,  279,
 /*  1480 */   142,  143,  144,  145,  127,  209,  279,  279,  131,  132,
 /*  1490 */   133,  279,  279,  136,  279,  138,  139,  279,  279,  142,
 /*  1500 */   143,  144,  145,  279,  209,  279,  279,  279,  279,  127,
 /*  1510 */   279,  279,  279,  131,  132,  133,  279,  279,  136,  279,
 /*  1520 */   138,  139,  279,  209,  142,  143,  144,  145,  127,  279,
 /*  1530 */   279,  279,  131,  132,  133,  279,  279,  136,  279,  138,
 /*  1540 */   139,  271,  279,  142,  143,  144,  145,  209,  279,  279,
 /*  1550 */   279,  279,  279,  279,  279,  279,  127,  279,  279,  279,
 /*  1560 */   131,  132,  133,  279,  279,  136,  209,  138,  139,  279,
 /*  1570 */   279,  142,  143,  144,  145,  279,  279,  279,  127,  279,
 /*  1580 */   279,  279,  131,  132,  133,  279,  279,  136,  279,  138,
 /*  1590 */   139,  209,  279,  142,  143,  144,  145,  127,  279,  279,
 /*  1600 */   279,  131,  132,  133,  279,  279,  136,  279,  138,  139,
 /*  1610 */   209,  279,  142,  143,  144,  145,  127,  279,  279,  279,
 /*  1620 */   131,  132,  133,  279,  279,  136,  279,  138,  139,  279,
 /*  1630 */   279,  142,  143,  144,  145,  279,  279,  127,  209,  279,
 /*  1640 */   279,  131,  132,  133,  279,  279,  136,  279,  138,  139,
 /*  1650 */   279,  279,  142,  143,  144,  145,  279,  279,  279,  279,
 /*  1660 */   209,  279,  279,  279,  279,  127,  279,  279,  279,  131,
 /*  1670 */   132,  133,  279,  279,  136,  279,  138,  139,  279,  209,
 /*  1680 */   142,  143,  144,  145,  127,  279,  279,  279,  131,  132,
 /*  1690 */   133,  279,  279,  136,  279,  138,  139,  279,  209,  142,
 /*  1700 */   143,  144,  145,  279,  279,  279,  127,  279,  279,  279,
 /*  1710 */   131,  132,  133,  279,  279,  136,  279,  138,  139,  209,
 /*  1720 */   279,  142,  143,  144,  145,  127,  279,  279,  279,  131,
 /*  1730 */   132,  133,  279,  279,  136,  279,  138,  139,  279,  279,
 /*  1740 */   142,  143,  144,  145,  127,  279,  279,  209,  131,  132,
 /*  1750 */   133,  279,  279,  136,  279,  138,  139,  279,  279,  142,
 /*  1760 */   143,  144,  145,  279,  279,  127,  209,  279,  279,  131,
 /*  1770 */   132,  133,  279,  279,  136,  279,  138,  139,  279,  279,
 /*  1780 */   142,  143,  144,  145,  279,  279,  279,  279,  209,  279,
 /*  1790 */   279,  279,  279,  127,  279,  279,  279,  131,  132,  133,
 /*  1800 */   279,  279,  136,  279,  138,  139,  279,  209,  142,  143,
 /*  1810 */   144,  145,  127,  279,  279,  279,  131,  132,  133,  279,
 /*  1820 */   279,  136,  279,  138,  139,  279,  209,  142,  143,  144,
 /*  1830 */   145,  279,  279,  279,  127,  279,  279,  279,  131,  132,
 /*  1840 */   133,  279,  279,  136,  279,  138,  139,  209,  279,  142,
 /*  1850 */   143,  144,  145,  127,  279,  279,  279,  131,  132,  133,
 /*  1860 */   279,  279,  136,  279,  138,  139,  279,  279,  142,  143,
 /*  1870 */   144,  145,  127,  279,  279,  209,  131,  132,  133,  279,
 /*  1880 */   279,  136,  279,  138,  139,  279,  279,  142,  143,  144,
 /*  1890 */   145,  279,  279,  127,  209,  279,  279,  131,  132,  133,
 /*  1900 */   279,  279,  136,  279,  138,  139,  279,  279,  142,  143,
 /*  1910 */   144,  145,  279,  279,  279,  279,  209,  279,  279,  279,
 /*  1920 */   279,  127,  279,  279,  279,  131,  132,  133,  279,  279,
 /*  1930 */   136,  279,  138,  139,  279,  209,  142,  143,  144,  145,
 /*  1940 */   127,  279,  279,  279,  131,  132,  133,  279,  279,  136,
 /*  1950 */   279,  138,  139,  279,  209,  142,  143,  144,  145,  279,
 /*  1960 */   279,  279,  127,  279,  279,  279,  131,  132,  133,  279,
 /*  1970 */   279,  136,  279,  138,  139,  209,  279,  142,  143,  144,
 /*  1980 */   145,  127,  279,  279,  279,  131,  132,  133,  279,  279,
 /*  1990 */   136,  279,  138,  139,  279,  279,  142,  143,  144,  145,
 /*  2000 */   127,  279,  279,  209,  131,  132,  133,  279,  279,  136,
 /*  2010 */   279,  138,  139,  279,  279,  142,  143,  144,  145,  279,
 /*  2020 */   279,  127,  209,  279,  279,  131,  132,  133,  279,  279,
 /*  2030 */   136,  279,  138,  139,  279,  279,  142,  143,  144,  145,
 /*  2040 */   279,  279,  279,  279,  209,  279,  279,  279,  279,  127,
 /*  2050 */   279,  279,  279,  131,  132,  133,  279,  279,  136,  279,
 /*  2060 */   138,  139,  279,  209,  142,  143,  144,  145,  127,  279,
 /*  2070 */   279,  279,  131,  132,  133,  279,  279,  136,  279,  138,
 /*  2080 */   139,  279,  209,  142,  143,  144,  145,  279,  279,  279,
 /*  2090 */   127,  279,  279,  279,  131,  132,  133,  279,  279,  136,
 /*  2100 */   279,  138,  139,  209,  279,  142,  143,  144,  145,  127,
 /*  2110 */   279,  279,  279,  131,  132,  133,  279,  279,  136,  279,
 /*  2120 */   138,  139,  279,  279,  142,  143,  144,  145,  127,  279,
 /*  2130 */   279,  209,  131,  132,  133,  279,  279,  136,  279,  138,
 /*  2140 */   139,  279,  279,  142,  143,  144,  145,  279,  279,  127,
 /*  2150 */   209,  279,  279,  131,  132,  133,  279,  279,  136,  279,
 /*  2160 */   138,  139,  279,  279,  142,  143,  144,  145,  279,  279,
 /*  2170 */   279,  279,  209,  279,  279,  279,  279,  127,   34,   35,
 /*  2180 */   279,  131,  132,  133,  279,  279,  136,  279,  138,  139,
 /*  2190 */   279,  209,  142,  143,  144,  145,   52,  279,   54,  279,
 /*  2200 */   279,  279,  279,  279,  279,  279,  279,  279,   64,   65,
 /*  2210 */   209,  127,  279,  279,  279,  131,  279,  133,   74,   75,
 /*  2220 */   136,  279,  138,  139,  279,  279,  142,  143,  144,  145,
 /*  2230 */   279,  209,  279,  279,  279,  279,  279,  279,  279,  279,
 /*  2240 */   279,  279,  279,   99,  279,  279,  127,  103,  279,  105,
 /*  2250 */   131,  279,  133,  279,  279,  136,  112,  138,  139,  209,
 /*  2260 */   279,  142,  143,  144,  145,  127,  279,  279,  279,  131,
 /*  2270 */   279,  133,  279,  279,  136,  279,  138,  139,  279,  279,
 /*  2280 */   142,  143,  144,  145,  279,  279,  127,  279,  279,  279,
 /*  2290 */   131,  279,  133,  209,  279,  136,  279,  138,  139,  279,
 /*  2300 */   279,  142,  143,  144,  145,  127,  279,  279,  279,  131,
 /*  2310 */   279,  133,  279,  279,  136,  279,  138,  139,  279,  279,
 /*  2320 */   142,  143,  144,  145,  279,  279,  127,  279,  209,  279,
 /*  2330 */   131,  279,  133,  279,  279,  136,  279,  138,  139,  279,
 /*  2340 */   279,  142,  143,  144,  145,  127,  279,  209,  279,  131,
 /*  2350 */   279,  133,  279,  279,  136,  279,  138,  139,  279,  279,
 /*  2360 */   142,  143,  144,  145,  279,  279,  279,  279,  209,  279,
 /*  2370 */   279,  279,  279,  279,  127,  279,  279,  279,  131,  279,
 /*  2380 */   133,  279,  279,  136,  279,  138,  139,  209,  279,  142,
 /*  2390 */   143,  144,  145,  279,  279,  211,  212,  279,  279,  215,
 /*  2400 */   279,  279,  218,  219,  220,  127,  222,  223,  209,  131,
 /*  2410 */   279,  133,  279,  279,  136,  279,  138,  139,  279,  279,
 /*  2420 */   142,  143,  144,  145,  279,  127,  279,  209,  279,  131,
 /*  2430 */   279,  133,  279,  279,  136,  279,  138,  139,  279,  279,
 /*  2440 */   142,  143,  144,  145,  279,  279,  279,  279,  127,  279,
 /*  2450 */   279,  279,  131,  279,  133,  279,  209,  136,  279,  138,
 /*  2460 */   139,  279,  279,  142,  143,  144,  145,  279,  279,  279,
 /*  2470 */   279,  279,  279,  127,  279,  279,  279,  131,  279,  133,
 /*  2480 */   279,  279,  136,  279,  138,  139,  279,  209,  142,  143,
 /*  2490 */   144,  145,  211,  212,  279,  279,  215,  279,  279,  218,
 /*  2500 */   219,  220,  127,  222,  223,  279,  131,  209,  133,  279,
 /*  2510 */   279,  136,  279,  138,  139,  279,  279,  142,  143,  144,
 /*  2520 */   145,  279,  279,  279,  279,  279,  279,  279,  279,  279,
 /*  2530 */   209,  279,  279,  127,  279,  279,  279,  131,  279,  133,
 /*  2540 */   279,  279,  136,  279,  138,  139,  279,  279,  142,  143,
 /*  2550 */   144,  145,  279,  127,  279,  209,  279,  131,  279,  133,
 /*  2560 */   279,  279,  136,  279,  138,  139,  279,  279,  142,  143,
 /*  2570 */   127,  145,  279,  279,  131,  279,  133,  279,  279,  136,
 /*  2580 */   279,  138,  139,  279,  209,  142,  143,  279,  145,  279,
 /*  2590 */   211,  212,  279,  279,  215,  279,  279,  218,  219,  220,
 /*  2600 */   279,  222,  223,  279,  279,  279,  279,  279,  279,  279,
 /*  2610 */   279,  279,  279,  279,  279,  209,  211,  212,  279,  279,
 /*  2620 */   215,  279,  279,  218,  219,  220,  279,  222,  223,  211,
 /*  2630 */   212,  279,  279,  215,  279,  209,  218,  219,  220,  279,
 /*  2640 */   222,  223,  279,  279,  279,  279,  211,  212,  279,  279,
 /*  2650 */   215,  279,  209,  218,  219,  220,  279,  222,  223,
};
#define YY_SHIFT_USE_DFLT (-70)
#define YY_SHIFT_COUNT (402)
#define YY_SHIFT_MIN   (-69)
#define YY_SHIFT_MAX   (2144)
static const short yy_shift_ofst[] = {
 /*     0 */   606, 1265, 1265, 1265, 1265, 1265, 1265, 1265, 1265, 1265,
 /*    10 */    89,  155,  907,  907,  907,  155,   39,  189, 2144, 2144,
 /*    20 */   907,  -11,  189,  139,  139,  139,  139,  139,  139,  139,
 /*    30 */   139,  139,  139,  139,  139,  139,  139,  139,  139,  139,
 /*    40 */   139,  139,  139,  139,  139,  139,  139,  139,  139,  139,
 /*    50 */   139,  139,  139,  139,  139,  139,  139,  139,  139,  139,
 /*    60 */   139,  139,  139,  139,  139,  139,  139,  139,  139,  139,
 /*    70 */   139,  139,  139,  139,  139,  139,  500,  139,  139,  139,
 /*    80 */  1310, 1310, 1310, 1310, 1310, 1310, 1310, 1310, 1310, 1310,
 /*    90 */   246,  294,  294,  294,  294,  294,  294,  294,  294,  294,
 /*   100 */   294,  294,  294,  294,   37,  600,   37,   37,   37,   37,
 /*   110 */   461,  600,   37,   37,  362,  362,  362,  118,  235,   10,
 /*   120 */    10,   10, 1052, 1052, 1191,  123,   15,  603,  629,  599,
 /*   130 */    10,   10,   10, 1124, 1111, 1020,  901, 1255, 1191, 1083,
 /*   140 */   901, 1020,  -70,  -70,  -70,  280,  280, 1090, 1115, 1090,
 /*   150 */  1090, 1090,  249,  865,   -6,   96,   96,   96,   96,  317,
 /*   160 */   -60,  560,  558,  542,  -60,  506,  320,   10,  474,  425,
 /*   170 */   253,  253,  379,  185,   92,  -60,  747,  747,  747, 1122,
 /*   180 */   747,  747, 1124, 1122,  747,  747, 1111,  747, 1020,  747,
 /*   190 */   747, 1052, 1088,  747,  747, 1083, 1067,  747,  747,  747,
 /*   200 */   747,  821,  821, 1052, 1048,  747,  747,  747,  747,  892,
 /*   210 */   747,  747,  747,  747,  892,  913,  747,  747,  747,  747,
 /*   220 */   747,  747,  747,  901,  888,  747,  747,  895,  747,  901,
 /*   230 */   901,  901,  901,  854,  847,  747,  890,  821,  821,  815,
 /*   240 */   851,  815,  851,  851,  862,  851,  849,  747,  747,  -70,
 /*   250 */   -70,  -70,  -70,  -70,  -70, 1053, 1033, 1013,  979,  949,
 /*   260 */   918,  889,  860,  840,  719,  668,  809, 1263, 1263, 1263,
 /*   270 */  1263, 1263, 1263, 1263, 1263, 1263, 1263, 1263, 1263,  423,
 /*   280 */   460,   -8,  368,  368,  174,   78,   28,   13,   13,   13,
 /*   290 */    13,   13,   13,   13,   13,   13,   13,  554,  497,  455,
 /*   300 */   442,  449,  217,  416,  291,  109,  323,  178,  382,  178,
 /*   310 */   315,  161,  228,  -26,  278,  803,  724,  802,  756,  797,
 /*   320 */   752,  796,  737,  793,  773,  766,  695,  739,  798,  771,
 /*   330 */   764,  680,  698,  694,  754,  760,  753,  751,  750,  749,
 /*   340 */   748,  741,  746,  743,  713,  732,  740,  657,  733,  716,
 /*   350 */   731,  656,  649,  715,  675,  696,  704,  714,  706,  705,
 /*   360 */   710,  702,  707,  703,  683,  604,  618,  679,  647,  628,
 /*   370 */   608,  650,  635,  613,  611,  593,  601,  589,  584,  484,
 /*   380 */   399,  489,  475,  419,  373,  287,  336,  285,  276,  130,
 /*   390 */   130,  130,  130,  130,  130,  191,  134,   86,   86,   57,
 /*   400 */    34,    9,  -69,
};
#define YY_REDUCE_USE_DFLT (-179)
#define YY_REDUCE_COUNT (254)
#define YY_REDUCE_MIN   (-178)
#define YY_REDUCE_MAX   (2443)
static const short yy_reduce_ofst[] = {
 /*     0 */  -160,  494,  451,  401,  357,  314,  264,  220,  177,  120,
 /*    10 */  -102,  257,  557,  530,  510,  257, 1117, 1043, 1036, 1015,
 /*    20 */   920, 1171, 1152,  864, 2050, 2022, 2001, 1982, 1963, 1941,
 /*    30 */  1922, 1894, 1873, 1854, 1835, 1813, 1794, 1766, 1745, 1726,
 /*    40 */  1707, 1685, 1666, 1638, 1617, 1598, 1579, 1557, 1538, 1510,
 /*    50 */  1489, 1470, 1451, 1429, 1401, 1382, 1357, 1338, 1314, 1295,
 /*    60 */  1276, 1257, 2406, 2375, 2346, 2321, 2298, 2278, 2247, 2218,
 /*    70 */  2199, 2178, 2159, 2138, 2119, 2084,  179, 2443, 2426,  999,
 /*    80 */  2435, 2418, 2405, 2379, 2281, 2184, 1123,  417,  358,  140,
 /*    90 */  -178, 1009,  982,  863,  758,  726,  692,  612,  488,  400,
 /*   100 */    -7,  -31,  -57,  -80,  387,   58, 1270, 1165, 1037,  225,
 /*   110 */   -21,  -76,  319,  312,   67,   31,  172,  -32,   -1,  114,
 /*   120 */    56,  -30,  -46,  -96,  -97,  133,  524,  480,   93,  299,
 /*   130 */   204,  297,  402,  361,  258,  413,  445,  331, -139,    2,
 /*   140 */   421,  390,  388, -101,  219, 1100, 1093, 1049, 1127, 1044,
 /*   150 */  1041, 1026,  981, 1016, 1060, 1057, 1057, 1057, 1057,  992,
 /*   160 */  1021, 1001, 1000,  980, 1017,  975,  967, 1071,  966,  965,
 /*   170 */   954,  947,  963,  962,  951,  989,  943,  925,  924,  912,
 /*   180 */   923,  922,  900,  891,  911,  910,  916,  903,  958,  899,
 /*   190 */   894,  957,  941,  875,  874,  940,  830,  870,  869,  868,
 /*   200 */   850,  898,  896,  915,  829,  842,  834,  831,  828,  917,
 /*   210 */   816,  814,  813,  811,  902,  882,  808,  807,  794,  792,
 /*   220 */   791,  788,  776,  799,  790,  767,  757,  770,  729,  765,
 /*   230 */   762,  755,  734,  761,  763,  699,  781,  742,  738,  723,
 /*   240 */   774,  712,  769,  759,  727,  744,  645,  623,  620,  617,
 /*   250 */   718,  700,  581,  569,  614,
};
static const YYACTIONTYPE yy_default[] = {
 /*     0 */   977,  913,  913,  913,  913,  913,  913,  913,  913,  913,
 /*    10 */   700,  894,  649,  649,  649,  893,  977,  977,  977,  977,
 /*    20 */   649,  977,  972,  977,  977,  977,  977,  977,  977,  977,
 /*    30 */   977,  977,  977,  977,  977,  977,  977,  977,  977,  977,
 /*    40 */   977,  977,  977,  977,  977,  977,  977,  977,  977,  977,
 /*    50 */   977,  977,  977,  977,  977,  977,  977,  977,  977,  977,
 /*    60 */   977,  977,  977,  977,  977,  977,  977,  977,  977,  977,
 /*    70 */   977,  977,  977,  977,  977,  977,  977,  977,  977,  977,
 /*    80 */   977,  977,  977,  977,  977,  977,  977,  977,  977,  977,
 /*    90 */   686,  977,  977,  977,  977,  977,  977,  977,  977,  977,
 /*   100 */   977,  977,  977,  977,  977,  977,  977,  977,  977,  977,
 /*   110 */   714,  965,  977,  977,  707,  707,  977,  916,  977,  977,
 /*   120 */   977,  977,  839,  839,  760,  943,  977,  977,  975,  977,
 /*   130 */   825,  977,  715,  977,  977,  973,  977,  977,  760,  763,
 /*   140 */   977,  973,  695,  675,  813,  977,  977,  977,  691,  977,
 /*   150 */   977,  977,  977,  734,  977,  954,  953,  952,  749,  977,
 /*   160 */   849,  977,  977,  977,  849,  977,  977,  977,  977,  977,
 /*   170 */   977,  977,  977,  977,  977,  849,  977,  977,  977,  977,
 /*   180 */   810,  977,  977,  977,  807,  977,  977,  977,  977,  977,
 /*   190 */   977,  977,  977,  977,  977,  763,  977,  977,  977,  977,
 /*   200 */   977,  955,  955,  977,  977,  977,  977,  977,  977,  966,
 /*   210 */   977,  977,  977,  977,  966,  975,  977,  977,  977,  977,
 /*   220 */   977,  977,  977,  977,  917,  977,  977,  728,  977,  977,
 /*   230 */   977,  977,  977,  964,  824,  977,  977,  955,  955,  854,
 /*   240 */   856,  854,  856,  856,  977,  856,  977,  977,  977,  686,
 /*   250 */   892,  863,  843,  842,  667,  977,  977,  977,  977,  977,
 /*   260 */   977,  977,  977,  977,  977,  977,  977,  836,  698,  699,
 /*   270 */   976,  967,  692,  799,  657,  659,  758,  759,  655,  977,
 /*   280 */   977,  748,  757,  756,  977,  977,  977,  747,  746,  745,
 /*   290 */   744,  743,  742,  741,  740,  739,  738,  977,  977,  977,
 /*   300 */   977,  977,  977,  977,  977,  794,  977,  928,  977,  927,
 /*   310 */   977,  977,  794,  977,  977,  977,  977,  977,  977,  977,
 /*   320 */   800,  977,  977,  977,  977,  977,  977,  977,  977,  977,
 /*   330 */   977,  977,  977,  977,  977,  977,  977,  977,  977,  977,
 /*   340 */   977,  977,  977,  788,  977,  977,  977,  868,  977,  977,
 /*   350 */   977,  977,  977,  977,  977,  977,  977,  977,  977,  977,
 /*   360 */   977,  977,  977,  977,  921,  977,  977,  977,  977,  977,
 /*   370 */   977,  977,  977,  977,  724,  977,  977,  977,  977,  851,
 /*   380 */   850,  977,  977,  656,  658,  977,  977,  977,  794,  755,
 /*   390 */   754,  753,  752,  751,  750,  977,  977,  737,  736,  977,
 /*   400 */   977,  977,  977,  732,  897,  896,  895,  814,  812,  811,
 /*   410 */   809,  808,  806,  804,  803,  802,  801,  805,  891,  890,
 /*   420 */   889,  888,  887,  886,  772,  941,  939,  938,  937,  936,
 /*   430 */   935,  934,  933,  932,  898,  847,  722,  940,  862,  861,
 /*   440 */   860,  841,  840,  838,  837,  774,  775,  776,  773,  765,
 /*   450 */   766,  764,  790,  791,  762,  661,  781,  783,  785,  787,
 /*   460 */   789,  786,  784,  782,  780,  779,  770,  769,  768,  767,
 /*   470 */   660,  761,  709,  708,  706,  650,  648,  647,  646,  942,
 /*   480 */   702,  701,  697,  696,  882,  915,  914,  912,  911,  910,
 /*   490 */   909,  908,  907,  906,  905,  904,  903,  902,  884,  883,
 /*   500 */   881,  798,  865,  859,  858,  796,  795,  723,  703,  694,
 /*   510 */   670,  671,  669,  666,  645,  721,  922,  930,  931,  926,
 /*   520 */   924,  929,  925,  923,  848,  794,  918,  920,  846,  845,
 /*   530 */   919,  720,  730,  729,  727,  726,  823,  820,  819,  818,
 /*   540 */   817,  816,  822,  821,  963,  962,  960,  961,  959,  958,
 /*   550 */   957,  974,  971,  970,  969,  968,  719,  717,  725,  724,
 /*   560 */   718,  716,  853,  852,  678,  828,  956,  901,  900,  844,
 /*   570 */   827,  826,  685,  855,  684,  683,  682,  681,  857,  735,
 /*   580 */   870,  869,  771,  652,  880,  879,  878,  877,  876,  875,
 /*   590 */   874,  873,  945,  951,  950,  949,  948,  947,  946,  944,
 /*   600 */   872,  871,  835,  834,  833,  832,  831,  830,  829,  885,
 /*   610 */   815,  793,  792,  778,  651,  868,  867,  693,  705,  704,
 /*   620 */   654,  653,  680,  679,  677,  674,  673,  672,  668,  665,
 /*   630 */   664,  663,  662,  676,  713,  712,  711,  710,  690,  689,
 /*   640 */   688,  687,  899,  797,  733,
};

/* The next table maps tokens into fallback tokens.  If a construct
** like the following:
** 
**      %fallback ID X Y Z.
**
** appears in the grammar, then ID becomes a fallback token for X, Y,
** and Z.  Whenever one of the tokens X, Y, or Z is input to the parser
** but it does not parse, the type of the token is changed to ID and
** the parse is retried before an error is thrown.
*/
#ifdef YYFALLBACK
static const YYCODETYPE yyFallback[] = {
};
#endif /* YYFALLBACK */

/* The following structure represents a single element of the
** parser's stack.  Information stored includes:
**
**   +  The state number for the parser at this level of the stack.
**
**   +  The value of the token stored at this level of the stack.
**      (In other words, the "major" token.)
**
**   +  The semantic value stored at this level of the stack.  This is
**      the information used by the action routines in the grammar.
**      It is sometimes called the "minor" token.
*/
struct yyStackEntry {
  YYACTIONTYPE stateno;  /* The state-number */
  YYCODETYPE major;      /* The major token value.  This is the code
                         ** number for the token at this stack level */
  YYMINORTYPE minor;     /* The user-supplied minor token value.  This
                         ** is the value of the token  */
};
typedef struct yyStackEntry yyStackEntry;

/* The state of the parser is completely contained in an instance of
** the following structure */
struct yyParser {
  int yyidx;                    /* Index of top element in stack */
#ifdef YYTRACKMAXSTACKDEPTH
  int yyidxMax;                 /* Maximum value of yyidx */
#endif
  int yyerrcnt;                 /* Shifts left before out of the error */
  ParseARG_SDECL                /* A place to hold %extra_argument */
#if YYSTACKDEPTH<=0
  int yystksz;                  /* Current side of the stack */
  yyStackEntry *yystack;        /* The parser's stack */
#else
  yyStackEntry yystack[YYSTACKDEPTH];  /* The parser's stack */
#endif
};
typedef struct yyParser yyParser;

#ifndef NDEBUG
#include <stdio.h>
static FILE *yyTraceFILE = 0;
static char *yyTracePrompt = 0;
#endif /* NDEBUG */

#ifndef NDEBUG
/* 
** Turn parser tracing on by giving a stream to which to write the trace
** and a prompt to preface each trace message.  Tracing is turned off
** by making either argument NULL 
**
** Inputs:
** <ul>
** <li> A FILE* to which trace output should be written.
**      If NULL, then tracing is turned off.
** <li> A prefix string written at the beginning of every
**      line of trace output.  If NULL, then tracing is
**      turned off.
** </ul>
**
** Outputs:
** None.
*/
void ParseTrace(FILE *TraceFILE, char *zTracePrompt){
  yyTraceFILE = TraceFILE;
  yyTracePrompt = zTracePrompt;
  if( yyTraceFILE==0 ) yyTracePrompt = 0;
  else if( yyTracePrompt==0 ) yyTraceFILE = 0;
}
#endif /* NDEBUG */

#ifndef NDEBUG
/* For tracing shifts, the names of all terminals and nonterminals
** are required.  The following table supplies these names */
static const char *const yyTokenName[] = { 
  "$",             "TOK_EQUAL",     "TOK_GREATER_EQUAL",  "TOK_GREATER_THAN",
  "TOK_IN",        "TOK_INST_EQUAL",  "TOK_INST_NOT_EQUAL",  "TOK_LESS_EQUAL",
  "TOK_LESS_THAN",  "TOK_LIKE",      "TOK_NOT_EQUAL",  "TOK_MINUS",   
  "TOK_PLUS",      "TOK_OR",        "TOK_XOR",       "TOK_DIV",     
  "TOK_MOD",       "TOK_REAL_DIV",  "TOK_TIMES",     "TOK_AND",     
  "TOK_ANDOR",     "TOK_CONCAT_OP",  "TOK_EXP",       "TOK_NOT",     
  "TOK_DOT",       "TOK_BACKSLASH",  "TOK_LEFT_BRACKET",  "TOK_LEFT_PAREN",
  "TOK_RIGHT_PAREN",  "TOK_RIGHT_BRACKET",  "TOK_COLON",     "TOK_COMMA",   
  "TOK_AGGREGATE",  "TOK_OF",        "TOK_IDENTIFIER",  "TOK_ALIAS",   
  "TOK_FOR",       "TOK_END_ALIAS",  "TOK_ARRAY",     "TOK_ASSIGNMENT",
  "TOK_BAG",       "TOK_BOOLEAN",   "TOK_INTEGER",   "TOK_REAL",    
  "TOK_NUMBER",    "TOK_LOGICAL",   "TOK_BINARY",    "TOK_STRING",  
  "TOK_BY",        "TOK_LEFT_CURL",  "TOK_RIGHT_CURL",  "TOK_OTHERWISE",
  "TOK_CASE",      "TOK_END_CASE",  "TOK_BEGIN",     "TOK_END",     
  "TOK_PI",        "TOK_E",         "TOK_CONSTANT",  "TOK_END_CONSTANT",
  "TOK_DERIVE",    "TOK_END_ENTITY",  "TOK_ENTITY",    "TOK_ENUMERATION",
  "TOK_ESCAPE",    "TOK_SELF",      "TOK_OPTIONAL",  "TOK_VAR",     
  "TOK_END_FUNCTION",  "TOK_FUNCTION",  "TOK_BUILTIN_FUNCTION",  "TOK_LIST",    
  "TOK_SET",       "TOK_GENERIC",   "TOK_QUESTION_MARK",  "TOK_IF",      
  "TOK_THEN",      "TOK_END_IF",    "TOK_ELSE",      "TOK_INCLUDE", 
  "TOK_STRING_LITERAL",  "TOK_TO",        "TOK_AS",        "TOK_REFERENCE",
  "TOK_FROM",      "TOK_USE",       "TOK_INVERSE",   "TOK_INTEGER_LITERAL",
  "TOK_REAL_LITERAL",  "TOK_STRING_LITERAL_ENCODED",  "TOK_LOGICAL_LITERAL",  "TOK_BINARY_LITERAL",
  "TOK_LOCAL",     "TOK_END_LOCAL",  "TOK_ONEOF",     "TOK_UNIQUE",  
  "TOK_FIXED",     "TOK_END_PROCEDURE",  "TOK_PROCEDURE",  "TOK_BUILTIN_PROCEDURE",
  "TOK_QUERY",     "TOK_ALL_IN",    "TOK_SUCH_THAT",  "TOK_REPEAT",  
  "TOK_END_REPEAT",  "TOK_RETURN",    "TOK_END_RULE",  "TOK_RULE",    
  "TOK_END_SCHEMA",  "TOK_SCHEMA",    "TOK_SELECT",    "TOK_SEMICOLON",
  "TOK_SKIP",      "TOK_SUBTYPE",   "TOK_ABSTRACT",  "TOK_SUPERTYPE",
  "TOK_END_TYPE",  "TOK_TYPE",      "TOK_UNTIL",     "TOK_WHERE",   
  "TOK_WHILE",     "error",         "statement_list",  "case_action", 
  "case_otherwise",  "entity_body",   "aggregate_init_element",  "aggregate_initializer",
  "assignable",    "attribute_decl",  "by_expression",  "constant",    
  "expression",    "function_call",  "general_ref",   "group_ref",   
  "identifier",    "initializer",   "interval",      "literal",     
  "local_initializer",  "precision_spec",  "query_expression",  "query_start", 
  "simple_expression",  "unary_expression",  "supertype_expression",  "until_control",
  "while_control",  "function_header",  "fh_lineno",     "rule_header", 
  "rh_start",      "rh_get_line",   "procedure_header",  "ph_get_line", 
  "action_body",   "actual_parameters",  "aggregate_init_body",  "explicit_attr_list",
  "case_action_list",  "case_block",    "case_labels",   "where_clause_list",
  "derive_decl",   "explicit_attribute",  "expression_list",  "formal_parameter",
  "formal_parameter_list",  "formal_parameter_rep",  "id_list",       "defined_type_list",
  "nested_id_list",  "statement_rep",  "subtype_decl",  "where_rule",  
  "where_rule_OPT",  "supertype_expression_list",  "labelled_attrib_list_list",  "labelled_attrib_list",
  "inverse_attr_list",  "inverse_clause",  "attribute_decl_list",  "derived_attribute_rep",
  "unique_clause",  "rule_formal_parameter_list",  "qualified_attr_list",  "rel_op",      
  "optional_or_unique",  "optional_fixed",  "optional",      "var",         
  "unique",        "qualified_attr",  "qualifier",     "alias_statement",
  "assignment_statement",  "case_statement",  "compound_statement",  "escape_statement",
  "if_statement",  "proc_call_statement",  "repeat_statement",  "return_statement",
  "skip_statement",  "statement",     "subsuper_decl",  "supertype_decl",
  "supertype_factor",  "function_id",   "procedure_id",  "attribute_type",
  "defined_type",  "parameter_type",  "generic_type",  "basic_type",  
  "select_type",   "aggregate_type",  "aggregation_type",  "array_type",  
  "bag_type",      "conformant_aggregation",  "list_type",     "set_type",    
  "set_or_bag_of_entity",  "type",          "cardinality_op",  "bound_spec",  
  "inverse_attr",  "derived_attribute",  "rule_formal_parameter",  "where_clause",
  "action_body_item_rep",  "action_body_item",  "declaration",   "constant_decl",
  "local_decl",    "semicolon",     "alias_push_scope",  "block_list",  
  "block_member",  "include_directive",  "rule_decl",     "constant_body",
  "constant_body_list",  "entity_decl",   "function_decl",  "procedure_decl",
  "type_decl",     "entity_header",  "enumeration_type",  "express_file",
  "schema_decl_list",  "schema_decl",   "fh_push_scope",  "fh_plist",    
  "increment_control",  "rename",        "rename_list",   "parened_rename_list",
  "reference_clause",  "reference_head",  "use_clause",    "use_head",    
  "interface_specification",  "interface_specification_list",  "right_curl",    "local_variable",
  "local_body",    "local_decl_rules_on",  "local_decl_rules_off",  "oneof_op",    
  "ph_push_scope",  "schema_body",   "schema_header",  "type_item_body",
  "type_item",     "ti_start",      "td_start",    
};
#endif /* NDEBUG */

#ifndef NDEBUG
/* For tracing reduce actions, the names of all rules are required.
*/
static const char *const yyRuleName[] = {
 /*   0 */ "action_body ::= action_body_item_rep statement_rep",
 /*   1 */ "action_body_item ::= declaration",
 /*   2 */ "action_body_item ::= constant_decl",
 /*   3 */ "action_body_item ::= local_decl",
 /*   4 */ "action_body_item_rep ::=",
 /*   5 */ "action_body_item_rep ::= action_body_item action_body_item_rep",
 /*   6 */ "actual_parameters ::= TOK_LEFT_PAREN expression_list TOK_RIGHT_PAREN",
 /*   7 */ "actual_parameters ::= TOK_LEFT_PAREN TOK_RIGHT_PAREN",
 /*   8 */ "aggregate_initializer ::= TOK_LEFT_BRACKET TOK_RIGHT_BRACKET",
 /*   9 */ "aggregate_initializer ::= TOK_LEFT_BRACKET aggregate_init_body TOK_RIGHT_BRACKET",
 /*  10 */ "aggregate_init_element ::= expression",
 /*  11 */ "aggregate_init_body ::= aggregate_init_element",
 /*  12 */ "aggregate_init_body ::= aggregate_init_element TOK_COLON expression",
 /*  13 */ "aggregate_init_body ::= aggregate_init_body TOK_COMMA aggregate_init_element",
 /*  14 */ "aggregate_init_body ::= aggregate_init_body TOK_COMMA aggregate_init_element TOK_COLON expression",
 /*  15 */ "aggregate_type ::= TOK_AGGREGATE TOK_OF parameter_type",
 /*  16 */ "aggregate_type ::= TOK_AGGREGATE TOK_COLON TOK_IDENTIFIER TOK_OF parameter_type",
 /*  17 */ "aggregation_type ::= array_type",
 /*  18 */ "aggregation_type ::= bag_type",
 /*  19 */ "aggregation_type ::= list_type",
 /*  20 */ "aggregation_type ::= set_type",
 /*  21 */ "alias_statement ::= TOK_ALIAS TOK_IDENTIFIER TOK_FOR general_ref semicolon alias_push_scope statement_rep TOK_END_ALIAS semicolon",
 /*  22 */ "alias_push_scope ::=",
 /*  23 */ "array_type ::= TOK_ARRAY bound_spec TOK_OF optional_or_unique attribute_type",
 /*  24 */ "assignable ::= assignable qualifier",
 /*  25 */ "assignable ::= identifier",
 /*  26 */ "assignment_statement ::= assignable TOK_ASSIGNMENT expression semicolon",
 /*  27 */ "attribute_type ::= aggregation_type",
 /*  28 */ "attribute_type ::= basic_type",
 /*  29 */ "attribute_type ::= defined_type",
 /*  30 */ "explicit_attr_list ::=",
 /*  31 */ "explicit_attr_list ::= explicit_attr_list explicit_attribute",
 /*  32 */ "bag_type ::= TOK_BAG bound_spec TOK_OF attribute_type",
 /*  33 */ "bag_type ::= TOK_BAG TOK_OF attribute_type",
 /*  34 */ "basic_type ::= TOK_BOOLEAN",
 /*  35 */ "basic_type ::= TOK_INTEGER precision_spec",
 /*  36 */ "basic_type ::= TOK_REAL precision_spec",
 /*  37 */ "basic_type ::= TOK_NUMBER",
 /*  38 */ "basic_type ::= TOK_LOGICAL",
 /*  39 */ "basic_type ::= TOK_BINARY precision_spec optional_fixed",
 /*  40 */ "basic_type ::= TOK_STRING precision_spec optional_fixed",
 /*  41 */ "block_list ::=",
 /*  42 */ "block_list ::= block_list block_member",
 /*  43 */ "block_member ::= declaration",
 /*  44 */ "block_member ::= include_directive",
 /*  45 */ "block_member ::= rule_decl",
 /*  46 */ "by_expression ::=",
 /*  47 */ "by_expression ::= TOK_BY expression",
 /*  48 */ "cardinality_op ::= TOK_LEFT_CURL expression TOK_COLON expression TOK_RIGHT_CURL",
 /*  49 */ "case_action ::= case_labels TOK_COLON statement",
 /*  50 */ "case_action_list ::=",
 /*  51 */ "case_action_list ::= case_action_list case_action",
 /*  52 */ "case_block ::= case_action_list case_otherwise",
 /*  53 */ "case_labels ::= expression",
 /*  54 */ "case_labels ::= case_labels TOK_COMMA expression",
 /*  55 */ "case_otherwise ::=",
 /*  56 */ "case_otherwise ::= TOK_OTHERWISE TOK_COLON statement",
 /*  57 */ "case_statement ::= TOK_CASE expression TOK_OF case_block TOK_END_CASE semicolon",
 /*  58 */ "compound_statement ::= TOK_BEGIN statement_rep TOK_END semicolon",
 /*  59 */ "constant ::= TOK_PI",
 /*  60 */ "constant ::= TOK_E",
 /*  61 */ "constant_body ::= identifier TOK_COLON attribute_type TOK_ASSIGNMENT expression semicolon",
 /*  62 */ "constant_body_list ::=",
 /*  63 */ "constant_body_list ::= constant_body constant_body_list",
 /*  64 */ "constant_decl ::= TOK_CONSTANT constant_body_list TOK_END_CONSTANT semicolon",
 /*  65 */ "declaration ::= entity_decl",
 /*  66 */ "declaration ::= function_decl",
 /*  67 */ "declaration ::= procedure_decl",
 /*  68 */ "declaration ::= type_decl",
 /*  69 */ "derive_decl ::=",
 /*  70 */ "derive_decl ::= TOK_DERIVE derived_attribute_rep",
 /*  71 */ "derived_attribute ::= attribute_decl TOK_COLON attribute_type initializer semicolon",
 /*  72 */ "derived_attribute_rep ::= derived_attribute",
 /*  73 */ "derived_attribute_rep ::= derived_attribute_rep derived_attribute",
 /*  74 */ "entity_body ::= explicit_attr_list derive_decl inverse_clause unique_clause where_rule_OPT",
 /*  75 */ "entity_decl ::= entity_header subsuper_decl semicolon entity_body TOK_END_ENTITY semicolon",
 /*  76 */ "entity_header ::= TOK_ENTITY TOK_IDENTIFIER",
 /*  77 */ "enumeration_type ::= TOK_ENUMERATION TOK_OF nested_id_list",
 /*  78 */ "escape_statement ::= TOK_ESCAPE semicolon",
 /*  79 */ "attribute_decl ::= TOK_IDENTIFIER",
 /*  80 */ "attribute_decl ::= TOK_SELF TOK_BACKSLASH TOK_IDENTIFIER TOK_DOT TOK_IDENTIFIER",
 /*  81 */ "attribute_decl_list ::= attribute_decl",
 /*  82 */ "attribute_decl_list ::= attribute_decl_list TOK_COMMA attribute_decl",
 /*  83 */ "optional ::=",
 /*  84 */ "optional ::= TOK_OPTIONAL",
 /*  85 */ "explicit_attribute ::= attribute_decl_list TOK_COLON optional attribute_type semicolon",
 /*  86 */ "express_file ::= schema_decl_list",
 /*  87 */ "schema_decl_list ::= schema_decl",
 /*  88 */ "schema_decl_list ::= schema_decl_list schema_decl",
 /*  89 */ "expression ::= simple_expression",
 /*  90 */ "expression ::= expression TOK_AND expression",
 /*  91 */ "expression ::= expression TOK_OR expression",
 /*  92 */ "expression ::= expression TOK_XOR expression",
 /*  93 */ "expression ::= expression TOK_LESS_THAN expression",
 /*  94 */ "expression ::= expression TOK_GREATER_THAN expression",
 /*  95 */ "expression ::= expression TOK_EQUAL expression",
 /*  96 */ "expression ::= expression TOK_LESS_EQUAL expression",
 /*  97 */ "expression ::= expression TOK_GREATER_EQUAL expression",
 /*  98 */ "expression ::= expression TOK_NOT_EQUAL expression",
 /*  99 */ "expression ::= expression TOK_INST_EQUAL expression",
 /* 100 */ "expression ::= expression TOK_INST_NOT_EQUAL expression",
 /* 101 */ "expression ::= expression TOK_IN expression",
 /* 102 */ "expression ::= expression TOK_LIKE expression",
 /* 103 */ "expression ::= simple_expression cardinality_op simple_expression",
 /* 104 */ "simple_expression ::= unary_expression",
 /* 105 */ "simple_expression ::= simple_expression TOK_CONCAT_OP simple_expression",
 /* 106 */ "simple_expression ::= simple_expression TOK_EXP simple_expression",
 /* 107 */ "simple_expression ::= simple_expression TOK_TIMES simple_expression",
 /* 108 */ "simple_expression ::= simple_expression TOK_DIV simple_expression",
 /* 109 */ "simple_expression ::= simple_expression TOK_REAL_DIV simple_expression",
 /* 110 */ "simple_expression ::= simple_expression TOK_MOD simple_expression",
 /* 111 */ "simple_expression ::= simple_expression TOK_PLUS simple_expression",
 /* 112 */ "simple_expression ::= simple_expression TOK_MINUS simple_expression",
 /* 113 */ "expression_list ::= expression",
 /* 114 */ "expression_list ::= expression_list TOK_COMMA expression",
 /* 115 */ "var ::=",
 /* 116 */ "var ::= TOK_VAR",
 /* 117 */ "formal_parameter ::= var id_list TOK_COLON parameter_type",
 /* 118 */ "formal_parameter_list ::=",
 /* 119 */ "formal_parameter_list ::= TOK_LEFT_PAREN formal_parameter_rep TOK_RIGHT_PAREN",
 /* 120 */ "formal_parameter_rep ::= formal_parameter",
 /* 121 */ "formal_parameter_rep ::= formal_parameter_rep semicolon formal_parameter",
 /* 122 */ "parameter_type ::= basic_type",
 /* 123 */ "parameter_type ::= conformant_aggregation",
 /* 124 */ "parameter_type ::= defined_type",
 /* 125 */ "parameter_type ::= generic_type",
 /* 126 */ "function_call ::= function_id actual_parameters",
 /* 127 */ "function_decl ::= function_header action_body TOK_END_FUNCTION semicolon",
 /* 128 */ "function_header ::= fh_lineno fh_push_scope fh_plist TOK_COLON parameter_type semicolon",
 /* 129 */ "fh_lineno ::= TOK_FUNCTION",
 /* 130 */ "fh_push_scope ::= TOK_IDENTIFIER",
 /* 131 */ "fh_plist ::= formal_parameter_list",
 /* 132 */ "function_id ::= TOK_IDENTIFIER",
 /* 133 */ "function_id ::= TOK_BUILTIN_FUNCTION",
 /* 134 */ "conformant_aggregation ::= aggregate_type",
 /* 135 */ "conformant_aggregation ::= TOK_ARRAY TOK_OF optional_or_unique parameter_type",
 /* 136 */ "conformant_aggregation ::= TOK_ARRAY bound_spec TOK_OF optional_or_unique parameter_type",
 /* 137 */ "conformant_aggregation ::= TOK_BAG TOK_OF parameter_type",
 /* 138 */ "conformant_aggregation ::= TOK_BAG bound_spec TOK_OF parameter_type",
 /* 139 */ "conformant_aggregation ::= TOK_LIST TOK_OF unique parameter_type",
 /* 140 */ "conformant_aggregation ::= TOK_LIST bound_spec TOK_OF unique parameter_type",
 /* 141 */ "conformant_aggregation ::= TOK_SET TOK_OF parameter_type",
 /* 142 */ "conformant_aggregation ::= TOK_SET bound_spec TOK_OF parameter_type",
 /* 143 */ "generic_type ::= TOK_GENERIC",
 /* 144 */ "generic_type ::= TOK_GENERIC TOK_COLON TOK_IDENTIFIER",
 /* 145 */ "id_list ::= TOK_IDENTIFIER",
 /* 146 */ "id_list ::= id_list TOK_COMMA TOK_IDENTIFIER",
 /* 147 */ "identifier ::= TOK_SELF",
 /* 148 */ "identifier ::= TOK_QUESTION_MARK",
 /* 149 */ "identifier ::= TOK_IDENTIFIER",
 /* 150 */ "if_statement ::= TOK_IF expression TOK_THEN statement_rep TOK_END_IF semicolon",
 /* 151 */ "if_statement ::= TOK_IF expression TOK_THEN statement_rep TOK_ELSE statement_rep TOK_END_IF semicolon",
 /* 152 */ "include_directive ::= TOK_INCLUDE TOK_STRING_LITERAL semicolon",
 /* 153 */ "increment_control ::= TOK_IDENTIFIER TOK_ASSIGNMENT expression TOK_TO expression by_expression",
 /* 154 */ "initializer ::= TOK_ASSIGNMENT expression",
 /* 155 */ "rename ::= TOK_IDENTIFIER",
 /* 156 */ "rename ::= TOK_IDENTIFIER TOK_AS TOK_IDENTIFIER",
 /* 157 */ "rename_list ::= rename",
 /* 158 */ "rename_list ::= rename_list TOK_COMMA rename",
 /* 159 */ "parened_rename_list ::= TOK_LEFT_PAREN rename_list TOK_RIGHT_PAREN",
 /* 160 */ "reference_clause ::= TOK_REFERENCE TOK_FROM TOK_IDENTIFIER semicolon",
 /* 161 */ "reference_clause ::= reference_head parened_rename_list semicolon",
 /* 162 */ "reference_head ::= TOK_REFERENCE TOK_FROM TOK_IDENTIFIER",
 /* 163 */ "use_clause ::= TOK_USE TOK_FROM TOK_IDENTIFIER semicolon",
 /* 164 */ "use_clause ::= use_head parened_rename_list semicolon",
 /* 165 */ "use_head ::= TOK_USE TOK_FROM TOK_IDENTIFIER",
 /* 166 */ "interface_specification ::= use_clause",
 /* 167 */ "interface_specification ::= reference_clause",
 /* 168 */ "interface_specification_list ::=",
 /* 169 */ "interface_specification_list ::= interface_specification_list interface_specification",
 /* 170 */ "interval ::= TOK_LEFT_CURL simple_expression rel_op simple_expression rel_op simple_expression right_curl",
 /* 171 */ "set_or_bag_of_entity ::= defined_type",
 /* 172 */ "set_or_bag_of_entity ::= TOK_SET TOK_OF defined_type",
 /* 173 */ "set_or_bag_of_entity ::= TOK_SET bound_spec TOK_OF defined_type",
 /* 174 */ "set_or_bag_of_entity ::= TOK_BAG bound_spec TOK_OF defined_type",
 /* 175 */ "set_or_bag_of_entity ::= TOK_BAG TOK_OF defined_type",
 /* 176 */ "inverse_attr_list ::= inverse_attr",
 /* 177 */ "inverse_attr_list ::= inverse_attr_list inverse_attr",
 /* 178 */ "inverse_attr ::= attribute_decl TOK_COLON set_or_bag_of_entity TOK_FOR TOK_IDENTIFIER semicolon",
 /* 179 */ "inverse_clause ::=",
 /* 180 */ "inverse_clause ::= TOK_INVERSE inverse_attr_list",
 /* 181 */ "bound_spec ::= TOK_LEFT_BRACKET expression TOK_COLON expression TOK_RIGHT_BRACKET",
 /* 182 */ "list_type ::= TOK_LIST bound_spec TOK_OF unique attribute_type",
 /* 183 */ "list_type ::= TOK_LIST TOK_OF unique attribute_type",
 /* 184 */ "literal ::= TOK_INTEGER_LITERAL",
 /* 185 */ "literal ::= TOK_REAL_LITERAL",
 /* 186 */ "literal ::= TOK_STRING_LITERAL",
 /* 187 */ "literal ::= TOK_STRING_LITERAL_ENCODED",
 /* 188 */ "literal ::= TOK_LOGICAL_LITERAL",
 /* 189 */ "literal ::= TOK_BINARY_LITERAL",
 /* 190 */ "literal ::= constant",
 /* 191 */ "local_initializer ::= TOK_ASSIGNMENT expression",
 /* 192 */ "local_variable ::= id_list TOK_COLON parameter_type semicolon",
 /* 193 */ "local_variable ::= id_list TOK_COLON parameter_type local_initializer semicolon",
 /* 194 */ "local_body ::=",
 /* 195 */ "local_body ::= local_variable local_body",
 /* 196 */ "local_decl ::= TOK_LOCAL local_decl_rules_on local_body TOK_END_LOCAL semicolon local_decl_rules_off",
 /* 197 */ "local_decl_rules_on ::=",
 /* 198 */ "local_decl_rules_off ::=",
 /* 199 */ "defined_type ::= TOK_IDENTIFIER",
 /* 200 */ "defined_type_list ::= defined_type",
 /* 201 */ "defined_type_list ::= defined_type_list TOK_COMMA defined_type",
 /* 202 */ "nested_id_list ::= TOK_LEFT_PAREN id_list TOK_RIGHT_PAREN",
 /* 203 */ "oneof_op ::= TOK_ONEOF",
 /* 204 */ "optional_or_unique ::=",
 /* 205 */ "optional_or_unique ::= TOK_OPTIONAL",
 /* 206 */ "optional_or_unique ::= TOK_UNIQUE",
 /* 207 */ "optional_or_unique ::= TOK_OPTIONAL TOK_UNIQUE",
 /* 208 */ "optional_or_unique ::= TOK_UNIQUE TOK_OPTIONAL",
 /* 209 */ "optional_fixed ::=",
 /* 210 */ "optional_fixed ::= TOK_FIXED",
 /* 211 */ "precision_spec ::=",
 /* 212 */ "precision_spec ::= TOK_LEFT_PAREN expression TOK_RIGHT_PAREN",
 /* 213 */ "proc_call_statement ::= procedure_id actual_parameters semicolon",
 /* 214 */ "proc_call_statement ::= procedure_id semicolon",
 /* 215 */ "procedure_decl ::= procedure_header action_body TOK_END_PROCEDURE semicolon",
 /* 216 */ "procedure_header ::= TOK_PROCEDURE ph_get_line ph_push_scope formal_parameter_list semicolon",
 /* 217 */ "ph_push_scope ::= TOK_IDENTIFIER",
 /* 218 */ "ph_get_line ::=",
 /* 219 */ "procedure_id ::= TOK_IDENTIFIER",
 /* 220 */ "procedure_id ::= TOK_BUILTIN_PROCEDURE",
 /* 221 */ "group_ref ::= TOK_BACKSLASH TOK_IDENTIFIER",
 /* 222 */ "qualifier ::= TOK_DOT TOK_IDENTIFIER",
 /* 223 */ "qualifier ::= TOK_BACKSLASH TOK_IDENTIFIER",
 /* 224 */ "qualifier ::= TOK_LEFT_BRACKET simple_expression TOK_RIGHT_BRACKET",
 /* 225 */ "qualifier ::= TOK_LEFT_BRACKET simple_expression TOK_COLON simple_expression TOK_RIGHT_BRACKET",
 /* 226 */ "query_expression ::= query_start expression TOK_RIGHT_PAREN",
 /* 227 */ "query_start ::= TOK_QUERY TOK_LEFT_PAREN TOK_IDENTIFIER TOK_ALL_IN expression TOK_SUCH_THAT",
 /* 228 */ "rel_op ::= TOK_LESS_THAN",
 /* 229 */ "rel_op ::= TOK_GREATER_THAN",
 /* 230 */ "rel_op ::= TOK_EQUAL",
 /* 231 */ "rel_op ::= TOK_LESS_EQUAL",
 /* 232 */ "rel_op ::= TOK_GREATER_EQUAL",
 /* 233 */ "rel_op ::= TOK_NOT_EQUAL",
 /* 234 */ "rel_op ::= TOK_INST_EQUAL",
 /* 235 */ "rel_op ::= TOK_INST_NOT_EQUAL",
 /* 236 */ "repeat_statement ::= TOK_REPEAT increment_control while_control until_control semicolon statement_rep TOK_END_REPEAT semicolon",
 /* 237 */ "repeat_statement ::= TOK_REPEAT while_control until_control semicolon statement_rep TOK_END_REPEAT semicolon",
 /* 238 */ "return_statement ::= TOK_RETURN semicolon",
 /* 239 */ "return_statement ::= TOK_RETURN TOK_LEFT_PAREN expression TOK_RIGHT_PAREN semicolon",
 /* 240 */ "right_curl ::= TOK_RIGHT_CURL",
 /* 241 */ "rule_decl ::= rule_header action_body where_rule TOK_END_RULE semicolon",
 /* 242 */ "rule_formal_parameter ::= TOK_IDENTIFIER",
 /* 243 */ "rule_formal_parameter_list ::= rule_formal_parameter",
 /* 244 */ "rule_formal_parameter_list ::= rule_formal_parameter_list TOK_COMMA rule_formal_parameter",
 /* 245 */ "rule_header ::= rh_start rule_formal_parameter_list TOK_RIGHT_PAREN semicolon",
 /* 246 */ "rh_start ::= TOK_RULE rh_get_line TOK_IDENTIFIER TOK_FOR TOK_LEFT_PAREN",
 /* 247 */ "rh_get_line ::=",
 /* 248 */ "schema_body ::= interface_specification_list block_list",
 /* 249 */ "schema_body ::= interface_specification_list constant_decl block_list",
 /* 250 */ "schema_decl ::= schema_header schema_body TOK_END_SCHEMA semicolon",
 /* 251 */ "schema_decl ::= include_directive",
 /* 252 */ "schema_header ::= TOK_SCHEMA TOK_IDENTIFIER semicolon",
 /* 253 */ "select_type ::= TOK_SELECT TOK_LEFT_PAREN defined_type_list TOK_RIGHT_PAREN",
 /* 254 */ "semicolon ::= TOK_SEMICOLON",
 /* 255 */ "set_type ::= TOK_SET bound_spec TOK_OF attribute_type",
 /* 256 */ "set_type ::= TOK_SET TOK_OF attribute_type",
 /* 257 */ "skip_statement ::= TOK_SKIP semicolon",
 /* 258 */ "statement ::= alias_statement",
 /* 259 */ "statement ::= assignment_statement",
 /* 260 */ "statement ::= case_statement",
 /* 261 */ "statement ::= compound_statement",
 /* 262 */ "statement ::= escape_statement",
 /* 263 */ "statement ::= if_statement",
 /* 264 */ "statement ::= proc_call_statement",
 /* 265 */ "statement ::= repeat_statement",
 /* 266 */ "statement ::= return_statement",
 /* 267 */ "statement ::= skip_statement",
 /* 268 */ "statement_rep ::=",
 /* 269 */ "statement_rep ::= semicolon statement_rep",
 /* 270 */ "statement_rep ::= statement statement_rep",
 /* 271 */ "subsuper_decl ::=",
 /* 272 */ "subsuper_decl ::= supertype_decl",
 /* 273 */ "subsuper_decl ::= subtype_decl",
 /* 274 */ "subsuper_decl ::= supertype_decl subtype_decl",
 /* 275 */ "subtype_decl ::= TOK_SUBTYPE TOK_OF TOK_LEFT_PAREN defined_type_list TOK_RIGHT_PAREN",
 /* 276 */ "supertype_decl ::= TOK_ABSTRACT TOK_SUPERTYPE",
 /* 277 */ "supertype_decl ::= TOK_SUPERTYPE TOK_OF TOK_LEFT_PAREN supertype_expression TOK_RIGHT_PAREN",
 /* 278 */ "supertype_decl ::= TOK_ABSTRACT TOK_SUPERTYPE TOK_OF TOK_LEFT_PAREN supertype_expression TOK_RIGHT_PAREN",
 /* 279 */ "supertype_expression ::= supertype_factor",
 /* 280 */ "supertype_expression ::= supertype_expression TOK_AND supertype_factor",
 /* 281 */ "supertype_expression ::= supertype_expression TOK_ANDOR supertype_factor",
 /* 282 */ "supertype_expression_list ::= supertype_expression",
 /* 283 */ "supertype_expression_list ::= supertype_expression_list TOK_COMMA supertype_expression",
 /* 284 */ "supertype_factor ::= identifier",
 /* 285 */ "supertype_factor ::= oneof_op TOK_LEFT_PAREN supertype_expression_list TOK_RIGHT_PAREN",
 /* 286 */ "supertype_factor ::= TOK_LEFT_PAREN supertype_expression TOK_RIGHT_PAREN",
 /* 287 */ "type ::= aggregation_type",
 /* 288 */ "type ::= basic_type",
 /* 289 */ "type ::= defined_type",
 /* 290 */ "type ::= select_type",
 /* 291 */ "type_item_body ::= enumeration_type",
 /* 292 */ "type_item_body ::= type",
 /* 293 */ "type_item ::= ti_start type_item_body semicolon",
 /* 294 */ "ti_start ::= TOK_IDENTIFIER TOK_EQUAL",
 /* 295 */ "type_decl ::= td_start TOK_END_TYPE semicolon",
 /* 296 */ "td_start ::= TOK_TYPE type_item where_rule_OPT",
 /* 297 */ "general_ref ::= assignable group_ref",
 /* 298 */ "general_ref ::= assignable",
 /* 299 */ "unary_expression ::= aggregate_initializer",
 /* 300 */ "unary_expression ::= unary_expression qualifier",
 /* 301 */ "unary_expression ::= literal",
 /* 302 */ "unary_expression ::= function_call",
 /* 303 */ "unary_expression ::= identifier",
 /* 304 */ "unary_expression ::= TOK_LEFT_PAREN expression TOK_RIGHT_PAREN",
 /* 305 */ "unary_expression ::= interval",
 /* 306 */ "unary_expression ::= query_expression",
 /* 307 */ "unary_expression ::= TOK_NOT unary_expression",
 /* 308 */ "unary_expression ::= TOK_PLUS unary_expression",
 /* 309 */ "unary_expression ::= TOK_MINUS unary_expression",
 /* 310 */ "unique ::=",
 /* 311 */ "unique ::= TOK_UNIQUE",
 /* 312 */ "qualified_attr ::= attribute_decl",
 /* 313 */ "qualified_attr_list ::= qualified_attr",
 /* 314 */ "qualified_attr_list ::= qualified_attr_list TOK_COMMA qualified_attr",
 /* 315 */ "labelled_attrib_list ::= qualified_attr_list semicolon",
 /* 316 */ "labelled_attrib_list ::= TOK_IDENTIFIER TOK_COLON qualified_attr_list semicolon",
 /* 317 */ "labelled_attrib_list_list ::= labelled_attrib_list",
 /* 318 */ "labelled_attrib_list_list ::= labelled_attrib_list_list labelled_attrib_list",
 /* 319 */ "unique_clause ::=",
 /* 320 */ "unique_clause ::= TOK_UNIQUE labelled_attrib_list_list",
 /* 321 */ "until_control ::=",
 /* 322 */ "until_control ::= TOK_UNTIL expression",
 /* 323 */ "where_clause ::= expression semicolon",
 /* 324 */ "where_clause ::= TOK_IDENTIFIER TOK_COLON expression semicolon",
 /* 325 */ "where_clause_list ::= where_clause",
 /* 326 */ "where_clause_list ::= where_clause_list where_clause",
 /* 327 */ "where_rule ::= TOK_WHERE where_clause_list",
 /* 328 */ "where_rule_OPT ::=",
 /* 329 */ "where_rule_OPT ::= where_rule",
 /* 330 */ "while_control ::=",
 /* 331 */ "while_control ::= TOK_WHILE expression",
};
#endif /* NDEBUG */


#if YYSTACKDEPTH<=0
/*
** Try to increase the size of the parser stack.
*/
static void yyGrowStack(yyParser *p){
  int newSize;
  yyStackEntry *pNew;

  newSize = p->yystksz*2 + 256;
  pNew = realloc(p->yystack, newSize*sizeof(pNew[0]));
  if( pNew ){
    p->yystack = pNew;
    p->yystksz = newSize;
#ifndef NDEBUG
    if( yyTraceFILE ){
      fprintf(yyTraceFILE,"%sStack grows to %d entries!\n",
              yyTracePrompt, p->yystksz);
    }
#endif
  }
}
#endif

/* 
** This function allocates a new parser.
** The only argument is a pointer to a function which works like
** malloc.
**
** Inputs:
** A pointer to the function used to allocate memory.
**
** Outputs:
** A pointer to a parser.  This pointer is used in subsequent calls
** to Parse and ParseFree.
*/
void *ParseAlloc(void *(*mallocProc)(size_t)){
  yyParser *pParser;
  pParser = (yyParser*)(*mallocProc)( (size_t)sizeof(yyParser) );
  if( pParser ){
    pParser->yyidx = -1;
#ifdef YYTRACKMAXSTACKDEPTH
    pParser->yyidxMax = 0;
#endif
#if YYSTACKDEPTH<=0
    pParser->yystack = NULL;
    pParser->yystksz = 0;
    yyGrowStack(pParser);
#endif
  }
  return pParser;
}

/* The following function deletes the value associated with a
** symbol.  The symbol can be either a terminal or nonterminal.
** "yymajor" is the symbol code, and "yypminor" is a pointer to
** the value.
*/
static void yy_destructor(
  yyParser *yypParser,    /* The parser */
  YYCODETYPE yymajor,     /* Type code for object to destroy */
  YYMINORTYPE *yypminor   /* The object to be destroyed */
){
  ParseARG_FETCH;
  switch( yymajor ){
    /* Here is inserted the actions which take place when a
    ** terminal or non-terminal is destroyed.  This can happen
    ** when the symbol is popped from the stack during a
    ** reduce or during error processing or when a parser is 
    ** being destroyed before it is finished parsing.
    **
    ** Note: during a reduce, the only symbols destroyed are those
    ** which appear on the RHS of the rule, but which are not used
    ** inside the C code.
    */
    case 122: /* statement_list */
{
#line 192 "expparse.y"

    if (parseData.scanner == NULL) {
    (yypminor->yy0).string = (char*)NULL;
    }

#line 1617 "expparse.c"
}
      break;
    default:  break;   /* If no destructor action specified: do nothing */
  }
}

/*
** Pop the parser's stack once.
**
** If there is a destructor routine associated with the token which
** is popped from the stack, then call it.
**
** Return the major token number for the symbol popped.
*/
static int yy_pop_parser_stack(yyParser *pParser){
  YYCODETYPE yymajor;
  yyStackEntry *yytos;

  if( pParser->yyidx<0 ) return 0;

  yytos = &pParser->yystack[pParser->yyidx];

#ifndef NDEBUG
  if( yyTraceFILE && pParser->yyidx>=0 ){
    fprintf(yyTraceFILE,"%sPopping %s\n",
      yyTracePrompt,
      yyTokenName[yytos->major]);
  }
#endif
  yymajor = yytos->major;
  yy_destructor(pParser, yymajor, &yytos->minor);
  pParser->yyidx--;
  return yymajor;
}

/* 
** Deallocate and destroy a parser.  Destructors are all called for
** all stack elements before shutting the parser down.
**
** Inputs:
** <ul>
** <li>  A pointer to the parser.  This should be a pointer
**       obtained from ParseAlloc.
** <li>  A pointer to a function used to reclaim memory obtained
**       from malloc.
** </ul>
*/
void ParseFree(
  void *p,                    /* The parser to be deleted */
  void (*freeProc)(void*)     /* Function used to reclaim memory */
){
  yyParser *pParser = (yyParser*)p;
  if( pParser==0 ) return;
  while( pParser->yyidx>=0 ) yy_pop_parser_stack(pParser);
#if YYSTACKDEPTH<=0
  free(pParser->yystack);
#endif
  (*freeProc)((void*)pParser);
}

/*
** Return the peak depth of the stack for a parser.
*/
#ifdef YYTRACKMAXSTACKDEPTH
int ParseStackPeak(void *p){
  yyParser *pParser = (yyParser*)p;
  return pParser->yyidxMax;
}
#endif

/*
** Find the appropriate action for a parser given the terminal
** look-ahead token iLookAhead.
**
** If the look-ahead token is YYNOCODE, then check to see if the action is
** independent of the look-ahead.  If it is, return the action, otherwise
** return YY_NO_ACTION.
*/
static int yy_find_shift_action(
  yyParser *pParser,        /* The parser */
  YYCODETYPE iLookAhead     /* The look-ahead token */
){
  int i;
  int stateno = pParser->yystack[pParser->yyidx].stateno;
 
  if( stateno>YY_SHIFT_COUNT
   || (i = yy_shift_ofst[stateno])==YY_SHIFT_USE_DFLT ){
    return yy_default[stateno];
  }
  assert( iLookAhead!=YYNOCODE );
  i += iLookAhead;
  if( i<0 || i>=YY_ACTTAB_COUNT || yy_lookahead[i]!=iLookAhead ){
    if( iLookAhead>0 ){
#ifdef YYFALLBACK
      YYCODETYPE iFallback;            /* Fallback token */
      if( iLookAhead<sizeof(yyFallback)/sizeof(yyFallback[0])
             && (iFallback = yyFallback[iLookAhead])!=0 ){
#ifndef NDEBUG
        if( yyTraceFILE ){
          fprintf(yyTraceFILE, "%sFALLBACK %s => %s\n",
             yyTracePrompt, yyTokenName[iLookAhead], yyTokenName[iFallback]);
        }
#endif
        return yy_find_shift_action(pParser, iFallback);
      }
#endif
#ifdef YYWILDCARD
      {
        int j = i - iLookAhead + YYWILDCARD;
        if( 
#if YY_SHIFT_MIN+YYWILDCARD<0
          j>=0 &&
#endif
#if YY_SHIFT_MAX+YYWILDCARD>=YY_ACTTAB_COUNT
          j<YY_ACTTAB_COUNT &&
#endif
          yy_lookahead[j]==YYWILDCARD
        ){
#ifndef NDEBUG
          if( yyTraceFILE ){
            fprintf(yyTraceFILE, "%sWILDCARD %s => %s\n",
               yyTracePrompt, yyTokenName[iLookAhead], yyTokenName[YYWILDCARD]);
          }
#endif /* NDEBUG */
          return yy_action[j];
        }
      }
#endif /* YYWILDCARD */
    }
    return yy_default[stateno];
  }else{
    return yy_action[i];
  }
}

/*
** Find the appropriate action for a parser given the non-terminal
** look-ahead token iLookAhead.
**
** If the look-ahead token is YYNOCODE, then check to see if the action is
** independent of the look-ahead.  If it is, return the action, otherwise
** return YY_NO_ACTION.
*/
static int yy_find_reduce_action(
  int stateno,              /* Current state number */
  YYCODETYPE iLookAhead     /* The look-ahead token */
){
  int i;
#ifdef YYERRORSYMBOL
  if( stateno>YY_REDUCE_COUNT ){
    return yy_default[stateno];
  }
#else
  assert( stateno<=YY_REDUCE_COUNT );
#endif
  i = yy_reduce_ofst[stateno];
  assert( i!=YY_REDUCE_USE_DFLT );
  assert( iLookAhead!=YYNOCODE );
  i += iLookAhead;
#ifdef YYERRORSYMBOL
  if( i<0 || i>=YY_ACTTAB_COUNT || yy_lookahead[i]!=iLookAhead ){
    return yy_default[stateno];
  }
#else
  assert( i>=0 && i<YY_ACTTAB_COUNT );
  assert( yy_lookahead[i]==iLookAhead );
#endif
  return yy_action[i];
}

/*
** The following routine is called if the stack overflows.
*/
static void yyStackOverflow(yyParser *yypParser, YYMINORTYPE *yypMinor){
   ParseARG_FETCH;
   yypParser->yyidx--;
#ifndef NDEBUG
   if( yyTraceFILE ){
     fprintf(yyTraceFILE,"%sStack Overflow!\n",yyTracePrompt);
   }
#endif
   while( yypParser->yyidx>=0 ) yy_pop_parser_stack(yypParser);
   /* Here code is inserted which will execute if the parser
   ** stack every overflows */
#line 2508 "expparse.y"

    fprintf(stderr, "Express parser experienced stack overflow.\n");
    fprintf(stderr, "Last token had value %x\n", yypMinor->yy0.val);
#line 1806 "expparse.c"
   ParseARG_STORE; /* Suppress warning about unused %extra_argument var */
}

/*
** Perform a shift action.
*/
static void yy_shift(
  yyParser *yypParser,          /* The parser to be shifted */
  int yyNewState,               /* The new state to shift in */
  int yyMajor,                  /* The major token to shift in */
  YYMINORTYPE *yypMinor         /* Pointer to the minor token to shift in */
){
  yyStackEntry *yytos;
  yypParser->yyidx++;
#ifdef YYTRACKMAXSTACKDEPTH
  if( yypParser->yyidx>yypParser->yyidxMax ){
    yypParser->yyidxMax = yypParser->yyidx;
  }
#endif
#if YYSTACKDEPTH>0 
  if( yypParser->yyidx>=YYSTACKDEPTH ){
    yyStackOverflow(yypParser, yypMinor);
    return;
  }
#else
  if( yypParser->yyidx>=yypParser->yystksz ){
    yyGrowStack(yypParser);
    if( yypParser->yyidx>=yypParser->yystksz ){
      yyStackOverflow(yypParser, yypMinor);
      return;
    }
  }
#endif
  yytos = &yypParser->yystack[yypParser->yyidx];
  yytos->stateno = (YYACTIONTYPE)yyNewState;
  yytos->major = (YYCODETYPE)yyMajor;
  yytos->minor = *yypMinor;
#ifndef NDEBUG
  if( yyTraceFILE && yypParser->yyidx>0 ){
    int i;
    fprintf(yyTraceFILE,"%sShift %d\n",yyTracePrompt,yyNewState);
    fprintf(yyTraceFILE,"%sStack:",yyTracePrompt);
    for(i=1; i<=yypParser->yyidx; i++)
      fprintf(yyTraceFILE," %s",yyTokenName[yypParser->yystack[i].major]);
    fprintf(yyTraceFILE,"\n");
  }
#endif
}

/* The following table contains information about every rule that
** is used during the reduce.
*/
static const struct {
  YYCODETYPE lhs;         /* Symbol on the left-hand side of the rule */
  unsigned char nrhs;     /* Number of right-hand side symbols in the rule */
} yyRuleInfo[] = {
  { 156, 2 },
  { 233, 1 },
  { 233, 1 },
  { 233, 1 },
  { 232, 0 },
  { 232, 2 },
  { 157, 3 },
  { 157, 2 },
  { 127, 2 },
  { 127, 3 },
  { 126, 1 },
  { 158, 1 },
  { 158, 3 },
  { 158, 3 },
  { 158, 5 },
  { 217, 3 },
  { 217, 5 },
  { 218, 1 },
  { 218, 1 },
  { 218, 1 },
  { 218, 1 },
  { 195, 9 },
  { 238, 0 },
  { 219, 5 },
  { 128, 2 },
  { 128, 1 },
  { 196, 4 },
  { 211, 1 },
  { 211, 1 },
  { 211, 1 },
  { 159, 0 },
  { 159, 2 },
  { 220, 4 },
  { 220, 3 },
  { 215, 1 },
  { 215, 2 },
  { 215, 2 },
  { 215, 1 },
  { 215, 1 },
  { 215, 3 },
  { 215, 3 },
  { 239, 0 },
  { 239, 2 },
  { 240, 1 },
  { 240, 1 },
  { 240, 1 },
  { 130, 0 },
  { 130, 2 },
  { 226, 5 },
  { 123, 3 },
  { 160, 0 },
  { 160, 2 },
  { 161, 2 },
  { 162, 1 },
  { 162, 3 },
  { 124, 0 },
  { 124, 3 },
  { 197, 6 },
  { 198, 4 },
  { 131, 1 },
  { 131, 1 },
  { 243, 6 },
  { 244, 0 },
  { 244, 2 },
  { 235, 4 },
  { 234, 1 },
  { 234, 1 },
  { 234, 1 },
  { 234, 1 },
  { 164, 0 },
  { 164, 2 },
  { 229, 5 },
  { 183, 1 },
  { 183, 2 },
  { 125, 5 },
  { 245, 6 },
  { 249, 2 },
  { 250, 3 },
  { 199, 2 },
  { 129, 1 },
  { 129, 5 },
  { 182, 1 },
  { 182, 3 },
  { 190, 0 },
  { 190, 1 },
  { 165, 5 },
  { 251, 1 },
  { 252, 1 },
  { 252, 2 },
  { 132, 1 },
  { 132, 3 },
  { 132, 3 },
  { 132, 3 },
  { 132, 3 },
  { 132, 3 },
  { 132, 3 },
  { 132, 3 },
  { 132, 3 },
  { 132, 3 },
  { 132, 3 },
  { 132, 3 },
  { 132, 3 },
  { 132, 3 },
  { 132, 3 },
  { 144, 1 },
  { 144, 3 },
  { 144, 3 },
  { 144, 3 },
  { 144, 3 },
  { 144, 3 },
  { 144, 3 },
  { 144, 3 },
  { 144, 3 },
  { 166, 1 },
  { 166, 3 },
  { 191, 0 },
  { 191, 1 },
  { 167, 4 },
  { 168, 0 },
  { 168, 3 },
  { 169, 1 },
  { 169, 3 },
  { 213, 1 },
  { 213, 1 },
  { 213, 1 },
  { 213, 1 },
  { 133, 2 },
  { 246, 4 },
  { 149, 6 },
  { 150, 1 },
  { 254, 1 },
  { 255, 1 },
  { 209, 1 },
  { 209, 1 },
  { 221, 1 },
  { 221, 4 },
  { 221, 5 },
  { 221, 3 },
  { 221, 4 },
  { 221, 4 },
  { 221, 5 },
  { 221, 3 },
  { 221, 4 },
  { 214, 1 },
  { 214, 3 },
  { 170, 1 },
  { 170, 3 },
  { 136, 1 },
  { 136, 1 },
  { 136, 1 },
  { 200, 6 },
  { 200, 8 },
  { 241, 3 },
  { 256, 6 },
  { 137, 2 },
  { 257, 1 },
  { 257, 3 },
  { 258, 1 },
  { 258, 3 },
  { 259, 3 },
  { 260, 4 },
  { 260, 3 },
  { 261, 3 },
  { 262, 4 },
  { 262, 3 },
  { 263, 3 },
  { 264, 1 },
  { 264, 1 },
  { 265, 0 },
  { 265, 2 },
  { 138, 7 },
  { 224, 1 },
  { 224, 3 },
  { 224, 4 },
  { 224, 4 },
  { 224, 3 },
  { 180, 1 },
  { 180, 2 },
  { 228, 6 },
  { 181, 0 },
  { 181, 2 },
  { 227, 5 },
  { 222, 5 },
  { 222, 4 },
  { 139, 1 },
  { 139, 1 },
  { 139, 1 },
  { 139, 1 },
  { 139, 1 },
  { 139, 1 },
  { 139, 1 },
  { 140, 2 },
  { 267, 4 },
  { 267, 5 },
  { 268, 0 },
  { 268, 2 },
  { 236, 6 },
  { 269, 0 },
  { 270, 0 },
  { 212, 1 },
  { 171, 1 },
  { 171, 3 },
  { 172, 3 },
  { 271, 1 },
  { 188, 0 },
  { 188, 1 },
  { 188, 1 },
  { 188, 2 },
  { 188, 2 },
  { 189, 0 },
  { 189, 1 },
  { 141, 0 },
  { 141, 3 },
  { 201, 3 },
  { 201, 2 },
  { 247, 4 },
  { 154, 5 },
  { 272, 1 },
  { 155, 0 },
  { 210, 1 },
  { 210, 1 },
  { 135, 2 },
  { 194, 2 },
  { 194, 2 },
  { 194, 3 },
  { 194, 5 },
  { 142, 3 },
  { 143, 6 },
  { 187, 1 },
  { 187, 1 },
  { 187, 1 },
  { 187, 1 },
  { 187, 1 },
  { 187, 1 },
  { 187, 1 },
  { 187, 1 },
  { 202, 8 },
  { 202, 7 },
  { 203, 2 },
  { 203, 5 },
  { 266, 1 },
  { 242, 5 },
  { 230, 1 },
  { 185, 1 },
  { 185, 3 },
  { 151, 4 },
  { 152, 5 },
  { 153, 0 },
  { 273, 2 },
  { 273, 3 },
  { 253, 4 },
  { 253, 1 },
  { 274, 3 },
  { 216, 4 },
  { 237, 1 },
  { 223, 4 },
  { 223, 3 },
  { 204, 2 },
  { 205, 1 },
  { 205, 1 },
  { 205, 1 },
  { 205, 1 },
  { 205, 1 },
  { 205, 1 },
  { 205, 1 },
  { 205, 1 },
  { 205, 1 },
  { 205, 1 },
  { 173, 0 },
  { 173, 2 },
  { 173, 2 },
  { 206, 0 },
  { 206, 1 },
  { 206, 1 },
  { 206, 2 },
  { 174, 5 },
  { 207, 2 },
  { 207, 5 },
  { 207, 6 },
  { 146, 1 },
  { 146, 3 },
  { 146, 3 },
  { 177, 1 },
  { 177, 3 },
  { 208, 1 },
  { 208, 4 },
  { 208, 3 },
  { 225, 1 },
  { 225, 1 },
  { 225, 1 },
  { 225, 1 },
  { 275, 1 },
  { 275, 1 },
  { 276, 3 },
  { 277, 2 },
  { 248, 3 },
  { 278, 3 },
  { 134, 2 },
  { 134, 1 },
  { 145, 1 },
  { 145, 2 },
  { 145, 1 },
  { 145, 1 },
  { 145, 1 },
  { 145, 3 },
  { 145, 1 },
  { 145, 1 },
  { 145, 2 },
  { 145, 2 },
  { 145, 2 },
  { 192, 0 },
  { 192, 1 },
  { 193, 1 },
  { 186, 1 },
  { 186, 3 },
  { 179, 2 },
  { 179, 4 },
  { 178, 1 },
  { 178, 2 },
  { 184, 0 },
  { 184, 2 },
  { 147, 0 },
  { 147, 2 },
  { 231, 2 },
  { 231, 4 },
  { 163, 1 },
  { 163, 2 },
  { 175, 2 },
  { 176, 0 },
  { 176, 1 },
  { 148, 0 },
  { 148, 2 },
};

static void yy_accept(yyParser*);  /* Forward Declaration */

/*
** Perform a reduce action and the shift that must immediately
** follow the reduce.
*/
static void yy_reduce(
  yyParser *yypParser,         /* The parser */
  int yyruleno                 /* Number of the rule by which to reduce */
){
  int yygoto;                     /* The next state */
  int yyact;                      /* The next action */
  YYMINORTYPE yygotominor;        /* The LHS of the rule reduced */
  yyStackEntry *yymsp;            /* The top of the parser's stack */
  int yysize;                     /* Amount to pop the stack */
  ParseARG_FETCH;

  yymsp = &yypParser->yystack[yypParser->yyidx];

  if( yyruleno>=0 ) {
#ifndef NDEBUG
      if ( yyruleno<(int)(sizeof(yyRuleName)/sizeof(yyRuleName[0]))) {
         if (yyTraceFILE) {
      fprintf(yyTraceFILE, "%sReduce [%s].\n", yyTracePrompt,
              yyRuleName[yyruleno]);
    }
   }
#endif /* NDEBUG */
  } else {
    /* invalid rule number range */
    return;
  }


  /* Silence complaints from purify about yygotominor being uninitialized
  ** in some cases when it is copied into the stack after the following
  ** switch.  yygotominor is uninitialized when a rule reduces that does
  ** not set the value of its left-hand side nonterminal.  Leaving the
  ** value of the nonterminal uninitialized is utterly harmless as long
  ** as the value is never used.  So really the only thing this code
  ** accomplishes is to quieten purify.  
  **
  ** 2007-01-16:  The wireshark project (www.wireshark.org) reports that
  ** without this code, their parser segfaults.  I'm not sure what there
  ** parser is doing to make this happen.  This is the second bug report
  ** from wireshark this week.  Clearly they are stressing Lemon in ways
  ** that it has not been previously stressed...  (SQLite ticket #2172)
  */
  /*memset(&yygotominor, 0, sizeof(yygotominor));*/
  yygotominor = yyzerominor;


  switch( yyruleno ){
  /* Beginning here are the reduction cases.  A typical example
  ** follows:
  **   case 0:
  **  #line <lineno> <grammarfile>
  **     { ... }           // User supplied code
  **  #line <lineno> <thisfile>
  **     break;
  */
      case 0: /* action_body ::= action_body_item_rep statement_rep */
      case 70: /* derive_decl ::= TOK_DERIVE derived_attribute_rep */ yytestcase(yyruleno==70);
      case 180: /* inverse_clause ::= TOK_INVERSE inverse_attr_list */ yytestcase(yyruleno==180);
      case 269: /* statement_rep ::= semicolon statement_rep */ yytestcase(yyruleno==269);
      case 320: /* unique_clause ::= TOK_UNIQUE labelled_attrib_list_list */ yytestcase(yyruleno==320);
      case 327: /* where_rule ::= TOK_WHERE where_clause_list */ yytestcase(yyruleno==327);
      case 329: /* where_rule_OPT ::= where_rule */ yytestcase(yyruleno==329);
#line 365 "expparse.y"
{
    yygotominor.yy371 = yymsp[0].minor.yy371;
}
#line 2269 "expparse.c"
        break;
      case 1: /* action_body_item ::= declaration */
      case 2: /* action_body_item ::= constant_decl */ yytestcase(yyruleno==2);
      case 3: /* action_body_item ::= local_decl */ yytestcase(yyruleno==3);
      case 43: /* block_member ::= declaration */ yytestcase(yyruleno==43);
      case 44: /* block_member ::= include_directive */ yytestcase(yyruleno==44);
      case 45: /* block_member ::= rule_decl */ yytestcase(yyruleno==45);
      case 65: /* declaration ::= entity_decl */ yytestcase(yyruleno==65);
      case 66: /* declaration ::= function_decl */ yytestcase(yyruleno==66);
      case 67: /* declaration ::= procedure_decl */ yytestcase(yyruleno==67);
      case 68: /* declaration ::= type_decl */ yytestcase(yyruleno==68);
      case 87: /* schema_decl_list ::= schema_decl */ yytestcase(yyruleno==87);
      case 157: /* rename_list ::= rename */ yytestcase(yyruleno==157);
      case 166: /* interface_specification ::= use_clause */ yytestcase(yyruleno==166);
      case 167: /* interface_specification ::= reference_clause */ yytestcase(yyruleno==167);
      case 203: /* oneof_op ::= TOK_ONEOF */ yytestcase(yyruleno==203);
      case 251: /* schema_decl ::= include_directive */ yytestcase(yyruleno==251);
      case 291: /* type_item_body ::= enumeration_type */ yytestcase(yyruleno==291);
#line 371 "expparse.y"
{
    yygotominor.yy0 = yymsp[0].minor.yy0;
}
#line 2292 "expparse.c"
        break;
      case 5: /* action_body_item_rep ::= action_body_item action_body_item_rep */
      case 42: /* block_list ::= block_list block_member */ yytestcase(yyruleno==42);
      case 63: /* constant_body_list ::= constant_body constant_body_list */ yytestcase(yyruleno==63);
      case 88: /* schema_decl_list ::= schema_decl_list schema_decl */ yytestcase(yyruleno==88);
      case 169: /* interface_specification_list ::= interface_specification_list interface_specification */ yytestcase(yyruleno==169);
      case 195: /* local_body ::= local_variable local_body */ yytestcase(yyruleno==195);
      case 248: /* schema_body ::= interface_specification_list block_list */ yytestcase(yyruleno==248);
#line 388 "expparse.y"
{
    yygotominor.yy0 = yymsp[-1].minor.yy0;
}
#line 2305 "expparse.c"
        break;
      case 6: /* actual_parameters ::= TOK_LEFT_PAREN expression_list TOK_RIGHT_PAREN */
      case 202: /* nested_id_list ::= TOK_LEFT_PAREN id_list TOK_RIGHT_PAREN */ yytestcase(yyruleno==202);
      case 275: /* subtype_decl ::= TOK_SUBTYPE TOK_OF TOK_LEFT_PAREN defined_type_list TOK_RIGHT_PAREN */ yytestcase(yyruleno==275);
#line 405 "expparse.y"
{
    yygotominor.yy371 = yymsp[-1].minor.yy371;
}
#line 2314 "expparse.c"
        break;
      case 7: /* actual_parameters ::= TOK_LEFT_PAREN TOK_RIGHT_PAREN */
      case 319: /* unique_clause ::= */ yytestcase(yyruleno==319);
#line 409 "expparse.y"
{
    yygotominor.yy371 = 0;
}
#line 2322 "expparse.c"
        break;
      case 8: /* aggregate_initializer ::= TOK_LEFT_BRACKET TOK_RIGHT_BRACKET */
#line 415 "expparse.y"
{
    yygotominor.yy401 = EXPcreate(Type_Aggregate);
    yygotominor.yy401->u.list = LISTcreate();
}
#line 2330 "expparse.c"
        break;
      case 9: /* aggregate_initializer ::= TOK_LEFT_BRACKET aggregate_init_body TOK_RIGHT_BRACKET */
#line 421 "expparse.y"
{
    yygotominor.yy401 = EXPcreate(Type_Aggregate);
    yygotominor.yy401->u.list = yymsp[-1].minor.yy371;
}
#line 2338 "expparse.c"
        break;
      case 10: /* aggregate_init_element ::= expression */
      case 25: /* assignable ::= identifier */ yytestcase(yyruleno==25);
      case 47: /* by_expression ::= TOK_BY expression */ yytestcase(yyruleno==47);
      case 89: /* expression ::= simple_expression */ yytestcase(yyruleno==89);
      case 104: /* simple_expression ::= unary_expression */ yytestcase(yyruleno==104);
      case 154: /* initializer ::= TOK_ASSIGNMENT expression */ yytestcase(yyruleno==154);
      case 190: /* literal ::= constant */ yytestcase(yyruleno==190);
      case 191: /* local_initializer ::= TOK_ASSIGNMENT expression */ yytestcase(yyruleno==191);
      case 298: /* general_ref ::= assignable */ yytestcase(yyruleno==298);
      case 299: /* unary_expression ::= aggregate_initializer */ yytestcase(yyruleno==299);
      case 301: /* unary_expression ::= literal */ yytestcase(yyruleno==301);
      case 302: /* unary_expression ::= function_call */ yytestcase(yyruleno==302);
      case 303: /* unary_expression ::= identifier */ yytestcase(yyruleno==303);
      case 305: /* unary_expression ::= interval */ yytestcase(yyruleno==305);
      case 306: /* unary_expression ::= query_expression */ yytestcase(yyruleno==306);
      case 308: /* unary_expression ::= TOK_PLUS unary_expression */ yytestcase(yyruleno==308);
      case 312: /* qualified_attr ::= attribute_decl */ yytestcase(yyruleno==312);
      case 322: /* until_control ::= TOK_UNTIL expression */ yytestcase(yyruleno==322);
      case 331: /* while_control ::= TOK_WHILE expression */ yytestcase(yyruleno==331);
#line 427 "expparse.y"
{
    yygotominor.yy401 = yymsp[0].minor.yy401;
}
#line 2363 "expparse.c"
        break;
      case 11: /* aggregate_init_body ::= aggregate_init_element */
      case 113: /* expression_list ::= expression */ yytestcase(yyruleno==113);
      case 282: /* supertype_expression_list ::= supertype_expression */ yytestcase(yyruleno==282);
      case 313: /* qualified_attr_list ::= qualified_attr */ yytestcase(yyruleno==313);
#line 432 "expparse.y"
{
    yygotominor.yy371 = LISTcreate();
    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy401);
}
#line 2374 "expparse.c"
        break;
      case 12: /* aggregate_init_body ::= aggregate_init_element TOK_COLON expression */
#line 437 "expparse.y"
{
    yygotominor.yy371 = LISTcreate();
    LISTadd_last(yygotominor.yy371, (Generic)yymsp[-2].minor.yy401);

    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy401);

    yymsp[0].minor.yy401->type = Type_Repeat;
}
#line 2386 "expparse.c"
        break;
      case 13: /* aggregate_init_body ::= aggregate_init_body TOK_COMMA aggregate_init_element */
#line 447 "expparse.y"
{ 
    yygotominor.yy371 = yymsp[-2].minor.yy371;

    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy401);

}
#line 2396 "expparse.c"
        break;
      case 14: /* aggregate_init_body ::= aggregate_init_body TOK_COMMA aggregate_init_element TOK_COLON expression */
#line 455 "expparse.y"
{
    yygotominor.yy371 = yymsp[-4].minor.yy371;

    LISTadd_last(yygotominor.yy371, (Generic)yymsp[-2].minor.yy401);
    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy401);

    yymsp[0].minor.yy401->type = Type_Repeat;
}
#line 2408 "expparse.c"
        break;
      case 15: /* aggregate_type ::= TOK_AGGREGATE TOK_OF parameter_type */
#line 465 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(aggregate_);
    yygotominor.yy477->base = yymsp[0].minor.yy297;

    if (tag_count < 0) {
        Symbol sym;
        sym.line = yylineno;
        sym.filename = current_filename;
        ERRORreport_with_symbol(ERROR_unlabelled_param_type, &sym,
        CURRENT_SCOPE_NAME);
    }
}
#line 2424 "expparse.c"
        break;
      case 16: /* aggregate_type ::= TOK_AGGREGATE TOK_COLON TOK_IDENTIFIER TOK_OF parameter_type */
#line 479 "expparse.y"
{
    Type t = TYPEcreate_user_defined_tag(yymsp[0].minor.yy297, CURRENT_SCOPE, yymsp[-2].minor.yy0.symbol);

    if (t) {
        SCOPEadd_super(t);
        yygotominor.yy477 = TYPEBODYcreate(aggregate_);
        yygotominor.yy477->tag = t;
        yygotominor.yy477->base = yymsp[0].minor.yy297;
    }
}
#line 2438 "expparse.c"
        break;
      case 17: /* aggregation_type ::= array_type */
      case 18: /* aggregation_type ::= bag_type */ yytestcase(yyruleno==18);
      case 19: /* aggregation_type ::= list_type */ yytestcase(yyruleno==19);
      case 20: /* aggregation_type ::= set_type */ yytestcase(yyruleno==20);
#line 491 "expparse.y"
{
    yygotominor.yy477 = yymsp[0].minor.yy477;
}
#line 2448 "expparse.c"
        break;
      case 21: /* alias_statement ::= TOK_ALIAS TOK_IDENTIFIER TOK_FOR general_ref semicolon alias_push_scope statement_rep TOK_END_ALIAS semicolon */
#line 510 "expparse.y"
{
    Expression e = EXPcreate_from_symbol(Type_Attribute, yymsp[-7].minor.yy0.symbol);
    Variable v = VARcreate(e, Type_Unknown);

    v->initializer = yymsp[-5].minor.yy401; 

    DICTdefine(CURRENT_SCOPE->symbol_table, yymsp[-7].minor.yy0.symbol->name, (Generic)v,
        yymsp[-7].minor.yy0.symbol, OBJ_VARIABLE);
    yygotominor.yy332 = ALIAScreate(CURRENT_SCOPE, v, yymsp[-2].minor.yy371);

    POP_SCOPE();
}
#line 2464 "expparse.c"
        break;
      case 22: /* alias_push_scope ::= */
#line 524 "expparse.y"
{
    struct Scope_ *s = SCOPEcreate_tiny(OBJ_ALIAS);
    PUSH_SCOPE(s, (Symbol *)0, OBJ_ALIAS);
}
#line 2472 "expparse.c"
        break;
      case 23: /* array_type ::= TOK_ARRAY bound_spec TOK_OF optional_or_unique attribute_type */
#line 531 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(array_);

    yygotominor.yy477->flags.optional = yymsp[-1].minor.yy252.optional;
    yygotominor.yy477->flags.unique = yymsp[-1].minor.yy252.unique;
    yygotominor.yy477->upper = yymsp[-3].minor.yy253.upper_limit;
    yygotominor.yy477->lower = yymsp[-3].minor.yy253.lower_limit;
    yygotominor.yy477->base = yymsp[0].minor.yy297;
}
#line 2485 "expparse.c"
        break;
      case 24: /* assignable ::= assignable qualifier */
      case 300: /* unary_expression ::= unary_expression qualifier */ yytestcase(yyruleno==300);
#line 543 "expparse.y"
{
    yymsp[0].minor.yy46.first->e.op1 = yymsp[-1].minor.yy401;
    yygotominor.yy401 = yymsp[0].minor.yy46.expr;
}
#line 2494 "expparse.c"
        break;
      case 26: /* assignment_statement ::= assignable TOK_ASSIGNMENT expression semicolon */
#line 554 "expparse.y"
{ 
    yygotominor.yy332 = ASSIGNcreate(yymsp[-3].minor.yy401, yymsp[-1].minor.yy401);
}
#line 2501 "expparse.c"
        break;
      case 27: /* attribute_type ::= aggregation_type */
      case 28: /* attribute_type ::= basic_type */ yytestcase(yyruleno==28);
      case 122: /* parameter_type ::= basic_type */ yytestcase(yyruleno==122);
      case 123: /* parameter_type ::= conformant_aggregation */ yytestcase(yyruleno==123);
#line 559 "expparse.y"
{
    yygotominor.yy297 = TYPEcreate_from_body_anonymously(yymsp[0].minor.yy477);
    SCOPEadd_super(yygotominor.yy297);
}
#line 2512 "expparse.c"
        break;
      case 29: /* attribute_type ::= defined_type */
      case 124: /* parameter_type ::= defined_type */ yytestcase(yyruleno==124);
      case 125: /* parameter_type ::= generic_type */ yytestcase(yyruleno==125);
#line 569 "expparse.y"
{
    yygotominor.yy297 = yymsp[0].minor.yy297;
}
#line 2521 "expparse.c"
        break;
      case 30: /* explicit_attr_list ::= */
      case 50: /* case_action_list ::= */ yytestcase(yyruleno==50);
      case 69: /* derive_decl ::= */ yytestcase(yyruleno==69);
      case 268: /* statement_rep ::= */ yytestcase(yyruleno==268);
#line 574 "expparse.y"
{
    yygotominor.yy371 = LISTcreate();
}
#line 2531 "expparse.c"
        break;
      case 31: /* explicit_attr_list ::= explicit_attr_list explicit_attribute */
#line 578 "expparse.y"
{
    yygotominor.yy371 = yymsp[-1].minor.yy371;
    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy371); 
}
#line 2539 "expparse.c"
        break;
      case 32: /* bag_type ::= TOK_BAG bound_spec TOK_OF attribute_type */
      case 138: /* conformant_aggregation ::= TOK_BAG bound_spec TOK_OF parameter_type */ yytestcase(yyruleno==138);
#line 584 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(bag_);
    yygotominor.yy477->base = yymsp[0].minor.yy297;
    yygotominor.yy477->upper = yymsp[-2].minor.yy253.upper_limit;
    yygotominor.yy477->lower = yymsp[-2].minor.yy253.lower_limit;
}
#line 2550 "expparse.c"
        break;
      case 33: /* bag_type ::= TOK_BAG TOK_OF attribute_type */
#line 591 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(bag_);
    yygotominor.yy477->base = yymsp[0].minor.yy297;
}
#line 2558 "expparse.c"
        break;
      case 34: /* basic_type ::= TOK_BOOLEAN */
#line 597 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(boolean_);
}
#line 2565 "expparse.c"
        break;
      case 35: /* basic_type ::= TOK_INTEGER precision_spec */
#line 601 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(integer_);
    yygotominor.yy477->precision = yymsp[0].minor.yy401;
}
#line 2573 "expparse.c"
        break;
      case 36: /* basic_type ::= TOK_REAL precision_spec */
#line 606 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(real_);
    yygotominor.yy477->precision = yymsp[0].minor.yy401;
}
#line 2581 "expparse.c"
        break;
      case 37: /* basic_type ::= TOK_NUMBER */
#line 611 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(number_);
}
#line 2588 "expparse.c"
        break;
      case 38: /* basic_type ::= TOK_LOGICAL */
#line 615 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(logical_);
}
#line 2595 "expparse.c"
        break;
      case 39: /* basic_type ::= TOK_BINARY precision_spec optional_fixed */
#line 619 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(binary_);
    yygotominor.yy477->precision = yymsp[-1].minor.yy401;
    yygotominor.yy477->flags.fixed = yymsp[0].minor.yy252.fixed;
}
#line 2604 "expparse.c"
        break;
      case 40: /* basic_type ::= TOK_STRING precision_spec optional_fixed */
#line 625 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(string_);
    yygotominor.yy477->precision = yymsp[-1].minor.yy401;
    yygotominor.yy477->flags.fixed = yymsp[0].minor.yy252.fixed;
}
#line 2613 "expparse.c"
        break;
      case 46: /* by_expression ::= */
#line 651 "expparse.y"
{
    yygotominor.yy401 = LITERAL_ONE;
}
#line 2620 "expparse.c"
        break;
      case 48: /* cardinality_op ::= TOK_LEFT_CURL expression TOK_COLON expression TOK_RIGHT_CURL */
      case 181: /* bound_spec ::= TOK_LEFT_BRACKET expression TOK_COLON expression TOK_RIGHT_BRACKET */ yytestcase(yyruleno==181);
#line 661 "expparse.y"
{
    yygotominor.yy253.lower_limit = yymsp[-3].minor.yy401;
    yygotominor.yy253.upper_limit = yymsp[-1].minor.yy401;
}
#line 2629 "expparse.c"
        break;
      case 49: /* case_action ::= case_labels TOK_COLON statement */
#line 667 "expparse.y"
{
    yygotominor.yy321 = CASE_ITcreate(yymsp[-2].minor.yy371, yymsp[0].minor.yy332);
    SYMBOLset(yygotominor.yy321);
}
#line 2637 "expparse.c"
        break;
      case 51: /* case_action_list ::= case_action_list case_action */
#line 677 "expparse.y"
{
    yyerrok;

    yygotominor.yy371 = yymsp[-1].minor.yy371;

    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy321);
}
#line 2648 "expparse.c"
        break;
      case 52: /* case_block ::= case_action_list case_otherwise */
#line 686 "expparse.y"
{
    yygotominor.yy371 = yymsp[-1].minor.yy371;

    if (yymsp[0].minor.yy321) {
        LISTadd_last(yygotominor.yy371,
        (Generic)yymsp[0].minor.yy321);
    }
}
#line 2660 "expparse.c"
        break;
      case 53: /* case_labels ::= expression */
#line 696 "expparse.y"
{
    yygotominor.yy371 = LISTcreate();

    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy401);
}
#line 2669 "expparse.c"
        break;
      case 54: /* case_labels ::= case_labels TOK_COMMA expression */
#line 702 "expparse.y"
{
    yyerrok;

    yygotominor.yy371 = yymsp[-2].minor.yy371;
    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy401);
}
#line 2679 "expparse.c"
        break;
      case 55: /* case_otherwise ::= */
#line 710 "expparse.y"
{
    yygotominor.yy321 = (Case_Item)0;
}
#line 2686 "expparse.c"
        break;
      case 56: /* case_otherwise ::= TOK_OTHERWISE TOK_COLON statement */
#line 714 "expparse.y"
{
    yygotominor.yy321 = CASE_ITcreate(LIST_NULL, yymsp[0].minor.yy332);
    SYMBOLset(yygotominor.yy321);
}
#line 2694 "expparse.c"
        break;
      case 57: /* case_statement ::= TOK_CASE expression TOK_OF case_block TOK_END_CASE semicolon */
#line 721 "expparse.y"
{
    yygotominor.yy332 = CASEcreate(yymsp[-4].minor.yy401, yymsp[-2].minor.yy371);
}
#line 2701 "expparse.c"
        break;
      case 58: /* compound_statement ::= TOK_BEGIN statement_rep TOK_END semicolon */
#line 726 "expparse.y"
{
    yygotominor.yy332 = COMP_STMTcreate(yymsp[-2].minor.yy371);
}
#line 2708 "expparse.c"
        break;
      case 59: /* constant ::= TOK_PI */
#line 731 "expparse.y"
{ 
    yygotominor.yy401 = LITERAL_PI;
}
#line 2715 "expparse.c"
        break;
      case 60: /* constant ::= TOK_E */
#line 736 "expparse.y"
{ 
    yygotominor.yy401 = LITERAL_E;
}
#line 2722 "expparse.c"
        break;
      case 61: /* constant_body ::= identifier TOK_COLON attribute_type TOK_ASSIGNMENT expression semicolon */
#line 743 "expparse.y"
{
    Variable v;

    yymsp[-5].minor.yy401->type = yymsp[-3].minor.yy297;
    v = VARcreate(yymsp[-5].minor.yy401, yymsp[-3].minor.yy297);
    v->initializer = yymsp[-1].minor.yy401;
    v->flags.constant = 1;
    DICTdefine(CURRENT_SCOPE->symbol_table, yymsp[-5].minor.yy401->symbol.name, (Generic)v,
    &yymsp[-5].minor.yy401->symbol, OBJ_VARIABLE);
}
#line 2736 "expparse.c"
        break;
      case 64: /* constant_decl ::= TOK_CONSTANT constant_body_list TOK_END_CONSTANT semicolon */
#line 762 "expparse.y"
{
    yygotominor.yy0 = yymsp[-3].minor.yy0;
}
#line 2743 "expparse.c"
        break;
      case 71: /* derived_attribute ::= attribute_decl TOK_COLON attribute_type initializer semicolon */
#line 794 "expparse.y"
{
    yygotominor.yy91 = VARcreate(yymsp[-4].minor.yy401, yymsp[-2].minor.yy297);
    yygotominor.yy91->initializer = yymsp[-1].minor.yy401;
    yygotominor.yy91->flags.attribute = true;
}
#line 2752 "expparse.c"
        break;
      case 72: /* derived_attribute_rep ::= derived_attribute */
      case 176: /* inverse_attr_list ::= inverse_attr */ yytestcase(yyruleno==176);
#line 801 "expparse.y"
{
    yygotominor.yy371 = LISTcreate();
    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy91);
}
#line 2761 "expparse.c"
        break;
      case 73: /* derived_attribute_rep ::= derived_attribute_rep derived_attribute */
      case 177: /* inverse_attr_list ::= inverse_attr_list inverse_attr */ yytestcase(yyruleno==177);
#line 806 "expparse.y"
{
    yygotominor.yy371 = yymsp[-1].minor.yy371;
    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy91);
}
#line 2770 "expparse.c"
        break;
      case 74: /* entity_body ::= explicit_attr_list derive_decl inverse_clause unique_clause where_rule_OPT */
#line 813 "expparse.y"
{
    yygotominor.yy176.attributes = yymsp[-4].minor.yy371;
    /* this is flattened out in entity_decl - DEL */
    LISTadd_last(yygotominor.yy176.attributes, (Generic)yymsp[-3].minor.yy371);

    if (yymsp[-2].minor.yy371 != LIST_NULL) {
    LISTadd_last(yygotominor.yy176.attributes, (Generic)yymsp[-2].minor.yy371);
    }

    yygotominor.yy176.unique = yymsp[-1].minor.yy371;
    yygotominor.yy176.where = yymsp[0].minor.yy371;
}
#line 2786 "expparse.c"
        break;
      case 75: /* entity_decl ::= entity_header subsuper_decl semicolon entity_body TOK_END_ENTITY semicolon */
#line 828 "expparse.y"
{
    CURRENT_SCOPE->u.entity->subtype_expression = yymsp[-4].minor.yy242.subtypes;
    CURRENT_SCOPE->u.entity->supertype_symbols = yymsp[-4].minor.yy242.supertypes;
    LISTdo( yymsp[-2].minor.yy176.attributes, l, Linked_List ) {
        LISTdo_n( l, a, Variable, b ) {
            ENTITYadd_attribute(CURRENT_SCOPE, a);
        } LISTod;
    } LISTod;
    CURRENT_SCOPE->u.entity->abstract = yymsp[-4].minor.yy242.abstract;
    CURRENT_SCOPE->u.entity->unique = yymsp[-2].minor.yy176.unique;
    CURRENT_SCOPE->where = yymsp[-2].minor.yy176.where;
    POP_SCOPE();
}
#line 2803 "expparse.c"
        break;
      case 76: /* entity_header ::= TOK_ENTITY TOK_IDENTIFIER */
#line 843 "expparse.y"
{
    Entity e = ENTITYcreate(yymsp[0].minor.yy0.symbol);

    if (print_objects_while_running & OBJ_ENTITY_BITS) {
    fprintf( stderr, "parse: %s (entity)\n", yymsp[0].minor.yy0.symbol->name);
    }

    PUSH_SCOPE(e, yymsp[0].minor.yy0.symbol, OBJ_ENTITY);
}
#line 2816 "expparse.c"
        break;
      case 77: /* enumeration_type ::= TOK_ENUMERATION TOK_OF nested_id_list */
#line 854 "expparse.y"
{
    int value = 0;
    Expression x;
    Symbol *tmp;
    TypeBody tb;
    tb = TYPEBODYcreate(enumeration_);
    CURRENT_SCOPE->u.type->head = 0;
    CURRENT_SCOPE->u.type->body = tb;
    tb->list = yymsp[0].minor.yy371;

    if (!CURRENT_SCOPE->symbol_table) {
        CURRENT_SCOPE->symbol_table = DICTcreate(25);
    }
    if (!PREVIOUS_SCOPE->enum_table) {
        PREVIOUS_SCOPE->enum_table = DICTcreate(25);
    }
    LISTdo_links(yymsp[0].minor.yy371, id) {
        tmp = (Symbol *)id->data;
        id->data = (Generic)(x = EXPcreate(CURRENT_SCOPE));
        x->symbol = *(tmp);
        x->u.integer = ++value;

        /* define both in enum scope and scope of */
        /* 1st visibility */
        DICT_define(CURRENT_SCOPE->symbol_table, x->symbol.name,
            (Generic)x, &x->symbol, OBJ_EXPRESSION);
        DICTdefine(PREVIOUS_SCOPE->enum_table, x->symbol.name,
            (Generic)x, &x->symbol, OBJ_EXPRESSION);
        SYMBOL_destroy(tmp);
    } LISTod;
}
#line 2851 "expparse.c"
        break;
      case 78: /* escape_statement ::= TOK_ESCAPE semicolon */
#line 887 "expparse.y"
{
    yygotominor.yy332 = STATEMENT_ESCAPE;
}
#line 2858 "expparse.c"
        break;
      case 79: /* attribute_decl ::= TOK_IDENTIFIER */
#line 902 "expparse.y"
{
    yygotominor.yy401 = EXPcreate(Type_Attribute);
    yygotominor.yy401->symbol = *yymsp[0].minor.yy0.symbol;
    SYMBOL_destroy(yymsp[0].minor.yy0.symbol);
}
#line 2867 "expparse.c"
        break;
      case 80: /* attribute_decl ::= TOK_SELF TOK_BACKSLASH TOK_IDENTIFIER TOK_DOT TOK_IDENTIFIER */
#line 909 "expparse.y"
{
    yygotominor.yy401 = EXPcreate(Type_Expression);
    yygotominor.yy401->e.op1 = EXPcreate(Type_Expression);
    yygotominor.yy401->e.op1->e.op_code = OP_GROUP;
    yygotominor.yy401->e.op1->e.op1 = EXPcreate(Type_Self);
    yygotominor.yy401->e.op1->e.op2 = EXPcreate_from_symbol(Type_Entity, yymsp[-2].minor.yy0.symbol);
    SYMBOL_destroy(yymsp[-2].minor.yy0.symbol);

    yygotominor.yy401->e.op_code = OP_DOT;
    yygotominor.yy401->e.op2 = EXPcreate_from_symbol(Type_Attribute, yymsp[0].minor.yy0.symbol);
    SYMBOL_destroy(yymsp[0].minor.yy0.symbol);
}
#line 2883 "expparse.c"
        break;
      case 81: /* attribute_decl_list ::= attribute_decl */
#line 923 "expparse.y"
{
    yygotominor.yy371 = LISTcreate();
    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy401);

}
#line 2892 "expparse.c"
        break;
      case 82: /* attribute_decl_list ::= attribute_decl_list TOK_COMMA attribute_decl */
      case 114: /* expression_list ::= expression_list TOK_COMMA expression */ yytestcase(yyruleno==114);
      case 314: /* qualified_attr_list ::= qualified_attr_list TOK_COMMA qualified_attr */ yytestcase(yyruleno==314);
#line 930 "expparse.y"
{
    yygotominor.yy371 = yymsp[-2].minor.yy371;
    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy401);
}
#line 2902 "expparse.c"
        break;
      case 83: /* optional ::= */
#line 936 "expparse.y"
{
    yygotominor.yy252.optional = 0;
}
#line 2909 "expparse.c"
        break;
      case 84: /* optional ::= TOK_OPTIONAL */
#line 940 "expparse.y"
{
    yygotominor.yy252.optional = 1;
}
#line 2916 "expparse.c"
        break;
      case 85: /* explicit_attribute ::= attribute_decl_list TOK_COLON optional attribute_type semicolon */
#line 946 "expparse.y"
{
    Variable v;

    LISTdo_links (yymsp[-4].minor.yy371, attr)
    v = VARcreate((Expression)attr->data, yymsp[-1].minor.yy297);
    v->flags.optional = yymsp[-2].minor.yy252.optional;
    v->flags.attribute = true;
    attr->data = (Generic)v;
    LISTod;

    yygotominor.yy371 = yymsp[-4].minor.yy371;
}
#line 2932 "expparse.c"
        break;
      case 90: /* expression ::= expression TOK_AND expression */
#line 975 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_AND, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 2941 "expparse.c"
        break;
      case 91: /* expression ::= expression TOK_OR expression */
#line 981 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_OR, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 2950 "expparse.c"
        break;
      case 92: /* expression ::= expression TOK_XOR expression */
#line 987 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_XOR, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 2959 "expparse.c"
        break;
      case 93: /* expression ::= expression TOK_LESS_THAN expression */
#line 993 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_LESS_THAN, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 2968 "expparse.c"
        break;
      case 94: /* expression ::= expression TOK_GREATER_THAN expression */
#line 999 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_GREATER_THAN, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 2977 "expparse.c"
        break;
      case 95: /* expression ::= expression TOK_EQUAL expression */
#line 1005 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_EQUAL, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 2986 "expparse.c"
        break;
      case 96: /* expression ::= expression TOK_LESS_EQUAL expression */
#line 1011 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_LESS_EQUAL, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 2995 "expparse.c"
        break;
      case 97: /* expression ::= expression TOK_GREATER_EQUAL expression */
#line 1017 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_GREATER_EQUAL, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 3004 "expparse.c"
        break;
      case 98: /* expression ::= expression TOK_NOT_EQUAL expression */
#line 1023 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_NOT_EQUAL, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 3013 "expparse.c"
        break;
      case 99: /* expression ::= expression TOK_INST_EQUAL expression */
#line 1029 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_INST_EQUAL, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 3022 "expparse.c"
        break;
      case 100: /* expression ::= expression TOK_INST_NOT_EQUAL expression */
#line 1035 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_INST_NOT_EQUAL, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 3031 "expparse.c"
        break;
      case 101: /* expression ::= expression TOK_IN expression */
#line 1041 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_IN, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 3040 "expparse.c"
        break;
      case 102: /* expression ::= expression TOK_LIKE expression */
#line 1047 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_LIKE, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 3049 "expparse.c"
        break;
      case 103: /* expression ::= simple_expression cardinality_op simple_expression */
      case 240: /* right_curl ::= TOK_RIGHT_CURL */ yytestcase(yyruleno==240);
      case 254: /* semicolon ::= TOK_SEMICOLON */ yytestcase(yyruleno==254);
#line 1053 "expparse.y"
{
    yyerrok;
}
#line 3058 "expparse.c"
        break;
      case 105: /* simple_expression ::= simple_expression TOK_CONCAT_OP simple_expression */
#line 1063 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_CONCAT, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 3067 "expparse.c"
        break;
      case 106: /* simple_expression ::= simple_expression TOK_EXP simple_expression */
#line 1069 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_EXP, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 3076 "expparse.c"
        break;
      case 107: /* simple_expression ::= simple_expression TOK_TIMES simple_expression */
#line 1075 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_TIMES, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 3085 "expparse.c"
        break;
      case 108: /* simple_expression ::= simple_expression TOK_DIV simple_expression */
#line 1081 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_DIV, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 3094 "expparse.c"
        break;
      case 109: /* simple_expression ::= simple_expression TOK_REAL_DIV simple_expression */
#line 1087 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_REAL_DIV, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 3103 "expparse.c"
        break;
      case 110: /* simple_expression ::= simple_expression TOK_MOD simple_expression */
#line 1093 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_MOD, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 3112 "expparse.c"
        break;
      case 111: /* simple_expression ::= simple_expression TOK_PLUS simple_expression */
#line 1099 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_PLUS, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 3121 "expparse.c"
        break;
      case 112: /* simple_expression ::= simple_expression TOK_MINUS simple_expression */
#line 1105 "expparse.y"
{
    yyerrok;

    yygotominor.yy401 = BIN_EXPcreate(OP_MINUS, yymsp[-2].minor.yy401, yymsp[0].minor.yy401);
}
#line 3130 "expparse.c"
        break;
      case 115: /* var ::= */
#line 1123 "expparse.y"
{
    yygotominor.yy252.var = 0;
}
#line 3137 "expparse.c"
        break;
      case 116: /* var ::= TOK_VAR */
#line 1127 "expparse.y"
{
    yygotominor.yy252.var = 1;
}
#line 3144 "expparse.c"
        break;
      case 117: /* formal_parameter ::= var id_list TOK_COLON parameter_type */
#line 1132 "expparse.y"
{
    Symbol *tmp;
    Expression e;
    Variable v;

    yygotominor.yy371 = yymsp[-2].minor.yy371;
    LISTdo_links(yygotominor.yy371, param)
    tmp = (Symbol*)param->data;

    e = EXPcreate_from_symbol(Type_Attribute, tmp);
    v = VARcreate(e, yymsp[0].minor.yy297);
    v->flags.var = yymsp[-3].minor.yy252.var; /* NOTE this was flags.optional... ?! */
    v->flags.parameter = true;
    param->data = (Generic)v;

    /* link it in to the current scope's dict */
    DICTdefine(CURRENT_SCOPE->symbol_table,
    tmp->name, (Generic)v, tmp, OBJ_VARIABLE);

    LISTod;
}
#line 3169 "expparse.c"
        break;
      case 118: /* formal_parameter_list ::= */
      case 179: /* inverse_clause ::= */ yytestcase(yyruleno==179);
      case 328: /* where_rule_OPT ::= */ yytestcase(yyruleno==328);
#line 1155 "expparse.y"
{
    yygotominor.yy371 = LIST_NULL;
}
#line 3178 "expparse.c"
        break;
      case 119: /* formal_parameter_list ::= TOK_LEFT_PAREN formal_parameter_rep TOK_RIGHT_PAREN */
#line 1160 "expparse.y"
{
    yygotominor.yy371 = yymsp[-1].minor.yy371;

}
#line 3186 "expparse.c"
        break;
      case 120: /* formal_parameter_rep ::= formal_parameter */
#line 1166 "expparse.y"
{
    yygotominor.yy371 = yymsp[0].minor.yy371;

}
#line 3194 "expparse.c"
        break;
      case 121: /* formal_parameter_rep ::= formal_parameter_rep semicolon formal_parameter */
#line 1172 "expparse.y"
{
    yygotominor.yy371 = yymsp[-2].minor.yy371;
    LISTadd_all(yygotominor.yy371, yymsp[0].minor.yy371);
}
#line 3202 "expparse.c"
        break;
      case 126: /* function_call ::= function_id actual_parameters */
#line 1197 "expparse.y"
{
    yygotominor.yy401 = EXPcreate(Type_Funcall);
    yygotominor.yy401->symbol = *yymsp[-1].minor.yy275;
    SYMBOL_destroy(yymsp[-1].minor.yy275);
    yygotominor.yy401->u.funcall.list = yymsp[0].minor.yy371;
}
#line 3212 "expparse.c"
        break;
      case 127: /* function_decl ::= function_header action_body TOK_END_FUNCTION semicolon */
#line 1206 "expparse.y"
{
    FUNCput_body(CURRENT_SCOPE, yymsp[-2].minor.yy371);
    ALGput_full_text(CURRENT_SCOPE, yymsp[-3].minor.yy507, SCANtell());
    POP_SCOPE();
}
#line 3221 "expparse.c"
        break;
      case 128: /* function_header ::= fh_lineno fh_push_scope fh_plist TOK_COLON parameter_type semicolon */
#line 1214 "expparse.y"
{ 
    Function f = CURRENT_SCOPE;

    f->u.func->return_type = yymsp[-1].minor.yy297;
    yygotominor.yy507 = yymsp[-5].minor.yy507;
}
#line 3231 "expparse.c"
        break;
      case 129: /* fh_lineno ::= TOK_FUNCTION */
      case 218: /* ph_get_line ::= */ yytestcase(yyruleno==218);
      case 247: /* rh_get_line ::= */ yytestcase(yyruleno==247);
#line 1222 "expparse.y"
{
    yygotominor.yy507 = SCANtell();
}
#line 3240 "expparse.c"
        break;
      case 130: /* fh_push_scope ::= TOK_IDENTIFIER */
#line 1227 "expparse.y"
{
    Function f = ALGcreate(OBJ_FUNCTION);
    tag_count = 0;
    if (print_objects_while_running & OBJ_FUNCTION_BITS) {
        fprintf( stderr, "parse: %s (function)\n", yymsp[0].minor.yy0.symbol->name);
    }
    PUSH_SCOPE(f, yymsp[0].minor.yy0.symbol, OBJ_FUNCTION);
}
#line 3252 "expparse.c"
        break;
      case 131: /* fh_plist ::= formal_parameter_list */
#line 1237 "expparse.y"
{
    Function f = CURRENT_SCOPE;
    f->u.func->parameters = yymsp[0].minor.yy371;
    f->u.func->pcount = LISTget_length(yymsp[0].minor.yy371);
    f->u.func->tag_count = tag_count;
    tag_count = -1;     /* done with parameters, no new tags can be defined */
}
#line 3263 "expparse.c"
        break;
      case 132: /* function_id ::= TOK_IDENTIFIER */
      case 219: /* procedure_id ::= TOK_IDENTIFIER */ yytestcase(yyruleno==219);
      case 220: /* procedure_id ::= TOK_BUILTIN_PROCEDURE */ yytestcase(yyruleno==220);
#line 1246 "expparse.y"
{
    yygotominor.yy275 = yymsp[0].minor.yy0.symbol;
}
#line 3272 "expparse.c"
        break;
      case 133: /* function_id ::= TOK_BUILTIN_FUNCTION */
#line 1250 "expparse.y"
{
    yygotominor.yy275 = yymsp[0].minor.yy0.symbol;

}
#line 3280 "expparse.c"
        break;
      case 134: /* conformant_aggregation ::= aggregate_type */
#line 1256 "expparse.y"
{
    yygotominor.yy477 = yymsp[0].minor.yy477;

}
#line 3288 "expparse.c"
        break;
      case 135: /* conformant_aggregation ::= TOK_ARRAY TOK_OF optional_or_unique parameter_type */
#line 1262 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(array_);
    yygotominor.yy477->flags.optional = yymsp[-1].minor.yy252.optional;
    yygotominor.yy477->flags.unique = yymsp[-1].minor.yy252.unique;
    yygotominor.yy477->base = yymsp[0].minor.yy297;
}
#line 3298 "expparse.c"
        break;
      case 136: /* conformant_aggregation ::= TOK_ARRAY bound_spec TOK_OF optional_or_unique parameter_type */
#line 1270 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(array_);
    yygotominor.yy477->flags.optional = yymsp[-1].minor.yy252.optional;
    yygotominor.yy477->flags.unique = yymsp[-1].minor.yy252.unique;
    yygotominor.yy477->base = yymsp[0].minor.yy297;
    yygotominor.yy477->upper = yymsp[-3].minor.yy253.upper_limit;
    yygotominor.yy477->lower = yymsp[-3].minor.yy253.lower_limit;
}
#line 3310 "expparse.c"
        break;
      case 137: /* conformant_aggregation ::= TOK_BAG TOK_OF parameter_type */
#line 1279 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(bag_);
    yygotominor.yy477->base = yymsp[0].minor.yy297;

}
#line 3319 "expparse.c"
        break;
      case 139: /* conformant_aggregation ::= TOK_LIST TOK_OF unique parameter_type */
#line 1292 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(list_);
    yygotominor.yy477->flags.unique = yymsp[-1].minor.yy252.unique;
    yygotominor.yy477->base = yymsp[0].minor.yy297;

}
#line 3329 "expparse.c"
        break;
      case 140: /* conformant_aggregation ::= TOK_LIST bound_spec TOK_OF unique parameter_type */
#line 1300 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(list_);
    yygotominor.yy477->base = yymsp[0].minor.yy297;
    yygotominor.yy477->flags.unique = yymsp[-1].minor.yy252.unique;
    yygotominor.yy477->upper = yymsp[-3].minor.yy253.upper_limit;
    yygotominor.yy477->lower = yymsp[-3].minor.yy253.lower_limit;
}
#line 3340 "expparse.c"
        break;
      case 141: /* conformant_aggregation ::= TOK_SET TOK_OF parameter_type */
      case 256: /* set_type ::= TOK_SET TOK_OF attribute_type */ yytestcase(yyruleno==256);
#line 1308 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(set_);
    yygotominor.yy477->base = yymsp[0].minor.yy297;
}
#line 3349 "expparse.c"
        break;
      case 142: /* conformant_aggregation ::= TOK_SET bound_spec TOK_OF parameter_type */
#line 1313 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(set_);
    yygotominor.yy477->base = yymsp[0].minor.yy297;
    yygotominor.yy477->upper = yymsp[-2].minor.yy253.upper_limit;
    yygotominor.yy477->lower = yymsp[-2].minor.yy253.lower_limit;
}
#line 3359 "expparse.c"
        break;
      case 143: /* generic_type ::= TOK_GENERIC */
#line 1321 "expparse.y"
{
    yygotominor.yy297 = Type_Generic;

    if (tag_count < 0) {
        Symbol sym;
        sym.line = yylineno;
        sym.filename = current_filename;
        ERRORreport_with_symbol(ERROR_unlabelled_param_type, &sym,
        CURRENT_SCOPE_NAME);
    }
}
#line 3374 "expparse.c"
        break;
      case 144: /* generic_type ::= TOK_GENERIC TOK_COLON TOK_IDENTIFIER */
#line 1333 "expparse.y"
{
    TypeBody g = TYPEBODYcreate(generic_);
    yygotominor.yy297 = TYPEcreate_from_body_anonymously(g);

    SCOPEadd_super(yygotominor.yy297);

    g->tag = TYPEcreate_user_defined_tag(yygotominor.yy297, CURRENT_SCOPE, yymsp[0].minor.yy0.symbol);
    if (g->tag) {
        SCOPEadd_super(g->tag);
    }
}
#line 3389 "expparse.c"
        break;
      case 145: /* id_list ::= TOK_IDENTIFIER */
#line 1346 "expparse.y"
{
    yygotominor.yy371 = LISTcreate();
    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy0.symbol);

}
#line 3398 "expparse.c"
        break;
      case 146: /* id_list ::= id_list TOK_COMMA TOK_IDENTIFIER */
#line 1352 "expparse.y"
{
    yyerrok;

    yygotominor.yy371 = yymsp[-2].minor.yy371;
    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy0.symbol);
}
#line 3408 "expparse.c"
        break;
      case 147: /* identifier ::= TOK_SELF */
#line 1360 "expparse.y"
{
    yygotominor.yy401 = EXPcreate(Type_Self);
}
#line 3415 "expparse.c"
        break;
      case 148: /* identifier ::= TOK_QUESTION_MARK */
#line 1364 "expparse.y"
{
    yygotominor.yy401 = LITERAL_INFINITY;
}
#line 3422 "expparse.c"
        break;
      case 149: /* identifier ::= TOK_IDENTIFIER */
#line 1368 "expparse.y"
{
    yygotominor.yy401 = EXPcreate(Type_Identifier);
    yygotominor.yy401->symbol = *(yymsp[0].minor.yy0.symbol);
    SYMBOL_destroy(yymsp[0].minor.yy0.symbol);
}
#line 3431 "expparse.c"
        break;
      case 150: /* if_statement ::= TOK_IF expression TOK_THEN statement_rep TOK_END_IF semicolon */
#line 1376 "expparse.y"
{
    yygotominor.yy332 = CONDcreate(yymsp[-4].minor.yy401, yymsp[-2].minor.yy371, STATEMENT_LIST_NULL);
}
#line 3438 "expparse.c"
        break;
      case 151: /* if_statement ::= TOK_IF expression TOK_THEN statement_rep TOK_ELSE statement_rep TOK_END_IF semicolon */
#line 1381 "expparse.y"
{
    yygotominor.yy332 = CONDcreate(yymsp[-6].minor.yy401, yymsp[-4].minor.yy371, yymsp[-2].minor.yy371);
}
#line 3445 "expparse.c"
        break;
      case 152: /* include_directive ::= TOK_INCLUDE TOK_STRING_LITERAL semicolon */
#line 1386 "expparse.y"
{
    SCANinclude_file(yymsp[-1].minor.yy0.string);
}
#line 3452 "expparse.c"
        break;
      case 153: /* increment_control ::= TOK_IDENTIFIER TOK_ASSIGNMENT expression TOK_TO expression by_expression */
#line 1392 "expparse.y"
{
    Increment i = INCR_CTLcreate(yymsp[-5].minor.yy0.symbol, yymsp[-3].minor.yy401, yymsp[-1].minor.yy401, yymsp[0].minor.yy401);

    /* scope doesn't really have/need a name, I suppose */
    /* naming it by the iterator variable is fine */

    PUSH_SCOPE(i, (Symbol *)0, OBJ_INCREMENT);
}
#line 3464 "expparse.c"
        break;
      case 155: /* rename ::= TOK_IDENTIFIER */
#line 1410 "expparse.y"
{
    (*interface_func)(CURRENT_SCOPE, interface_schema, yymsp[0].minor.yy0, yymsp[0].minor.yy0);
}
#line 3471 "expparse.c"
        break;
      case 156: /* rename ::= TOK_IDENTIFIER TOK_AS TOK_IDENTIFIER */
#line 1414 "expparse.y"
{
    (*interface_func)(CURRENT_SCOPE, interface_schema, yymsp[-2].minor.yy0, yymsp[0].minor.yy0);
}
#line 3478 "expparse.c"
        break;
      case 158: /* rename_list ::= rename_list TOK_COMMA rename */
      case 161: /* reference_clause ::= reference_head parened_rename_list semicolon */ yytestcase(yyruleno==161);
      case 164: /* use_clause ::= use_head parened_rename_list semicolon */ yytestcase(yyruleno==164);
      case 249: /* schema_body ::= interface_specification_list constant_decl block_list */ yytestcase(yyruleno==249);
      case 295: /* type_decl ::= td_start TOK_END_TYPE semicolon */ yytestcase(yyruleno==295);
#line 1423 "expparse.y"
{
    yygotominor.yy0 = yymsp[-2].minor.yy0;
}
#line 3489 "expparse.c"
        break;
      case 160: /* reference_clause ::= TOK_REFERENCE TOK_FROM TOK_IDENTIFIER semicolon */
#line 1433 "expparse.y"
{
    if (!CURRENT_SCHEMA->ref_schemas) {
        CURRENT_SCHEMA->ref_schemas = LISTcreate();
    }

    LISTadd_last(CURRENT_SCHEMA->ref_schemas, (Generic)yymsp[-1].minor.yy0.symbol);
}
#line 3500 "expparse.c"
        break;
      case 162: /* reference_head ::= TOK_REFERENCE TOK_FROM TOK_IDENTIFIER */
#line 1446 "expparse.y"
{
    interface_schema = yymsp[0].minor.yy0.symbol;
    interface_func = SCHEMAadd_reference;
}
#line 3508 "expparse.c"
        break;
      case 163: /* use_clause ::= TOK_USE TOK_FROM TOK_IDENTIFIER semicolon */
#line 1452 "expparse.y"
{
    if (!CURRENT_SCHEMA->use_schemas) {
        CURRENT_SCHEMA->use_schemas = LISTcreate();
    }

    LISTadd_last(CURRENT_SCHEMA->use_schemas, (Generic)yymsp[-1].minor.yy0.symbol);
}
#line 3519 "expparse.c"
        break;
      case 165: /* use_head ::= TOK_USE TOK_FROM TOK_IDENTIFIER */
#line 1465 "expparse.y"
{
    interface_schema = yymsp[0].minor.yy0.symbol;
    interface_func = SCHEMAadd_use;
}
#line 3527 "expparse.c"
        break;
      case 170: /* interval ::= TOK_LEFT_CURL simple_expression rel_op simple_expression rel_op simple_expression right_curl */
#line 1488 "expparse.y"
{
    Expression    tmp1, tmp2;

    yygotominor.yy401 = (Expression)0;
    tmp1 = BIN_EXPcreate(yymsp[-4].minor.yy126, yymsp[-5].minor.yy401, yymsp[-3].minor.yy401);
    tmp2 = BIN_EXPcreate(yymsp[-2].minor.yy126, yymsp[-3].minor.yy401, yymsp[-1].minor.yy401);
    yygotominor.yy401 = BIN_EXPcreate(OP_AND, tmp1, tmp2);
}
#line 3539 "expparse.c"
        break;
      case 171: /* set_or_bag_of_entity ::= defined_type */
      case 289: /* type ::= defined_type */ yytestcase(yyruleno==289);
#line 1500 "expparse.y"
{
    yygotominor.yy378.type = yymsp[0].minor.yy297;
    yygotominor.yy378.body = 0;
}
#line 3548 "expparse.c"
        break;
      case 172: /* set_or_bag_of_entity ::= TOK_SET TOK_OF defined_type */
#line 1505 "expparse.y"
{
    yygotominor.yy378.type = 0;
    yygotominor.yy378.body = TYPEBODYcreate(set_);
    yygotominor.yy378.body->base = yymsp[0].minor.yy297;

}
#line 3558 "expparse.c"
        break;
      case 173: /* set_or_bag_of_entity ::= TOK_SET bound_spec TOK_OF defined_type */
#line 1512 "expparse.y"
{
    yygotominor.yy378.type = 0; 
    yygotominor.yy378.body = TYPEBODYcreate(set_);
    yygotominor.yy378.body->base = yymsp[0].minor.yy297;
    yygotominor.yy378.body->upper = yymsp[-2].minor.yy253.upper_limit;
    yygotominor.yy378.body->lower = yymsp[-2].minor.yy253.lower_limit;
}
#line 3569 "expparse.c"
        break;
      case 174: /* set_or_bag_of_entity ::= TOK_BAG bound_spec TOK_OF defined_type */
#line 1520 "expparse.y"
{
    yygotominor.yy378.type = 0;
    yygotominor.yy378.body = TYPEBODYcreate(bag_);
    yygotominor.yy378.body->base = yymsp[0].minor.yy297;
    yygotominor.yy378.body->upper = yymsp[-2].minor.yy253.upper_limit;
    yygotominor.yy378.body->lower = yymsp[-2].minor.yy253.lower_limit;
}
#line 3580 "expparse.c"
        break;
      case 175: /* set_or_bag_of_entity ::= TOK_BAG TOK_OF defined_type */
#line 1528 "expparse.y"
{
    yygotominor.yy378.type = 0;
    yygotominor.yy378.body = TYPEBODYcreate(bag_);
    yygotominor.yy378.body->base = yymsp[0].minor.yy297;
}
#line 3589 "expparse.c"
        break;
      case 178: /* inverse_attr ::= attribute_decl TOK_COLON set_or_bag_of_entity TOK_FOR TOK_IDENTIFIER semicolon */
#line 1555 "expparse.y"
{
    if (yymsp[-3].minor.yy378.type) {
        yygotominor.yy91 = VARcreate(yymsp[-5].minor.yy401, yymsp[-3].minor.yy378.type);
    } else {
        Type t = TYPEcreate_from_body_anonymously(yymsp[-3].minor.yy378.body);
        SCOPEadd_super(t);
        yygotominor.yy91 = VARcreate(yymsp[-5].minor.yy401, t);
    }

    yygotominor.yy91->flags.attribute = true;
    yygotominor.yy91->inverse_symbol = yymsp[-1].minor.yy0.symbol;
}
#line 3605 "expparse.c"
        break;
      case 182: /* list_type ::= TOK_LIST bound_spec TOK_OF unique attribute_type */
#line 1589 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(list_);
    yygotominor.yy477->base = yymsp[0].minor.yy297;
    yygotominor.yy477->flags.unique = yymsp[-1].minor.yy252.unique;
    yygotominor.yy477->lower = yymsp[-3].minor.yy253.lower_limit;
    yygotominor.yy477->upper = yymsp[-3].minor.yy253.upper_limit;
}
#line 3616 "expparse.c"
        break;
      case 183: /* list_type ::= TOK_LIST TOK_OF unique attribute_type */
#line 1597 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(list_);
    yygotominor.yy477->base = yymsp[0].minor.yy297;
    yygotominor.yy477->flags.unique = yymsp[-1].minor.yy252.unique;
}
#line 3625 "expparse.c"
        break;
      case 184: /* literal ::= TOK_INTEGER_LITERAL */
#line 1604 "expparse.y"
{
    if (yymsp[0].minor.yy0.iVal == 0) {
        yygotominor.yy401 = LITERAL_ZERO;
    } else if (yymsp[0].minor.yy0.iVal == 1) {
    yygotominor.yy401 = LITERAL_ONE;
    } else {
    yygotominor.yy401 = EXPcreate_simple(Type_Integer);
    yygotominor.yy401->u.integer = (int)yymsp[0].minor.yy0.iVal;
    resolved_all(yygotominor.yy401);
    }
}
#line 3640 "expparse.c"
        break;
      case 185: /* literal ::= TOK_REAL_LITERAL */
#line 1616 "expparse.y"
{
    /* if rVal (a double) is nonzero and has magnitude <= the smallest non-denormal float, print a warning */
    if( ( fabs( yymsp[0].minor.yy0.rVal ) <= FLT_MIN ) && ( fabs( yymsp[0].minor.yy0.rVal ) > 0 ) ) {
        Symbol sym;
        sym.line = yylineno;
        sym.filename = current_filename;
        ERRORreport_with_symbol(ERROR_warn_small_real, &sym, yymsp[0].minor.yy0.rVal );
    }
    if( fabs( yymsp[0].minor.yy0.rVal ) < DBL_MIN ) {
        yygotominor.yy401 = LITERAL_ZERO;
    } else {
        yygotominor.yy401 = EXPcreate_simple(Type_Real);
        yygotominor.yy401->u.real = yymsp[0].minor.yy0.rVal;
        resolved_all(yygotominor.yy401);
    }
}
#line 3660 "expparse.c"
        break;
      case 186: /* literal ::= TOK_STRING_LITERAL */
#line 1633 "expparse.y"
{
    yygotominor.yy401 = EXPcreate_simple(Type_String);
    yygotominor.yy401->symbol.name = yymsp[0].minor.yy0.string;
    resolved_all(yygotominor.yy401);
}
#line 3669 "expparse.c"
        break;
      case 187: /* literal ::= TOK_STRING_LITERAL_ENCODED */
#line 1639 "expparse.y"
{
    yygotominor.yy401 = EXPcreate_simple(Type_String_Encoded);
    yygotominor.yy401->symbol.name = yymsp[0].minor.yy0.string;
    resolved_all(yygotominor.yy401);
}
#line 3678 "expparse.c"
        break;
      case 188: /* literal ::= TOK_LOGICAL_LITERAL */
#line 1645 "expparse.y"
{
    yygotominor.yy401 = EXPcreate_simple(Type_Logical);
    yygotominor.yy401->u.logical = yymsp[0].minor.yy0.logical;
    resolved_all(yygotominor.yy401);
}
#line 3687 "expparse.c"
        break;
      case 189: /* literal ::= TOK_BINARY_LITERAL */
#line 1651 "expparse.y"
{
    yygotominor.yy401 = EXPcreate_simple(Type_Binary);
    yygotominor.yy401->symbol.name = yymsp[0].minor.yy0.binary;
    resolved_all(yygotominor.yy401);
}
#line 3696 "expparse.c"
        break;
      case 192: /* local_variable ::= id_list TOK_COLON parameter_type semicolon */
#line 1667 "expparse.y"
{
    Expression e;
    Variable v;
    LISTdo(yymsp[-3].minor.yy371, sym, Symbol *)

    /* convert symbol to name-expression */

    e = EXPcreate(Type_Attribute);
    e->symbol = *sym; SYMBOL_destroy(sym);
    v = VARcreate(e, yymsp[-1].minor.yy297);
    v->offset = local_var_count++;
    DICTdefine(CURRENT_SCOPE->symbol_table, e->symbol.name, (Generic)v, &e->symbol, OBJ_VARIABLE);
    LISTod;
    LISTfree(yymsp[-3].minor.yy371);
}
#line 3715 "expparse.c"
        break;
      case 193: /* local_variable ::= id_list TOK_COLON parameter_type local_initializer semicolon */
#line 1684 "expparse.y"
{
    Expression e;
    Variable v;
    LISTdo(yymsp[-4].minor.yy371, sym, Symbol *)
    e = EXPcreate(Type_Attribute);
    e->symbol = *sym; SYMBOL_destroy(sym);
    v = VARcreate(e, yymsp[-2].minor.yy297);
    v->offset = local_var_count++;
    v->initializer = yymsp[-1].minor.yy401;
    DICTdefine(CURRENT_SCOPE->symbol_table, e->symbol.name, (Generic)v,
    &e->symbol, OBJ_VARIABLE);
    LISTod;
    LISTfree(yymsp[-4].minor.yy371);
}
#line 3733 "expparse.c"
        break;
      case 197: /* local_decl_rules_on ::= */
#line 1708 "expparse.y"
{
    tag_count = 0; /* don't signal an error if we find a generic_type */
    local_var_count = 0; /* used to keep local var decl's in the same order */
}
#line 3741 "expparse.c"
        break;
      case 198: /* local_decl_rules_off ::= */
#line 1714 "expparse.y"
{
    tag_count = -1; /* signal an error if we find a generic_type */
}
#line 3748 "expparse.c"
        break;
      case 199: /* defined_type ::= TOK_IDENTIFIER */
#line 1719 "expparse.y"
{
    yygotominor.yy297 = TYPEcreate_name(yymsp[0].minor.yy0.symbol);
    SCOPEadd_super(yygotominor.yy297);
    SYMBOL_destroy(yymsp[0].minor.yy0.symbol);
}
#line 3757 "expparse.c"
        break;
      case 200: /* defined_type_list ::= defined_type */
#line 1726 "expparse.y"
{
    yygotominor.yy371 = LISTcreate();
    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy297);

}
#line 3766 "expparse.c"
        break;
      case 201: /* defined_type_list ::= defined_type_list TOK_COMMA defined_type */
#line 1732 "expparse.y"
{
    yygotominor.yy371 = yymsp[-2].minor.yy371;
    LISTadd_last(yygotominor.yy371,
    (Generic)yymsp[0].minor.yy297);
}
#line 3775 "expparse.c"
        break;
      case 204: /* optional_or_unique ::= */
#line 1749 "expparse.y"
{
    yygotominor.yy252.unique = 0;
    yygotominor.yy252.optional = 0;
}
#line 3783 "expparse.c"
        break;
      case 205: /* optional_or_unique ::= TOK_OPTIONAL */
#line 1754 "expparse.y"
{
    yygotominor.yy252.unique = 0;
    yygotominor.yy252.optional = 1;
}
#line 3791 "expparse.c"
        break;
      case 206: /* optional_or_unique ::= TOK_UNIQUE */
#line 1759 "expparse.y"
{
    yygotominor.yy252.unique = 1;
    yygotominor.yy252.optional = 0;
}
#line 3799 "expparse.c"
        break;
      case 207: /* optional_or_unique ::= TOK_OPTIONAL TOK_UNIQUE */
      case 208: /* optional_or_unique ::= TOK_UNIQUE TOK_OPTIONAL */ yytestcase(yyruleno==208);
#line 1764 "expparse.y"
{
    yygotominor.yy252.unique = 1;
    yygotominor.yy252.optional = 1;
}
#line 3808 "expparse.c"
        break;
      case 209: /* optional_fixed ::= */
#line 1775 "expparse.y"
{
    yygotominor.yy252.fixed = 0;
}
#line 3815 "expparse.c"
        break;
      case 210: /* optional_fixed ::= TOK_FIXED */
#line 1779 "expparse.y"
{
    yygotominor.yy252.fixed = 1;
}
#line 3822 "expparse.c"
        break;
      case 211: /* precision_spec ::= */
#line 1784 "expparse.y"
{
    yygotominor.yy401 = (Expression)0;
}
#line 3829 "expparse.c"
        break;
      case 212: /* precision_spec ::= TOK_LEFT_PAREN expression TOK_RIGHT_PAREN */
      case 304: /* unary_expression ::= TOK_LEFT_PAREN expression TOK_RIGHT_PAREN */ yytestcase(yyruleno==304);
#line 1788 "expparse.y"
{
    yygotominor.yy401 = yymsp[-1].minor.yy401;
}
#line 3837 "expparse.c"
        break;
      case 213: /* proc_call_statement ::= procedure_id actual_parameters semicolon */
#line 1798 "expparse.y"
{
    yygotominor.yy332 = PCALLcreate(yymsp[-1].minor.yy371);
    yygotominor.yy332->symbol = *(yymsp[-2].minor.yy275);
}
#line 3845 "expparse.c"
        break;
      case 214: /* proc_call_statement ::= procedure_id semicolon */
#line 1803 "expparse.y"
{
    yygotominor.yy332 = PCALLcreate((Linked_List)0);
    yygotominor.yy332->symbol = *(yymsp[-1].minor.yy275);
}
#line 3853 "expparse.c"
        break;
      case 215: /* procedure_decl ::= procedure_header action_body TOK_END_PROCEDURE semicolon */
#line 1810 "expparse.y"
{
    PROCput_body(CURRENT_SCOPE, yymsp[-2].minor.yy371);
    ALGput_full_text(CURRENT_SCOPE, yymsp[-3].minor.yy507, SCANtell());
    POP_SCOPE();
}
#line 3862 "expparse.c"
        break;
      case 216: /* procedure_header ::= TOK_PROCEDURE ph_get_line ph_push_scope formal_parameter_list semicolon */
#line 1818 "expparse.y"
{
    Procedure p = CURRENT_SCOPE;
    p->u.proc->parameters = yymsp[-1].minor.yy371;
    p->u.proc->pcount = LISTget_length(yymsp[-1].minor.yy371);
    p->u.proc->tag_count = tag_count;
    tag_count = -1;    /* done with parameters, no new tags can be defined */
    yygotominor.yy507 = yymsp[-3].minor.yy507;
}
#line 3874 "expparse.c"
        break;
      case 217: /* ph_push_scope ::= TOK_IDENTIFIER */
#line 1828 "expparse.y"
{
    Procedure p = ALGcreate(OBJ_PROCEDURE);
    tag_count = 0;

    if (print_objects_while_running & OBJ_PROCEDURE_BITS) {
    fprintf( stderr, "parse: %s (procedure)\n", yymsp[0].minor.yy0.symbol->name);
    }

    PUSH_SCOPE(p, yymsp[0].minor.yy0.symbol, OBJ_PROCEDURE);
}
#line 3888 "expparse.c"
        break;
      case 221: /* group_ref ::= TOK_BACKSLASH TOK_IDENTIFIER */
#line 1854 "expparse.y"
{
    yygotominor.yy401 = BIN_EXPcreate(OP_GROUP, (Expression)0, (Expression)0);
    yygotominor.yy401->e.op2 = EXPcreate(Type_Identifier);
    yygotominor.yy401->e.op2->symbol = *yymsp[0].minor.yy0.symbol;
    SYMBOL_destroy(yymsp[0].minor.yy0.symbol);
}
#line 3898 "expparse.c"
        break;
      case 222: /* qualifier ::= TOK_DOT TOK_IDENTIFIER */
#line 1862 "expparse.y"
{
    yygotominor.yy46.expr = yygotominor.yy46.first = BIN_EXPcreate(OP_DOT, (Expression)0, (Expression)0);
    yygotominor.yy46.expr->e.op2 = EXPcreate(Type_Identifier);
    yygotominor.yy46.expr->e.op2->symbol = *yymsp[0].minor.yy0.symbol;
    SYMBOL_destroy(yymsp[0].minor.yy0.symbol);
}
#line 3908 "expparse.c"
        break;
      case 223: /* qualifier ::= TOK_BACKSLASH TOK_IDENTIFIER */
#line 1869 "expparse.y"
{
    yygotominor.yy46.expr = yygotominor.yy46.first = BIN_EXPcreate(OP_GROUP, (Expression)0, (Expression)0);
    yygotominor.yy46.expr->e.op2 = EXPcreate(Type_Identifier);
    yygotominor.yy46.expr->e.op2->symbol = *yymsp[0].minor.yy0.symbol;
    SYMBOL_destroy(yymsp[0].minor.yy0.symbol);
}
#line 3918 "expparse.c"
        break;
      case 224: /* qualifier ::= TOK_LEFT_BRACKET simple_expression TOK_RIGHT_BRACKET */
#line 1878 "expparse.y"
{
    yygotominor.yy46.expr = yygotominor.yy46.first = BIN_EXPcreate(OP_ARRAY_ELEMENT, (Expression)0,
    (Expression)0);
    yygotominor.yy46.expr->e.op2 = yymsp[-1].minor.yy401;
}
#line 3927 "expparse.c"
        break;
      case 225: /* qualifier ::= TOK_LEFT_BRACKET simple_expression TOK_COLON simple_expression TOK_RIGHT_BRACKET */
#line 1887 "expparse.y"
{
    yygotominor.yy46.expr = yygotominor.yy46.first = TERN_EXPcreate(OP_SUBCOMPONENT, (Expression)0,
    (Expression)0, (Expression)0);
    yygotominor.yy46.expr->e.op2 = yymsp[-3].minor.yy401;
    yygotominor.yy46.expr->e.op3 = yymsp[-1].minor.yy401;
}
#line 3937 "expparse.c"
        break;
      case 226: /* query_expression ::= query_start expression TOK_RIGHT_PAREN */
#line 1895 "expparse.y"
{
    yygotominor.yy401 = yymsp[-2].minor.yy401;
    yygotominor.yy401->u.query->expression = yymsp[-1].minor.yy401;
    POP_SCOPE();
}
#line 3946 "expparse.c"
        break;
      case 227: /* query_start ::= TOK_QUERY TOK_LEFT_PAREN TOK_IDENTIFIER TOK_ALL_IN expression TOK_SUCH_THAT */
#line 1903 "expparse.y"
{
    yygotominor.yy401 = QUERYcreate(yymsp[-3].minor.yy0.symbol, yymsp[-1].minor.yy401);
    SYMBOL_destroy(yymsp[-3].minor.yy0.symbol);
    PUSH_SCOPE(yygotominor.yy401->u.query->scope, (Symbol *)0, OBJ_QUERY);
}
#line 3955 "expparse.c"
        break;
      case 228: /* rel_op ::= TOK_LESS_THAN */
#line 1910 "expparse.y"
{
    yygotominor.yy126 = OP_LESS_THAN;
}
#line 3962 "expparse.c"
        break;
      case 229: /* rel_op ::= TOK_GREATER_THAN */
#line 1914 "expparse.y"
{
    yygotominor.yy126 = OP_GREATER_THAN;
}
#line 3969 "expparse.c"
        break;
      case 230: /* rel_op ::= TOK_EQUAL */
#line 1918 "expparse.y"
{
    yygotominor.yy126 = OP_EQUAL;
}
#line 3976 "expparse.c"
        break;
      case 231: /* rel_op ::= TOK_LESS_EQUAL */
#line 1922 "expparse.y"
{
    yygotominor.yy126 = OP_LESS_EQUAL;
}
#line 3983 "expparse.c"
        break;
      case 232: /* rel_op ::= TOK_GREATER_EQUAL */
#line 1926 "expparse.y"
{
    yygotominor.yy126 = OP_GREATER_EQUAL;
}
#line 3990 "expparse.c"
        break;
      case 233: /* rel_op ::= TOK_NOT_EQUAL */
#line 1930 "expparse.y"
{
    yygotominor.yy126 = OP_NOT_EQUAL;
}
#line 3997 "expparse.c"
        break;
      case 234: /* rel_op ::= TOK_INST_EQUAL */
#line 1934 "expparse.y"
{
    yygotominor.yy126 = OP_INST_EQUAL;
}
#line 4004 "expparse.c"
        break;
      case 235: /* rel_op ::= TOK_INST_NOT_EQUAL */
#line 1938 "expparse.y"
{
    yygotominor.yy126 = OP_INST_NOT_EQUAL;
}
#line 4011 "expparse.c"
        break;
      case 236: /* repeat_statement ::= TOK_REPEAT increment_control while_control until_control semicolon statement_rep TOK_END_REPEAT semicolon */
#line 1946 "expparse.y"
{
    yygotominor.yy332 = LOOPcreate(CURRENT_SCOPE, yymsp[-5].minor.yy401, yymsp[-4].minor.yy401, yymsp[-2].minor.yy371);

    /* matching PUSH_SCOPE is in increment_control */
    POP_SCOPE();
}
#line 4021 "expparse.c"
        break;
      case 237: /* repeat_statement ::= TOK_REPEAT while_control until_control semicolon statement_rep TOK_END_REPEAT semicolon */
#line 1954 "expparse.y"
{
    yygotominor.yy332 = LOOPcreate((struct Scope_ *)0, yymsp[-5].minor.yy401, yymsp[-4].minor.yy401, yymsp[-2].minor.yy371);
}
#line 4028 "expparse.c"
        break;
      case 238: /* return_statement ::= TOK_RETURN semicolon */
#line 1959 "expparse.y"
{
    yygotominor.yy332 = RETcreate((Expression)0);
}
#line 4035 "expparse.c"
        break;
      case 239: /* return_statement ::= TOK_RETURN TOK_LEFT_PAREN expression TOK_RIGHT_PAREN semicolon */
#line 1964 "expparse.y"
{
    yygotominor.yy332 = RETcreate(yymsp[-2].minor.yy401);
}
#line 4042 "expparse.c"
        break;
      case 241: /* rule_decl ::= rule_header action_body where_rule TOK_END_RULE semicolon */
#line 1975 "expparse.y"
{
    RULEput_body(CURRENT_SCOPE, yymsp[-3].minor.yy371);
    RULEput_where(CURRENT_SCOPE, yymsp[-2].minor.yy371);
    ALGput_full_text(CURRENT_SCOPE, yymsp[-4].minor.yy507, SCANtell());
    POP_SCOPE();
}
#line 4052 "expparse.c"
        break;
      case 242: /* rule_formal_parameter ::= TOK_IDENTIFIER */
#line 1983 "expparse.y"
{
    Expression e;
    Type t;

    /* it's true that we know it will be an entity_ type later */
    TypeBody tb = TYPEBODYcreate(set_);
    tb->base = TYPEcreate_name(yymsp[0].minor.yy0.symbol);
    SCOPEadd_super(tb->base);
    t = TYPEcreate_from_body_anonymously(tb);
    SCOPEadd_super(t);
    e = EXPcreate_from_symbol(t, yymsp[0].minor.yy0.symbol);
    yygotominor.yy91 = VARcreate(e, t);
    yygotominor.yy91->flags.attribute = true;
    yygotominor.yy91->flags.parameter = true;

    /* link it in to the current scope's dict */
    DICTdefine(CURRENT_SCOPE->symbol_table, yymsp[0].minor.yy0.symbol->name, (Generic)yygotominor.yy91,
    yymsp[0].minor.yy0.symbol, OBJ_VARIABLE);
}
#line 4075 "expparse.c"
        break;
      case 243: /* rule_formal_parameter_list ::= rule_formal_parameter */
#line 2004 "expparse.y"
{
    yygotominor.yy371 = LISTcreate();
    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy91); 
}
#line 4083 "expparse.c"
        break;
      case 244: /* rule_formal_parameter_list ::= rule_formal_parameter_list TOK_COMMA rule_formal_parameter */
#line 2010 "expparse.y"
{
    yygotominor.yy371 = yymsp[-2].minor.yy371;
    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy91);
}
#line 4091 "expparse.c"
        break;
      case 245: /* rule_header ::= rh_start rule_formal_parameter_list TOK_RIGHT_PAREN semicolon */
#line 2017 "expparse.y"
{
    CURRENT_SCOPE->u.rule->parameters = yymsp[-2].minor.yy371;

    yygotominor.yy507 = yymsp[-3].minor.yy507;
}
#line 4100 "expparse.c"
        break;
      case 246: /* rh_start ::= TOK_RULE rh_get_line TOK_IDENTIFIER TOK_FOR TOK_LEFT_PAREN */
#line 2025 "expparse.y"
{
    Rule r = ALGcreate(OBJ_RULE);

    if (print_objects_while_running & OBJ_RULE_BITS) {
    fprintf( stderr, "parse: %s (rule)\n", yymsp[-2].minor.yy0.symbol->name);
    }

    PUSH_SCOPE(r, yymsp[-2].minor.yy0.symbol, OBJ_RULE);

    yygotominor.yy507 = yymsp[-3].minor.yy507;
}
#line 4115 "expparse.c"
        break;
      case 250: /* schema_decl ::= schema_header schema_body TOK_END_SCHEMA semicolon */
#line 2052 "expparse.y"
{
    POP_SCOPE();
}
#line 4122 "expparse.c"
        break;
      case 252: /* schema_header ::= TOK_SCHEMA TOK_IDENTIFIER semicolon */
#line 2061 "expparse.y"
{
    Schema schema = ( Schema ) DICTlookup(CURRENT_SCOPE->symbol_table, yymsp[-1].minor.yy0.symbol->name);

    if (print_objects_while_running & OBJ_SCHEMA_BITS) {
    fprintf( stderr, "parse: %s (schema)\n", yymsp[-1].minor.yy0.symbol->name);
    }

    if (EXPRESSignore_duplicate_schemas && schema) {
    SCANskip_to_end_schema(parseData.scanner);
    PUSH_SCOPE_DUMMY();
    } else {
    schema = SCHEMAcreate();
    LISTadd_last(PARSEnew_schemas, (Generic)schema);
    PUSH_SCOPE(schema, yymsp[-1].minor.yy0.symbol, OBJ_SCHEMA);
    }
}
#line 4142 "expparse.c"
        break;
      case 253: /* select_type ::= TOK_SELECT TOK_LEFT_PAREN defined_type_list TOK_RIGHT_PAREN */
#line 2080 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(select_);
    yygotominor.yy477->list = yymsp[-1].minor.yy371;
}
#line 4150 "expparse.c"
        break;
      case 255: /* set_type ::= TOK_SET bound_spec TOK_OF attribute_type */
#line 2091 "expparse.y"
{
    yygotominor.yy477 = TYPEBODYcreate(set_);
    yygotominor.yy477->base = yymsp[0].minor.yy297;
    yygotominor.yy477->lower = yymsp[-2].minor.yy253.lower_limit;
    yygotominor.yy477->upper = yymsp[-2].minor.yy253.upper_limit;
}
#line 4160 "expparse.c"
        break;
      case 257: /* skip_statement ::= TOK_SKIP semicolon */
#line 2104 "expparse.y"
{
    yygotominor.yy332 = STATEMENT_SKIP;
}
#line 4167 "expparse.c"
        break;
      case 258: /* statement ::= alias_statement */
      case 259: /* statement ::= assignment_statement */ yytestcase(yyruleno==259);
      case 260: /* statement ::= case_statement */ yytestcase(yyruleno==260);
      case 261: /* statement ::= compound_statement */ yytestcase(yyruleno==261);
      case 262: /* statement ::= escape_statement */ yytestcase(yyruleno==262);
      case 263: /* statement ::= if_statement */ yytestcase(yyruleno==263);
      case 264: /* statement ::= proc_call_statement */ yytestcase(yyruleno==264);
      case 265: /* statement ::= repeat_statement */ yytestcase(yyruleno==265);
      case 266: /* statement ::= return_statement */ yytestcase(yyruleno==266);
      case 267: /* statement ::= skip_statement */ yytestcase(yyruleno==267);
#line 2109 "expparse.y"
{
    yygotominor.yy332 = yymsp[0].minor.yy332;
}
#line 4183 "expparse.c"
        break;
      case 270: /* statement_rep ::= statement statement_rep */
#line 2158 "expparse.y"
{
    yygotominor.yy371 = yymsp[0].minor.yy371;
    LISTadd_first(yygotominor.yy371, (Generic)yymsp[-1].minor.yy332); 
}
#line 4191 "expparse.c"
        break;
      case 271: /* subsuper_decl ::= */
#line 2168 "expparse.y"
{
    yygotominor.yy242.subtypes = EXPRESSION_NULL;
    yygotominor.yy242.abstract = false;
    yygotominor.yy242.supertypes = LIST_NULL;
}
#line 4200 "expparse.c"
        break;
      case 272: /* subsuper_decl ::= supertype_decl */
#line 2174 "expparse.y"
{
    yygotominor.yy242.subtypes = yymsp[0].minor.yy385.subtypes;
    yygotominor.yy242.abstract = yymsp[0].minor.yy385.abstract;
    yygotominor.yy242.supertypes = LIST_NULL;
}
#line 4209 "expparse.c"
        break;
      case 273: /* subsuper_decl ::= subtype_decl */
#line 2180 "expparse.y"
{
    yygotominor.yy242.supertypes = yymsp[0].minor.yy371;
    yygotominor.yy242.abstract = false;
    yygotominor.yy242.subtypes = EXPRESSION_NULL;
}
#line 4218 "expparse.c"
        break;
      case 274: /* subsuper_decl ::= supertype_decl subtype_decl */
#line 2186 "expparse.y"
{
    yygotominor.yy242.subtypes = yymsp[-1].minor.yy385.subtypes;
    yygotominor.yy242.abstract = yymsp[-1].minor.yy385.abstract;
    yygotominor.yy242.supertypes = yymsp[0].minor.yy371;
}
#line 4227 "expparse.c"
        break;
      case 276: /* supertype_decl ::= TOK_ABSTRACT TOK_SUPERTYPE */
#line 2199 "expparse.y"
{
    yygotominor.yy385.subtypes = (Expression)0;
    yygotominor.yy385.abstract = true;
}
#line 4235 "expparse.c"
        break;
      case 277: /* supertype_decl ::= TOK_SUPERTYPE TOK_OF TOK_LEFT_PAREN supertype_expression TOK_RIGHT_PAREN */
#line 2205 "expparse.y"
{
    yygotominor.yy385.subtypes = yymsp[-1].minor.yy401;
    yygotominor.yy385.abstract = false;
}
#line 4243 "expparse.c"
        break;
      case 278: /* supertype_decl ::= TOK_ABSTRACT TOK_SUPERTYPE TOK_OF TOK_LEFT_PAREN supertype_expression TOK_RIGHT_PAREN */
#line 2211 "expparse.y"
{
    yygotominor.yy385.subtypes = yymsp[-1].minor.yy401;
    yygotominor.yy385.abstract = true;
}
#line 4251 "expparse.c"
        break;
      case 279: /* supertype_expression ::= supertype_factor */
#line 2217 "expparse.y"
{
    yygotominor.yy401 = yymsp[0].minor.yy385.subtypes;
}
#line 4258 "expparse.c"
        break;
      case 280: /* supertype_expression ::= supertype_expression TOK_AND supertype_factor */
#line 2221 "expparse.y"
{
    yygotominor.yy401 = BIN_EXPcreate(OP_AND, yymsp[-2].minor.yy401, yymsp[0].minor.yy385.subtypes);
}
#line 4265 "expparse.c"
        break;
      case 281: /* supertype_expression ::= supertype_expression TOK_ANDOR supertype_factor */
#line 2226 "expparse.y"
{
    yygotominor.yy401 = BIN_EXPcreate(OP_ANDOR, yymsp[-2].minor.yy401, yymsp[0].minor.yy385.subtypes);
}
#line 4272 "expparse.c"
        break;
      case 283: /* supertype_expression_list ::= supertype_expression_list TOK_COMMA supertype_expression */
#line 2237 "expparse.y"
{
    LISTadd_last(yymsp[-2].minor.yy371, (Generic)yymsp[0].minor.yy401);
    yygotominor.yy371 = yymsp[-2].minor.yy371;
}
#line 4280 "expparse.c"
        break;
      case 284: /* supertype_factor ::= identifier */
#line 2243 "expparse.y"
{
    yygotominor.yy385.subtypes = yymsp[0].minor.yy401;
}
#line 4287 "expparse.c"
        break;
      case 285: /* supertype_factor ::= oneof_op TOK_LEFT_PAREN supertype_expression_list TOK_RIGHT_PAREN */
#line 2248 "expparse.y"
{
    yygotominor.yy385.subtypes = EXPcreate(Type_Oneof);
    yygotominor.yy385.subtypes->u.list = yymsp[-1].minor.yy371;
}
#line 4295 "expparse.c"
        break;
      case 286: /* supertype_factor ::= TOK_LEFT_PAREN supertype_expression TOK_RIGHT_PAREN */
#line 2253 "expparse.y"
{
    yygotominor.yy385.subtypes = yymsp[-1].minor.yy401;
}
#line 4302 "expparse.c"
        break;
      case 287: /* type ::= aggregation_type */
      case 288: /* type ::= basic_type */ yytestcase(yyruleno==288);
      case 290: /* type ::= select_type */ yytestcase(yyruleno==290);
#line 2258 "expparse.y"
{
    yygotominor.yy378.type = 0;
    yygotominor.yy378.body = yymsp[0].minor.yy477;
}
#line 4312 "expparse.c"
        break;
      case 292: /* type_item_body ::= type */
#line 2283 "expparse.y"
{
    CURRENT_SCOPE->u.type->head = yymsp[0].minor.yy378.type;
    CURRENT_SCOPE->u.type->body = yymsp[0].minor.yy378.body;
}
#line 4320 "expparse.c"
        break;
      case 294: /* ti_start ::= TOK_IDENTIFIER TOK_EQUAL */
#line 2291 "expparse.y"
{
    Type t = TYPEcreate_name(yymsp[-1].minor.yy0.symbol);
    PUSH_SCOPE(t, yymsp[-1].minor.yy0.symbol, OBJ_TYPE);
}
#line 4328 "expparse.c"
        break;
      case 296: /* td_start ::= TOK_TYPE type_item where_rule_OPT */
#line 2302 "expparse.y"
{
    CURRENT_SCOPE->where = yymsp[0].minor.yy371;
    POP_SCOPE();
    yygotominor.yy0 = yymsp[-2].minor.yy0;
}
#line 4337 "expparse.c"
        break;
      case 297: /* general_ref ::= assignable group_ref */
#line 2309 "expparse.y"
{
    yymsp[0].minor.yy401->e.op1 = yymsp[-1].minor.yy401;
    yygotominor.yy401 = yymsp[0].minor.yy401;
}
#line 4345 "expparse.c"
        break;
      case 307: /* unary_expression ::= TOK_NOT unary_expression */
#line 2352 "expparse.y"
{
    yygotominor.yy401 = UN_EXPcreate(OP_NOT, yymsp[0].minor.yy401);
}
#line 4352 "expparse.c"
        break;
      case 309: /* unary_expression ::= TOK_MINUS unary_expression */
#line 2360 "expparse.y"
{
    yygotominor.yy401 = UN_EXPcreate(OP_NEGATE, yymsp[0].minor.yy401);
}
#line 4359 "expparse.c"
        break;
      case 310: /* unique ::= */
#line 2365 "expparse.y"
{
    yygotominor.yy252.unique = 0;
}
#line 4366 "expparse.c"
        break;
      case 311: /* unique ::= TOK_UNIQUE */
#line 2369 "expparse.y"
{
    yygotominor.yy252.unique = 1;
}
#line 4373 "expparse.c"
        break;
      case 315: /* labelled_attrib_list ::= qualified_attr_list semicolon */
#line 2396 "expparse.y"
{
    LISTadd_first(yymsp[-1].minor.yy371, (Generic)EXPRESSION_NULL);
    yygotominor.yy371 = yymsp[-1].minor.yy371;
}
#line 4381 "expparse.c"
        break;
      case 316: /* labelled_attrib_list ::= TOK_IDENTIFIER TOK_COLON qualified_attr_list semicolon */
#line 2402 "expparse.y"
{
    LISTadd_first(yymsp[-1].minor.yy371, (Generic)yymsp[-3].minor.yy0.symbol); 
    yygotominor.yy371 = yymsp[-1].minor.yy371;
}
#line 4389 "expparse.c"
        break;
      case 317: /* labelled_attrib_list_list ::= labelled_attrib_list */
#line 2409 "expparse.y"
{
    yygotominor.yy371 = LISTcreate();
    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy371);
}
#line 4397 "expparse.c"
        break;
      case 318: /* labelled_attrib_list_list ::= labelled_attrib_list_list labelled_attrib_list */
#line 2415 "expparse.y"
{
    LISTadd_last(yymsp[-1].minor.yy371, (Generic)yymsp[0].minor.yy371);
    yygotominor.yy371 = yymsp[-1].minor.yy371;
}
#line 4405 "expparse.c"
        break;
      case 321: /* until_control ::= */
      case 330: /* while_control ::= */ yytestcase(yyruleno==330);
#line 2430 "expparse.y"
{
    yygotominor.yy401 = 0;
}
#line 4413 "expparse.c"
        break;
      case 323: /* where_clause ::= expression semicolon */
#line 2439 "expparse.y"
{
    yygotominor.yy234 = WHERE_new();
    yygotominor.yy234->label = SYMBOLcreate("<unnamed>", yylineno, current_filename);
    yygotominor.yy234->expr = yymsp[-1].minor.yy401;
}
#line 4422 "expparse.c"
        break;
      case 324: /* where_clause ::= TOK_IDENTIFIER TOK_COLON expression semicolon */
#line 2445 "expparse.y"
{
    yygotominor.yy234 = WHERE_new();
    yygotominor.yy234->label = yymsp[-3].minor.yy0.symbol;
    yygotominor.yy234->expr = yymsp[-1].minor.yy401;

    if (!CURRENT_SCOPE->symbol_table) {
    CURRENT_SCOPE->symbol_table = DICTcreate(25);
    }

    DICTdefine(CURRENT_SCOPE->symbol_table, yymsp[-3].minor.yy0.symbol->name, (Generic)yygotominor.yy234,
    yymsp[-3].minor.yy0.symbol, OBJ_WHERE);
}
#line 4438 "expparse.c"
        break;
      case 325: /* where_clause_list ::= where_clause */
#line 2459 "expparse.y"
{
    yygotominor.yy371 = LISTcreate();
    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy234);
}
#line 4446 "expparse.c"
        break;
      case 326: /* where_clause_list ::= where_clause_list where_clause */
#line 2464 "expparse.y"
{
    yygotominor.yy371 = yymsp[-1].minor.yy371;
    LISTadd_last(yygotominor.yy371, (Generic)yymsp[0].minor.yy234);
}
#line 4454 "expparse.c"
        break;
      default:
      /* (4) action_body_item_rep ::= */ yytestcase(yyruleno==4);
      /* (41) block_list ::= */ yytestcase(yyruleno==41);
      /* (62) constant_body_list ::= */ yytestcase(yyruleno==62);
      /* (86) express_file ::= schema_decl_list */ yytestcase(yyruleno==86);
      /* (159) parened_rename_list ::= TOK_LEFT_PAREN rename_list TOK_RIGHT_PAREN */ yytestcase(yyruleno==159);
      /* (168) interface_specification_list ::= */ yytestcase(yyruleno==168);
      /* (194) local_body ::= */ yytestcase(yyruleno==194);
      /* (196) local_decl ::= TOK_LOCAL local_decl_rules_on local_body TOK_END_LOCAL semicolon local_decl_rules_off */ yytestcase(yyruleno==196);
      /* (293) type_item ::= ti_start type_item_body semicolon */ yytestcase(yyruleno==293);
        break;
  };
  yygoto = yyRuleInfo[yyruleno].lhs;
  yysize = yyRuleInfo[yyruleno].nrhs;
  yypParser->yyidx -= yysize;
  yyact = yy_find_reduce_action(yymsp[-yysize].stateno,(YYCODETYPE)yygoto);
  if( yyact < YYNSTATE ){
#ifdef NDEBUG
    /* If we are not debugging and the reduce action popped at least
    ** one element off the stack, then we can push the new element back
    ** onto the stack here, and skip the stack overflow test in yy_shift().
    ** That gives a significant speed improvement. */
    if( yysize ){
      yypParser->yyidx++;
      yymsp -= yysize-1;
      yymsp->stateno = (YYACTIONTYPE)yyact;
      yymsp->major = (YYCODETYPE)yygoto;
      yymsp->minor = yygotominor;
    }else
#endif
    {
      yy_shift(yypParser,yyact,yygoto,&yygotominor);
    }
  }else{
    assert( yyact == YYNSTATE + YYNRULE + 1 );
    yy_accept(yypParser);
  }
}

/*
** The following code executes when the parse fails
*/
#ifndef YYNOERRORRECOVERY
static void yy_parse_failed(
  yyParser *yypParser           /* The parser */
){
  ParseARG_FETCH;
#ifndef NDEBUG
  if( yyTraceFILE ){
    fprintf(yyTraceFILE,"%sFail!\n",yyTracePrompt);
  }
#endif
  while( yypParser->yyidx>=0 ) yy_pop_parser_stack(yypParser);
  /* Here code is inserted which will be executed whenever the
  ** parser fails */
  ParseARG_STORE; /* Suppress warning about unused %extra_argument variable */
}
#endif /* YYNOERRORRECOVERY */

/*
** The following code executes when a syntax error first occurs.
*/
static void yy_syntax_error(
  yyParser *yypParser,           /* The parser */
  int yymajor,                   /* The major type of the error token */
  YYMINORTYPE yyminor            /* The minor type of the error token */
){
  ParseARG_FETCH;
#define TOKEN (yyminor.yy0)
#line 2492 "expparse.y"

    Symbol sym;

    (void) yymajor; /* quell unused param warning */
    (void) yyminor;
    yyerrstatus++;

    sym.line = yylineno;
    sym.filename = current_filename;

    ERRORreport_with_symbol(ERROR_syntax, &sym, "Syntax error",
    CURRENT_SCOPE_TYPE_PRINTABLE, CURRENT_SCOPE_NAME);
#line 4538 "expparse.c"
  ParseARG_STORE; /* Suppress warning about unused %extra_argument variable */
}

/*
** The following is executed when the parser accepts
*/
static void yy_accept(
  yyParser *yypParser           /* The parser */
){
  ParseARG_FETCH;
#ifndef NDEBUG
  if( yyTraceFILE ){
    fprintf(yyTraceFILE,"%sAccept!\n",yyTracePrompt);
  }
#endif
  while( yypParser->yyidx>=0 ) yy_pop_parser_stack(yypParser);
  /* Here code is inserted which will be executed whenever the
  ** parser accepts */
  ParseARG_STORE; /* Suppress warning about unused %extra_argument variable */
}

/* The main parser program.
** The first argument is a pointer to a structure obtained from
** "ParseAlloc" which describes the current state of the parser.
** The second argument is the major token number.  The third is
** the minor token.  The fourth optional argument is whatever the
** user wants (and specified in the grammar) and is available for
** use by the action routines.
**
** Inputs:
** <ul>
** <li> A pointer to the parser (an opaque structure.)
** <li> The major token number.
** <li> The minor token number.
** <li> An option argument of a grammar-specified type.
** </ul>
**
** Outputs:
** None.
*/
void Parse(
  void *yyp,                   /* The parser */
  int yymajor,                 /* The major token code number */
  ParseTOKENTYPE yyminor       /* The value for the token */
  ParseARG_PDECL               /* Optional %extra_argument parameter */
){
  YYMINORTYPE yyminorunion;
  int yyact;            /* The parser action. */
  int yyendofinput;     /* True if we are at the end of input */
#ifdef YYERRORSYMBOL
  int yyerrorhit = 0;   /* True if yymajor has invoked an error */
#endif
  yyParser *yypParser;  /* The parser */

  /* (re)initialize the parser, if necessary */
  yypParser = (yyParser*)yyp;
  if( yypParser->yyidx<0 ){
#if YYSTACKDEPTH<=0
    if( yypParser->yystksz <=0 ){
      /*memset(&yyminorunion, 0, sizeof(yyminorunion));*/
      yyminorunion = yyzerominor;
      yyStackOverflow(yypParser, &yyminorunion);
      return;
    }
#endif
    yypParser->yyidx = 0;
    yypParser->yyerrcnt = -1;
    yypParser->yystack[0].stateno = 0;
    yypParser->yystack[0].major = 0;
  }
  yyminorunion.yy0 = yyminor;
  yyendofinput = (yymajor==0);
  ParseARG_STORE;

#ifndef NDEBUG
  if( yyTraceFILE ){
    fprintf(yyTraceFILE,"%sInput %s\n",yyTracePrompt,yyTokenName[yymajor]);
  }
#endif

  do{
    yyact = yy_find_shift_action(yypParser,(YYCODETYPE)yymajor);
    if( yyact<YYNSTATE ){
      assert( !yyendofinput );  /* Impossible to shift the $ token */
      yy_shift(yypParser,yyact,yymajor,&yyminorunion);
      yypParser->yyerrcnt--;
      yymajor = YYNOCODE;
    }else if( yyact < YYNSTATE + YYNRULE ){
      yy_reduce(yypParser,yyact-YYNSTATE);
    }else{
      assert( yyact == YY_ERROR_ACTION );
#ifdef YYERRORSYMBOL
      int yymx;
#endif
#ifndef NDEBUG
      if( yyTraceFILE ){
        fprintf(yyTraceFILE,"%sSyntax Error!\n",yyTracePrompt);
      }
#endif
#ifdef YYERRORSYMBOL
      /* A syntax error has occurred.
      ** The response to an error depends upon whether or not the
      ** grammar defines an error token "ERROR".  
      **
      ** This is what we do if the grammar does define ERROR:
      **
      **  * Call the %syntax_error function.
      **
      **  * Begin popping the stack until we enter a state where
      **    it is legal to shift the error symbol, then shift
      **    the error symbol.
      **
      **  * Set the error count to three.
      **
      **  * Begin accepting and shifting new tokens.  No new error
      **    processing will occur until three tokens have been
      **    shifted successfully.
      **
      */
      if( yypParser->yyerrcnt<0 ){
        yy_syntax_error(yypParser,yymajor,yyminorunion);
      }
      yymx = yypParser->yystack[yypParser->yyidx].major;
      if( yymx==YYERRORSYMBOL || yyerrorhit ){
#ifndef NDEBUG
        if( yyTraceFILE ){
          fprintf(yyTraceFILE,"%sDiscard input token %s\n",
             yyTracePrompt,yyTokenName[yymajor]);
        }
#endif
        yy_destructor(yypParser, (YYCODETYPE)yymajor,&yyminorunion);
        yymajor = YYNOCODE;
      }else{
         while(
          yypParser->yyidx >= 0 &&
          yymx != YYERRORSYMBOL &&
          (yyact = yy_find_reduce_action(
                        yypParser->yystack[yypParser->yyidx].stateno,
                        YYERRORSYMBOL)) >= YYNSTATE
        ){
          yy_pop_parser_stack(yypParser);
        }
        if( yypParser->yyidx < 0 || yymajor==0 ){
          yy_destructor(yypParser,(YYCODETYPE)yymajor,&yyminorunion);
          yy_parse_failed(yypParser);
          yymajor = YYNOCODE;
        }else if( yymx!=YYERRORSYMBOL ){
          YYMINORTYPE u2;
          u2.YYERRSYMDT = 0;
          yy_shift(yypParser,yyact,YYERRORSYMBOL,&u2);
        }
      }
      yypParser->yyerrcnt = 3;
      yyerrorhit = 1;
#elif defined(YYNOERRORRECOVERY)
      /* If the YYNOERRORRECOVERY macro is defined, then do not attempt to
      ** do any kind of error recovery.  Instead, simply invoke the syntax
      ** error routine and continue going as if nothing had happened.
      **
      ** Applications can set this macro (for example inside %include) if
      ** they intend to abandon the parse upon the first syntax error seen.
      */
      yy_syntax_error(yypParser,yymajor,yyminorunion);
      yy_destructor(yypParser,(YYCODETYPE)yymajor,&yyminorunion);
      yymajor = YYNOCODE;
      
#else  /* YYERRORSYMBOL is not defined */
      /* This is what we do if the grammar does not define ERROR:
      **
      **  * Report an error message, and throw away the input token.
      **
      **  * If the input token is $, then fail the parse.
      **
      ** As before, subsequent error messages are suppressed until
      ** three input tokens have been successfully shifted.
      */
      if( yypParser->yyerrcnt<=0 ){
        yy_syntax_error(yypParser,yymajor,yyminorunion);
      }
      yypParser->yyerrcnt = 3;
      yy_destructor(yypParser,(YYCODETYPE)yymajor,&yyminorunion);
      if( yyendofinput ){
        yy_parse_failed(yypParser);
      }
      yymajor = YYNOCODE;
#endif
    }
  }while( yymajor!=YYNOCODE && yypParser->yyidx>=0 );
  return;
}
