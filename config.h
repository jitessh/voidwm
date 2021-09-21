/* See LICENSE file for copyright and license details. */

/* --------------- settings -------------- */
static const char *fonts[]      = { "JetBrainsMono Nerd Font Mono:style=medium:size=10", "Material Icons:size=10" };
static unsigned int borderpx    = 2;        /* border pixel of windows */
static unsigned int snap        = 10;       /* snap pixel */
static int swallowfloating      = 0;        /* 1 means swallow floating windows by default */
static const float mfact        = 0.50;     /* factor of master area size [0.05..0.95] */
static const int nmaster        = 1;        /* number of clients in master area */
static const int resizehints    = 0;        /* 1 means respect size hints in tiled resizals */

/* ----------------- gaps ---------------- */
static          int smartgaps   = 0;        /* 1 means no outer gap when there is only one window */
static          int enablegaps  = 1;        /* 1 means enable gaps on startup */
static unsigned int gappih      = 10;       /* horiz inner gap between windows */
static unsigned int gappiv      = 10;       /* vert inner gap between windows */
static unsigned int gappoh      = 10;       /* horiz outer gap between windows and screen edge */
static unsigned int gappov      = 10;       /* vert outer gap between windows and screen edge */

/* ----------------- bar ----------------- */
#define BARHEIGHT_STR           "27"        /* dmenu_run's -h value, match with barheight below */
static int barheight            = 27;       /* 0 means dwm will calculate bar height wrt font */
static int showbar              = 1;        /* 0 means no bar */
static int topbar               = 1;        /* 0 means bottom bar */

/* ------------- colorscheme ------------- */
#include "colors/amarena.h"
static char *colors[][3]        = {
	/*               fg             bg              border  */
	[SchemeNorm] = { foreground,    background,     color8  },
	[SchemeSel]  = { color12,       background,     color12 },
	[SchemeUrg]  = { color9,        background,     color9  },
};

/* ----------------- tags ---------------- */
static const char *tags[]       = { "", "", "", "", "", "", "" };

/* --------------- patches --------------- */
#define FORCE_VSPLIT            1           /* nrowgrid layout: force two clients to always split vertically */
#define VIEWONTAG               1           /* switch view on tag switch */
#define PERTAG_VANITYGAPS       1           /* vanitygaps per tag */

/* --------------- systray --------------- */
static unsigned int systrayspacing  = 3;    /* systray spacing */
static          int showsystray     = 1;    /* 0 means no systray */

/* -------------- autostart -------------- */
static const char autostartsh[]     = "autostart.sh";
static const char autostartwaitsh[] = "autostartwait.sh";
static const char dwmdir[]          = "dwm";     /* parent dir of autostart(wait).sh */
static const char pathfromhome[]    = ".config"; /* custom dir from $HOME if $XDG_CONFIG_HOME was not set; don't add leading & trailing '/' */

/* ------------ occ indicator ------------ */
static const unsigned int ulinepad      = 5;     /* horizontal padding between the underline and tag */
static const unsigned int ulinestroke   = 2;     /* underline thickness */
static const unsigned int ulinevoffset  = 0;     /* how far above the bottom of the bar the line should appear */

/* --------------- defaults -------------- */
#define APP_BROWSER             "firefox"
#define APP_BROWSER_            "firefox --private-window"
#define APP_MENU                "dmenu_run"
#define APP_TERMINAL            "st"
#define CLASS_TERMINAL          "St"

/* ---------------- rules ---------------- */
static const Rule rules[]       = {
	/* xprop(1):
	 *	WM_CLASS(STRING) = instance, class
	 *	WM_NAME(STRING) = title
	 */
	/* class            instance    title           tags mask   isfloating  isterminal  noswallow   monitor scratchpad */
	{ "Gimp",           NULL,       NULL,           0,          1,          0,          0,          -1,     0   },
	{ "ScratchPad",     NULL,       "scratchpad",   0,          1,          1,          0,          -1,     's' },
	{ CLASS_TERMINAL,   NULL,       NULL,           0,          0,          1,          0,          -1,     0   },
	{ NULL,             NULL,       "Event Tester", 0,          0,          0,          1,          -1,     0   },
};

