/*
 *    VMS supplement for "telnetenable".
 */

#include <stdio.h>
#include <string.h>
#include <unixlib.h>

#include <starlet.h>
#include <fab.h>
#include <nam.h>
#include <rmsdef.h>
#include <stsdef.h>

/* Define macros for use with either NAM or NAML. */

#ifdef NAML$C_MAXRSS            /* NAML is available.  Use it. */

#  define NAM_STRUCT NAML

#  define FAB_OR_NAML( fab, nam) nam
#  define FAB_OR_NAML_FNA naml$l_long_filename
#  define FAB_OR_NAML_FNS naml$l_long_filename_size
#  define NAME_FNS naml$l_long_filename_size
#  define NAM_ESA naml$l_long_expand
#  define NAM_ESS naml$l_long_expand_alloc
#  define NAM_RSA naml$l_long_result
#  define NAM_RSS naml$l_long_result_alloc
#  define CC_RMS_NAM cc$rms_naml
#  define FAB_NAM fab$l_naml
#  define NAM_MAXRSS NAML$C_MAXRSS
#  define NAM_NOP naml$b_nop
#  define NAM_M_SYNCHK NAML$M_SYNCHK
#  define NAM_B_NAME naml$l_long_name_size
#  define NAM_L_NAME naml$l_long_name

#else /* def NAML$C_MAXRSS */   /* NAML is not available.  Use NAM. */

#  define NAM_STRUCT NAM

#  define FAB_OR_NAML( fab, nam) fab
#  define FAB_OR_NAML_FNA fab$l_fna
#  define FAB_OR_NAML_FNS fab$b_fns
#  define NAME_FNS fab$b_fns
#  define NAM_ESA nam$l_esa
#  define NAM_ESS nam$b_ess
#  define NAM_RSA nam$l_rsa
#  define NAM_RSS nam$b_rss
#  define CC_RMS_NAM cc$rms_nam
#  define FAB_NAM fab$l_nam
#  define NAM_MAXRSS NAM$C_MAXRSS
#  define NAM_NOP nam$b_nop
#  define NAM_M_SYNCHK NAM$M_SYNCHK
#  define NAM_B_NAME nam$b_name
#  define NAM_L_NAME nam$l_name

#endif /* def NAML$C_MAXRSS */



/* 2005-09-29 SMS.
 *
 * vms_basename()
 *
 *    Extract the basename from a VMS file spec.
 */

char *vms_basename( char *file_spec)
{
    /* Static storage for NAM[L], and so on. */

    static struct NAM_STRUCT nam;
    static char exp_name[ NAM_MAXRSS+ 1];
    static char res_name[ NAM_MAXRSS+ 1];

    struct FAB fab;
    int status;

    /* Set up the FAB and NAM[L] blocks. */

    fab = cc$rms_fab;                   /* Initialize FAB. */
    nam = CC_RMS_NAM;                   /* Initialize NAM[L]. */

    fab.FAB_NAM = &nam;                 /* FAB -> NAM[L] */

#ifdef NAML$C_MAXRSS

    fab.fab$l_dna = (char *) -1;    /* Using NAML for default name. */
    fab.fab$l_fna = (char *) -1;    /* Using NAML for file name. */

#endif /* def NAML$C_MAXRSS */

    /* Arg name and length. */
    FAB_OR_NAML( fab, nam).FAB_OR_NAML_FNA = file_spec;
    FAB_OR_NAML( fab, nam).FAB_OR_NAML_FNS = strlen( file_spec);

    nam.NAM_ESA = exp_name;         /* Expanded name. */
    nam.NAM_ESS = NAM_MAXRSS;       /* Max length. */
    nam.NAM_RSA = res_name;         /* Resulting name. */
    nam.NAM_RSS = NAM_MAXRSS;       /* Max length. */

    nam.NAM_NOP = NAM_M_SYNCHK;     /* Syntax-only analysis. */

    /* Parse the file name. */
    status = sys$parse( &fab);      /* What could go wrong? */

    nam.NAM_L_NAME[ nam.NAM_B_NAME] = '\0';

    return nam.NAM_L_NAME;
}



/*--------------------------------------------------------------------*/

/*======================================================================
 *
 *       vms_init()
 *
 *    On non-VAX systems, uses LIB$INITIALIZE to set a collection of C
 *    RTL features without using the DECC$* logical name method.
 *
 *----------------------------------------------------------------------
 */

#if !defined( __VAX) && (__CRTL_VER >= 70301000)

/* vms_init()

      Uses LIB$INITIALIZE to set a collection of C RTL features without
      requiring the user to define the corresponding logical names.
*/

/* Structure to hold a DECC$* feature name and its desired value. */

typedef struct
   {
   char *name;
   int value;
   } decc_feat_t;

/* Array of DECC$* feature names and their desired values. */

decc_feat_t decc_feat_array[] = {
   /* Preserve command-line case with SET PROCESS/PARSE_STYLE=EXTENDED */
 { "DECC$ARGV_PARSE_STYLE", 1 },
   /* List terminator. */
 { (char *)NULL, 0 } };

/* LIB$INITIALIZE initialization function. */

static void vms_init( void)
{
int feat_index;
int feat_value;
int feat_value_max;
int feat_value_min;
int i;
int sts;

/* Loop through all items in the decc_feat_array[]. */

for (i = 0; decc_feat_array[ i].name != NULL; i++)
   {
   /* Get the feature index. */
   feat_index = decc$feature_get_index( decc_feat_array[ i].name);
   if (feat_index >= 0)
      {
      /* Valid item.  Collect its properties. */
      feat_value = decc$feature_get_value( feat_index, 1);
      feat_value_min = decc$feature_get_value( feat_index, 2);
      feat_value_max = decc$feature_get_value( feat_index, 3);

      if ((decc_feat_array[ i].value >= feat_value_min) &&
       (decc_feat_array[ i].value <= feat_value_max))
         {
         /* Valid value.  Set it if necessary. */
         if (feat_value != decc_feat_array[ i].value)
            {
            sts = decc$feature_set_value( feat_index,
             1,
             decc_feat_array[ i].value);
            }
         }
      else
         {
         /* Invalid DECC feature value. */
         printf( " INVALID DECC FEATURE VALUE, %d: %d <= %s <= %d.\n",
          feat_value,
          feat_value_min, decc_feat_array[ i].name, feat_value_max);
         }
      }
   else
      {
      /* Invalid DECC feature name. */
      printf( " UNKNOWN DECC FEATURE: %s.\n", decc_feat_array[ i].name);
      }
   }
}

/* Get "vms_init()" into a valid, loaded LIB$INITIALIZE PSECT. */

#pragma nostandard

/* Establish the LIB$INITIALIZE PSECTs, with proper alignment and
   other attributes.  Note that "nopic" is significant only on VAX.
*/
#pragma extern_model save

#pragma extern_model strict_refdef "LIB$INITIALIZE" 2, nopic, nowrt
void (*const x_vms_init)() = vms_init;

#pragma extern_model strict_refdef "LIB$INITIALIZ" 2, nopic, nowrt
const int spare[ 8] = { 0 };

#pragma extern_model restore

/* Fake reference to ensure loading the LIB$INITIALIZE PSECT. */

#pragma extern_model save
int lib$initialize(void);
#pragma extern_model strict_refdef
int dmy_lib$initialize = (int) lib$initialize;
#pragma extern_model restore

#pragma standard

#endif /* !defined( __VAX) && (__CRTL_VER >= 70301000) */

/*--------------------------------------------------------------------*/
