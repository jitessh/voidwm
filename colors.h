/* ---------- styling guidlines ----------- */
/* norm_fg     : text color of unselected tags & status bar    : (color15) light color     same as urg_fg */
/* norm_bg     : bg color of unselected tags & status bar      : (color0)  dark color      same as sel_fg */
/* norm_border : border color of unselected clients            : (color8)  medium color */
/* sel_fg      : text color of selected tags                   : (color0)  dark color      same as norm_bg */
/* sel_bg      : bg color of selected tags                     : (color4)  main color      same as sel_border */
/* sel_border  : border color of selected tags                 : (color4)  main color      same as sel_bg */
/* urg_fg      : text color of urgent tags                     : (color15) light color     same as norm_fg */
/* urg_bg      : bg color of urgent tags                       : (color1)  urgent color    same as urg_border */
/* urg_border  : border color of urgent clients                : (color1)  urgent color    same as urg_bg */

/* ------------ gruvbox theme ------------- */
static char norm_fg[]           = "#ebdbb2";
static char norm_bg[]           = "#282828";
static char norm_border[]       = "#928374";
static char sel_fg[]            = "#282828";
static char sel_bg[]            = "#458588";
static char sel_border[]        = "#458588";
static char urg_fg[]            = "#ebdbb2";
static char urg_bg[]            = "#cc241d";
static char urg_border[]        = "#cc241d";
static char *colors[][3]        = {
    /*               fg           bg         border     */
    [SchemeNorm] = { norm_fg,     norm_bg,   norm_border },
    [SchemeSel]  = { sel_fg,      sel_bg,    sel_border },
    [SchemeUrg] =  { urg_fg,      urg_bg,    urg_border },
};