/* -------------- barmodules ------------- */
static const BarRule barrules[] = {
	/* Bar rules allow you to configure what is shown where on the bar, as well as
	 * introducing your own bar modules.
	 *
	 *    monitor:
	 *      -1  show on all monitors
	 *       0  show on monitor 0
	 *      'A' show on active monitor (i.e. focused / selected) (or just -1 for active?)
	 *    bar - bar index, 0 is default, 1 is extrabar
	 *    alignment - how the module is aligned compared to other modules
	 *    widthfunc, drawfunc, clickfunc - providing bar module width, draw and click functions
	 *    name - does nothing, intended for visual clue and for logging / debugging
	 */
	/* monitor  bar    alignment                widthfunc              drawfunc              clickfunc           name */
	{ -1,       0,     BAR_ALIGN_CENTER,        width_tags,            draw_tags,            click_tags,         "tags"     },
	{ -1,       0,     BAR_ALIGN_LEFT_LEFT,     width_ltsymbol,        draw_ltsymbol,        click_ltsymbol,     "layout"   },
	/* { -1,       0,     BAR_ALIGN_LEFT_LEFT,     width_wintitle,        draw_wintitle,        click_wintitle,     "wintitle" }, */
	{ 'A',      0,     BAR_ALIGN_RIGHT_RIGHT,   width_systray,         draw_systray,         click_systray,      "systray"  },
	{ 'A',      0,     BAR_ALIGN_RIGHT_RIGHT,   width_status2d,        draw_status2d,        click_status2d,     "status2d" },
	/* { 'A',      0,     BAR_ALIGN_RIGHT_RIGHT,   width_status,          draw_status,          click_status,       "status"   }, */
};

/* --------------- layouts --------------- */
#include "vanitygaps.c"
static const Layout layouts[]   = {
	/* symbol   arrange function */
	{ "",      tile },                     /* Default: Master on left, Slaves on right */
	{ "",      bstack },                   /* Master on top, Slaves on bottom */
	{ "",      grid },                     /* Grid layout */
	{ "",      gaplessgrid },              /* Grid layout but without gaps */
	{ "",      centeredmaster },           /* Master in middle, Slaves on sides */
	{ "",      centeredfloatingmaster },   /* Same as centeredmaster but Master floats */
	{ "",      monocle },                  /* All windows on top of each other */
	{ "",      deck },                     /* Master on left, Slaves in monocle-like mode on right */
	{ "",      spiral },                   /* Fibonacci spiral */
	{ "",      dwindle },                  /* Decreasing in size right and leftward */
	{ "",      NULL },                     /* No layout function means floating behaviour */
	{ NULL,     NULL },
};

/* -------------- Xresources ------------- */
ResourcePref resources[]        = {
	{ "foreground", STRING,  &foreground },
	{ "background", STRING,  &background },
	{ "color0",     STRING,  &color0 },
	{ "color8",     STRING,  &color8 },
	{ "color1",     STRING,  &color1 },
	{ "color9",     STRING,  &color9 },
	{ "color2",     STRING,  &color2 },
	{ "color10",    STRING,  &color10 },
	{ "color3",     STRING,  &color3 },
	{ "color11",    STRING,  &color11 },
	{ "color4",     STRING,  &color4 },
	{ "color12",    STRING,  &color12 },
	{ "color5",     STRING,  &color5 },
	{ "color13",    STRING,  &color13 },
	{ "color6",     STRING,  &color6 },
	{ "color14",    STRING,  &color14 },
	{ "color7",     STRING,  &color7 },
	{ "color15",    STRING,  &color15 },
	{ "barheight",  INTEGER, &barheight },
	{ "showbar",    INTEGER, &showbar },
	{ "topbar",     INTEGER, &topbar },
	{ "borderpx",   INTEGER, &borderpx },
	{ "snap",       INTEGER, &snap },
	{ "enablegaps", INTEGER, &enablegaps },
	{ "smartgaps",  INTEGER, &smartgaps },
	{ "showsystray",INTEGER, &showsystray },
};

/* ----------- key definitions ----------- */
#define MODKEY Mod4Mask
#define TAGKEYS(KEY,TAG) \
	{ MODKEY,                       KEY,      view,           {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask,           KEY,      toggleview,     {.ui = 1 << TAG} }, \
	{ MODKEY|ShiftMask,             KEY,      tag,            {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask|ShiftMask, KEY,      toggletag,      {.ui = 1 << TAG} },

/* --------------- commands -------------- */
/* helper for spawning shell commands in the pre dwm-5.0 fashion */
#define SHCMD(cmd) { .v = (const char*[]){ "/bin/sh", "-c", cmd, NULL } }

/* first arg only serves to match against key in rules */
static const char *scratchpadcmd[] = {"s", "st", "-c", "ScratchPad", "-t", "scratchpad", NULL};

static const char *menucmd[]    = { APP_MENU, "-h", BARHEIGHT_STR, NULL };
static const char *termcmd[]    = { APP_TERMINAL, NULL };

#include <X11/XF86keysym.h>
static const char vol_up[]      =  "pactl set-sink-volume 0 +5%; kill -44 $(pidof dwmblocks)" ;
static const char vol_down[]    =  "pactl set-sink-volume 0 -5%; kill -44 $(pidof dwmblocks)" ;
static const char vol_mute[]    =  "pactl set-sink-mute 0 toggle; kill -44 $(pidof dwmblocks)";
#define XCLIP_PNG               " | xclip -selection clipboard -target image/png"

/* ------------- keybindings ------------- */
static Key keys[]               = {
	/* modifier                     key         function        argument */
	{ MODKEY,                       XK_Return,  spawn,          {.v = termcmd } },
	{ MODKEY,                       XK_d,       spawn,          {.v = menucmd } },
	{ MODKEY,                       XK_q,       xrdb,           {.v = NULL    } },
	{ MODKEY|ShiftMask,             XK_q,       quit,           {1} },
	{ MODKEY|ControlMask,           XK_q,       quit,           {0} },

	/* ---------- layouts ---------- */
	{ MODKEY,                       XK_t,       setlayout,      {.v = &layouts[0]} }, /* tile */
	{ MODKEY|ShiftMask,             XK_t,       setlayout,      {.v = &layouts[3]} }, /* gaplessgrid */
	{ MODKEY,                       XK_y,       setlayout,      {.v = &layouts[4]} }, /* centeredmaster */
	{ MODKEY|ShiftMask,             XK_y,       setlayout,      {.v = &layouts[7]} }, /* deck */
	{ MODKEY,                       XK_Tab,     cyclelayouts,   { .i = +1 } },
	{ MODKEY|ShiftMask,             XK_Tab,     cyclelayouts,   { .i = -1 } },

	/* ------------ gaps ----------- */
	{ MODKEY,                       XK_g,       togglegaps,     {0} },        /* toggle  gaps */
	{ MODKEY|ShiftMask,             XK_g,       defaultgaps,    {0} },        /* reset   gaps */
	{ MODKEY,                       XK_minus,   incrogaps,      {.i = -3 } }, /* dec out gaps */
	{ MODKEY,                       XK_equal,   incrogaps,      {.i = +3 } }, /* inc out gaps */
	{ MODKEY|ShiftMask,             XK_minus,   incrigaps,      {.i = -3 } }, /* dec inr gaps */
	{ MODKEY|ShiftMask,             XK_equal,   incrigaps,      {.i = +3 } }, /* inc inr gaps */
	{ MODKEY|ControlMask,           XK_minus,   incrgaps,       {.i = -3 } }, /* dec all gaps */
	{ MODKEY|ControlMask,           XK_equal,   incrgaps,       {.i = +3 } }, /* inc all gaps */

	/* ----------- stack ----------- */
	{ MODKEY|ShiftMask,             XK_c,       killclient,     {0} },
	{ MODKEY,                       XK_e,       focusurgent,    {0} },
	{ MODKEY,                       XK_s,       togglesticky,   {0} },
	{ MODKEY,                       XK_j,       focusstack,     {.i = +1 } },
	{ MODKEY,                       XK_k,       focusstack,     {.i = -1 } },
	{ MODKEY|ShiftMask,             XK_j,       rotatestack,    {.i = +1 } },
	{ MODKEY|ShiftMask,             XK_k,       rotatestack,    {.i = -1 } },
	{ MODKEY,                       XK_m,       incnmaster,     {.i = +1 } },
	{ MODKEY|ShiftMask,             XK_m,       incnmaster,     {.i = -1 } },

	/* ---------- monitor ---------- */
	{ MODKEY,                       XK_comma,   focusmon,       {.i = -1 } },
	{ MODKEY,                       XK_period,  focusmon,       {.i = +1 } },
	{ MODKEY|ShiftMask,             XK_comma,   tagmon,         {.i = -1 } },
	{ MODKEY|ShiftMask,             XK_period,  tagmon,         {.i = +1 } },

	/* ----------- resize ---------- */
	{ MODKEY,                       XK_space,   togglefloating,          {0} },
	{ MODKEY,                       XK_f,       togglefullscreen,        {0} },
	{ MODKEY|ShiftMask,             XK_f,       togglefakefullscreen,    {0} },
	{ MODKEY|ControlMask,           XK_h,       setmfact,       {.f = -0.05} },
	{ MODKEY|ControlMask,           XK_j,       setcfact,       {.f = -0.05} },
	{ MODKEY|ControlMask,           XK_k,       setcfact,       {.f = +0.05} },
	{ MODKEY|ControlMask,           XK_l,       setmfact,       {.f = +0.05} },
	{ MODKEY|ControlMask,           XK_g,       setcfact,       {.f =  0.00} },

	/* ------------ bar ------------ */
	{ MODKEY,                       XK_b,       togglebar,      {0} },

	/* ------------ tag ------------ */
	{ Mod1Mask,                     XK_Tab,     shiftviewclients, { .i = +1 } },
	{ Mod1Mask|ShiftMask,           XK_Tab,     shiftviewclients, { .i = -1 } },
	{ MODKEY,                       XK_r,       reorganizetags, {0} },
	{ MODKEY|ShiftMask,             XK_r,       distributetags, {0} },

	/* --------- scratchpad -------- */
	{ MODKEY,                       XK_n,       setscratch,     {.v = scratchpadcmd } },
	{ MODKEY|ShiftMask,             XK_n,       removescratch,  {.v = scratchpadcmd } },
	{ MODKEY,                       XK_p,       togglescratch,  {.v = scratchpadcmd } },

	/* ---------- keyboard --------- */
	{ MODKEY,                       XK_a,       spawn,          SHCMD("maim -qu"  XCLIP_PNG) },
	{ MODKEY|ShiftMask,             XK_a,       spawn,          SHCMD("maim -qus" XCLIP_PNG) },
	{ 0,            XF86XK_AudioLowerVolume,    spawn,          SHCMD(vol_down) },
	{ 0,            XF86XK_AudioRaiseVolume,    spawn,          SHCMD(vol_up)   },
	{ 0,            XF86XK_AudioMute,           spawn,          SHCMD(vol_mute) },
	{ MODKEY,       XK_bracketleft,             spawn,          SHCMD(vol_down) },
	{ MODKEY,       XK_bracketright,            spawn,          SHCMD(vol_up)   },
	{ MODKEY,       XK_backslash,               spawn,          SHCMD(vol_mute) },
	{ MODKEY|ShiftMask, XK_bracketleft,         spawn,          SHCMD("xbacklight -dec 10") },
	{ MODKEY|ShiftMask, XK_bracketright,        spawn,          SHCMD("xbacklight -inc 10") },
	{ MODKEY|ShiftMask, XK_backslash,           spawn,          SHCMD("xbacklight -set 50") },

	/* ------------ apps ----------- */
	{ MODKEY,                       XK_F2,      spawn,          SHCMD(APP_BROWSER)  },
	{ MODKEY,                       XK_F3,      spawn,          SHCMD(APP_BROWSER_) },

	/* ----------- other ----------- */
	/* { MODKEY,                       XK_Tab,     view,           {0} }, */
	{ MODKEY,                       XK_0,       view,           {.ui = ~0 } },
	{ MODKEY|ShiftMask,             XK_0,       tag,            {.ui = ~0 } },

	/* ------------ tag ------------ */
	TAGKEYS(                        XK_1,                      0)
	TAGKEYS(                        XK_2,                      1)
	TAGKEYS(                        XK_3,                      2)
	TAGKEYS(                        XK_4,                      3)
	TAGKEYS(                        XK_5,                      4)
	TAGKEYS(                        XK_6,                      5)
	TAGKEYS(                        XK_7,                      6)
	/* TAGKEYS(                        XK_8,                      7) */
	/* TAGKEYS(                        XK_9,                      8) */
};

/* ---------- button definitions --------- */
/* click can be ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle, ClkClientWin, or ClkRootWin */
static Button buttons[]         = {
	/* click                event mask      button          function        argument */
	{ ClkLtSymbol,          0,              Button1,        setlayout,      {0} },
	{ ClkLtSymbol,          0,              Button3,        setlayout,      {.v = &layouts[2]} },
	{ ClkWinTitle,          0,              Button2,        zoom,           {0} },
	{ ClkStatusText,        0,              Button2,        spawn,          {.v = termcmd } },
	/* placemouse options, choose which feels more natural:
	 *    0 - tiled position is relative to mouse cursor
	 *    1 - tiled position is relative to window center
	 *    2 - mouse pointer warps to window center
	 *
	 * The moveorplace uses movemouse or placemouse depending on the floating state
	 * of the selected client. Set up individual keybindings for the two if you want
	 * to control these separately (i.e. to retain the feature to move a tiled window
	 * into a floating position).
	 */
	{ ClkClientWin,         MODKEY,         Button1,        moveorplace,    {.i = 2} },
	{ ClkClientWin,         MODKEY,         Button2,        togglefloating, {0} },
	{ ClkClientWin,         MODKEY,         Button3,        resizemouse,    {0} },
	{ ClkClientWin,         MODKEY|ShiftMask, Button3,      dragcfact,      {0} },
	{ ClkClientWin,         MODKEY|ShiftMask, Button1,      dragmfact,      {0} },
	{ ClkTagBar,            0,              Button1,        view,           {0} },
	{ ClkTagBar,            0,              Button3,        toggleview,     {0} },
	{ ClkTagBar,            MODKEY,         Button1,        tag,            {0} },
	{ ClkTagBar,            MODKEY,         Button3,        toggletag,      {0} },
};
