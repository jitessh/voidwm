/* See LICENSE file for copyright and license details.
 *
 * dynamic window manager is designed like any other X client as well. It is
 * driven through handling X events. In contrast to other X clients, a window
 * manager selects for SubstructureRedirectMask on the root window, to receive
 * events about window (dis-)appearance. Only one X connection at a time is
 * allowed to select for this event mask.
 *
 * The event handlers of dwm are organized in an array which is accessed
 * whenever a new event has been fetched. This allows event dispatching
 * in O(1) time.
 *
 * Each child of the root window is called a client, except windows which have
 * set the override_redirect flag. Clients are organized in a linked client
 * list on each monitor, the focus history is remembered through a stack list
 * on each monitor. Each client contains a bit array to indicate the tags of a
 * client.
 *
 * Keys and tagging rules are organized as arrays and defined in config.h.
 *
 * To understand everything else, start reading main().
 */
#include <errno.h>
#include <locale.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xresource.h>
#include <X11/Xutil.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif /* XINERAMA */
#include <X11/Xft/Xft.h>
#include <X11/Xlib-xcb.h>
#include <xcb/res.h>
#ifdef __OpenBSD__
#include <sys/sysctl.h>
#include <kvm.h>
#endif /* __OpenBSD */

#include "drw.h"
#include "util.h"

/* macros */
#define BARRULES                20
#define BUTTONMASK              (ButtonPressMask|ButtonReleaseMask)
#define CLEANMASK(mask)         (mask & ~(numlockmask|LockMask) & (ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask))
#define INTERSECT(x,y,w,h,m)    (MAX(0, MIN((x)+(w),(m)->wx+(m)->ww) - MAX((x),(m)->wx)) \
                               * MAX(0, MIN((y)+(h),(m)->wy+(m)->wh) - MAX((y),(m)->wy)))
#define INTERSECTC(x,y,w,h,z)   (MAX(0, MIN((x)+(w),(z)->x+(z)->w) - MAX((x),(z)->x)) \
                               * MAX(0, MIN((y)+(h),(z)->y+(z)->h) - MAX((y),(z)->y)))
#define ISVISIBLE(C)            ((C->tags & C->mon->tagset[C->mon->seltags]) || C->issticky)
#define LENGTH(X)               (sizeof X / sizeof X[0])
#define MOUSEMASK               (BUTTONMASK|PointerMotionMask)
#define WIDTH(X)                ((X)->w + 2 * (X)->bw)
#define HEIGHT(X)               ((X)->h + 2 * (X)->bw)
#define TAGMASK                 ((1 << LENGTH(tags)) - 1)
#define TEXTW(X)                (drw_fontset_getwidth(drw, (X)) + lrpad)

/* enums */
enum { CurResizeBR, CurResizeBL, CurResizeTR, CurResizeTL,
       CurResizeHorzArrow, CurResizeVertArrow, CurIronCross, CurNormal, CurResize, CurMove, CurLast }; /* cursor */
enum { SchemeNorm, SchemeSel, SchemeUrg }; /* color schemes */
enum { NetSupported, NetWMName, NetWMState, NetWMCheck,
       NetWMFullscreen, NetActiveWindow, NetWMWindowType,
       NetSystemTray, NetSystemTrayOP, NetSystemTrayOrientation,
       NetSystemTrayVisual, NetWMWindowTypeDock, NetSystemTrayOrientationHorz,
       NetWMWindowTypeDialog, NetClientList, NetLast }; /* EWMH atoms */
enum { WMProtocols, WMDelete, WMState, WMTakeFocus, WMLast }; /* default atoms */
enum { ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle,
       ClkClientWin, ClkRootWin, ClkLast }; /* clicks */
enum {
	BAR_ALIGN_LEFT,
	BAR_ALIGN_CENTER,
	BAR_ALIGN_RIGHT,
	BAR_ALIGN_LEFT_LEFT,
	BAR_ALIGN_LEFT_RIGHT,
	BAR_ALIGN_LEFT_CENTER,
	BAR_ALIGN_NONE,
	BAR_ALIGN_RIGHT_LEFT,
	BAR_ALIGN_RIGHT_RIGHT,
	BAR_ALIGN_RIGHT_CENTER,
	BAR_ALIGN_LAST
}; /* bar alignment */

typedef union {
	int i;
	unsigned int ui;
	float f;
	const void *v;
} Arg;

typedef struct Monitor Monitor;
typedef struct Bar Bar;
struct Bar {
	Window win;
	Monitor *mon;
	Bar *next;
	int idx;
	int topbar;
	int bx, by, bw, bh; /* bar geometry */
	int w[BARRULES]; // module width
	int x[BARRULES]; // module position
};

typedef struct {
	int max_width;
} BarWidthArg;

typedef struct {
	int x;
	int w;
} BarDrawArg;

typedef struct {
	int rel_x;
	int rel_y;
	int rel_w;
	int rel_h;
} BarClickArg;

typedef struct {
	int monitor;
	int bar;
	int alignment; // see bar alignment enum
	int (*widthfunc)(Bar *bar, BarWidthArg *a);
	int (*drawfunc)(Bar *bar, BarDrawArg *a);
	int (*clickfunc)(Bar *bar, Arg *arg, BarClickArg *a);
	char *name; // for debugging
	int x, w; // position, width for internal use
} BarRule;

typedef struct {
	unsigned int click;
	unsigned int mask;
	unsigned int button;
	void (*func)(const Arg *arg);
	const Arg arg;
} Button;

typedef struct Pertag Pertag;
typedef struct Client Client;
struct Client {
	char name[256];
	float mina, maxa;
	float cfact;
	int x, y, w, h;
	int sfx, sfy, sfw, sfh; /* stored float geometry, used on mode revert */
	int oldx, oldy, oldw, oldh;
	int basew, baseh, incw, inch, maxw, maxh, minw, minh;
	int bw, oldbw;
	unsigned int tags;
	int isfixed, isfloating, isurgent, neverfocus, oldstate, isfullscreen, issticky, isterminal, noswallow;
	int ignorecfgreqpos, ignorecfgreqsize;
	pid_t pid;
	int beingmoved;
	int fakefullscreen;
	char scratchkey;
	Client *next;
	Client *snext;
	Client *swallowing;
	Monitor *mon;
	Window win;
};

typedef struct {
	unsigned int mod;
	KeySym keysym;
	void (*func)(const Arg *);
	const Arg arg;
} Key;

typedef struct {
	const char *symbol;
	void (*arrange)(Monitor *);
} Layout;

struct Monitor {
	char ltsymbol[16];
	float mfact;
	int nmaster;
	int num;
	int mx, my, mw, mh;   /* screen size */
	int wx, wy, ww, wh;   /* window area  */
	int gappih;           /* horizontal gap between windows */
	int gappiv;           /* vertical gap between windows */
	int gappoh;           /* horizontal outer gaps */
	int gappov;           /* vertical outer gaps */
	unsigned int seltags;
	unsigned int sellt;
	unsigned int tagset[2];
	int showbar;
	Client *clients;
	Client *sel;
	Client *stack;
	Monitor *next;
	Bar *bar;
	const Layout *lt[2];
	Pertag *pertag;
};

typedef struct {
	const char *class;
	const char *instance;
	const char *title;
	unsigned int tags;
	int isfloating;
	int isterminal;
	int noswallow;
	int monitor;
	const char scratchkey;
} Rule;

/* Xresources preferences */
enum resource_type {
	STRING = 0,
	INTEGER = 1,
	FLOAT = 2
};

typedef struct {
	char *name;
	enum resource_type type;
	void *dst;
} ResourcePref;

/* function declarations */
static void applyrules(Client *c);
static int applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact);
static void arrange(Monitor *m);
static void arrangemon(Monitor *m);
static void attach(Client *c);
static void attachstack(Client *c);
static void buttonpress(XEvent *e);
static void checkotherwm(void);
static void cleanup(void);
static void cleanupmon(Monitor *mon);
static void clientmessage(XEvent *e);
static void configure(Client *c);
static void configurenotify(XEvent *e);
static void configurerequest(XEvent *e);
static Monitor *createmon(void);
static void cyclelayouts(const Arg *arg);
static void destroynotify(XEvent *e);
static void detach(Client *c);
static void detachstack(Client *c);
static Monitor *dirtomon(int dir);
static void distributetags(const Arg *arg);
static void dragcfact(const Arg *arg);
static void dragmfact(const Arg *arg);
static void drawbar(Monitor *m);
static void drawbars(void);
static void drawbarwin(Bar *bar);
static void enqueue(Client *c);
static void enqueuestack(Client *c);
static void enternotify(XEvent *e);
static void expose(XEvent *e);
static void focus(Client *c);
static void focusin(XEvent *e);
static void focusmon(const Arg *arg);
static void focusstack(const Arg *arg);
static void focusurgent(const Arg *arg);
static Atom getatomprop(Client *c, Atom prop);
static void getgaps(Monitor *m, int *oh, int *ov, int *ih, int *iv, unsigned int *nc);
static pid_t getparentprocess(pid_t p);
static int getrootptr(int *x, int *y);
static long getstate(Window w);
static int gettextprop(Window w, Atom atom, char *text, unsigned int size);
static void grabbuttons(Client *c, int focused);
static void grabkeys(void);
static int isdescprocess(pid_t p, pid_t c);
static void incnmaster(const Arg *arg);
static void keypress(XEvent *e);
static void killclient(const Arg *arg);
static void loadxresources(void);
static void losefullscreen(Client *next);
static void manage(Window w, XWindowAttributes *wa);
static void mappingnotify(XEvent *e);
static void maprequest(XEvent *e);
static void monocle(Monitor *m);
static void motionnotify(XEvent *e);
static void movemouse(const Arg *arg);
static void moveorplace(const Arg *arg);
static Client *nexttiled(Client *c);
static void placemouse(const Arg *arg);
static void pop(Client *);
static void propertynotify(XEvent *e);
static void quit(const Arg *arg);
static Client *recttoclient(int x, int y, int w, int h);
static Monitor *recttomon(int x, int y, int w, int h);
static void removescratch(const Arg *arg);
static void reorganizetags(const Arg *arg);
static void replaceclient(Client *old, Client *new);
static void resize(Client *c, int x, int y, int w, int h, int interact);
static void resizeclient(Client *c, int x, int y, int w, int h);
static void resizemouse(const Arg *arg);
static void resourceload(XrmDatabase db, char *name, enum resource_type rtype, void *dst);
static void restack(Monitor *m);
static void rotatestack(const Arg *arg);
static void run(void);
static void runautostart(void);
static void scan(void);
static int sendevent(Window w, Atom proto, int m, long d0, long d1, long d2, long d3, long d4);
static void sendmon(Client *c, Monitor *m);
static void setclientstate(Client *c, long state);
static void setfocus(Client *c);
static void setfullscreen(Client *c, int fullscreen);
static void setlayout(const Arg *arg);
static void setcfact(const Arg *arg);
static void setmfact(const Arg *arg);
static void setscratch(const Arg *arg);
static void setup(void);
static void seturgent(Client *c, int urg);
static void shiftviewclients(const Arg *arg);
static void showhide(Client *c);
static void sigchld(int unused);
static void sighup(int unused);
static void sigterm(int unused);
static void spawn(const Arg *arg);
static void spawnscratch(const Arg *arg);
static int swallow(Client *p, Client *c);
static Client *swallowingclient(Window w);
static void tag(const Arg *arg);
static void tagmon(const Arg *arg);
static Client *termforwin(const Client *c);
static void togglebar(const Arg *arg);
static void togglefakefullscreen(const Arg *arg);
static void togglefloating(const Arg *arg);
static void togglefullscreen(const Arg *arg);
static void togglegaps(const Arg *arg);
static void togglescratch(const Arg *arg);
static void togglesticky(const Arg *arg);
static void toggletag(const Arg *arg);
static void toggleview(const Arg *arg);
static void unfocus(Client *c, int setfocus);
static void unmanage(Client *c, int destroyed);
static void unmapnotify(XEvent *e);
static void unswallow(Client *c);
static void updatebarpos(Monitor *m);
static void updatebars(void);
static void updateclientlist(void);
static int updategeom(void);
static void updatenumlockmask(void);
static void updatesizehints(Client *c);
static void updatestatus(void);
static void updatetitle(Client *c);
static void updatewindowtype(Client *c);
static void updatewmhints(Client *c);
static void view(const Arg *arg);
static pid_t winpid(Window w);
static Client *wintoclient(Window w);
static Monitor *wintomon(Window w);
static int xerror(Display *dpy, XErrorEvent *ee);
static int xerrordummy(Display *dpy, XErrorEvent *ee);
static int xerrorstart(Display *dpy, XErrorEvent *ee);
static void xrdb(const Arg *arg);
static void zoom(const Arg *arg);

#include "patch/include.h"
/* variables */
static const char broken[] = "broken";
static char stext[256];
static int screen;
static int sw, sh;           /* X display screen geometry width, height */
static int bh;               /* bar geometry */
static int lrpad;            /* sum of left and right padding for text */
static int (*xerrorxlib)(Display *, XErrorEvent *);
static unsigned int numlockmask = 0;
static void (*handler[LASTEvent]) (XEvent *) = {
	[ButtonPress] = buttonpress,
	[ClientMessage] = clientmessage,
	[ConfigureRequest] = configurerequest,
	[ConfigureNotify] = configurenotify,
	[DestroyNotify] = destroynotify,
	[EnterNotify] = enternotify,
	[Expose] = expose,
	[FocusIn] = focusin,
	[KeyPress] = keypress,
	[MappingNotify] = mappingnotify,
	[MapRequest] = maprequest,
	[MotionNotify] = motionnotify,
	[PropertyNotify] = propertynotify,
	[ResizeRequest] = resizerequest,
	[UnmapNotify] = unmapnotify
};
static Atom wmatom[WMLast], netatom[NetLast], xatom[XLast];
static int restart = 0;
static int running = 1;
static Cur *cursor[CurLast];
static Clr **scheme;
static Display *dpy;
static Drw *drw;
static Monitor *mons, *selmon;
static Window root, wmcheckwin;
static xcb_connection_t *xcon;

/* configuration, allows nested code to access above variables */
#include "config.h"
#include "patch/include.c"

struct Pertag {
	unsigned int curtag, prevtag; /* current and previous tag */
	int nmasters[LENGTH(tags) + 1]; /* number of windows in master area */
	float mfacts[LENGTH(tags) + 1]; /* mfacts per tag */
	#if PERTAG_VANITYGAPS
	int enablegaps[LENGTH(tags) + 1]; /* vanitygaps per tag */
	#endif // PERTAG_VANITYGAPS
	unsigned int sellts[LENGTH(tags) + 1]; /* selected layouts */
	const Layout *ltidxs[LENGTH(tags) + 1][2]; /* matrix of tags and layouts indexes  */
	int showbars[LENGTH(tags) + 1]; /* display bar for the current tag */
};

/* compile-time check if all tags fit into an unsigned int bit array. */
struct NumTags { char limitexceeded[LENGTH(tags) > 31 ? -1 : 1]; };

/* function implementations */
void
applyrules(Client *c)
{
	const char *class, *instance;
	unsigned int i;
	const Rule *r;
	Monitor *m;
	XClassHint ch = { NULL, NULL };

	/* rule matching */
	c->isfloating = 0;
	c->tags = 0;
	c->scratchkey = 0;
	XGetClassHint(dpy, c->win, &ch);
	class    = ch.res_class ? ch.res_class : broken;
	instance = ch.res_name  ? ch.res_name  : broken;

	for (i = 0; i < LENGTH(rules); i++) {
		r = &rules[i];
		if ((!r->title || strstr(c->name, r->title))
		&& (!r->class || strstr(class, r->class))
		&& (!r->instance || strstr(instance, r->instance)))
		{
			c->isterminal = r->isterminal;
			c->noswallow  = r->noswallow;
			c->isfloating = r->isfloating;
			c->tags |= r->tags;
			c->scratchkey = r->scratchkey;
			for (m = mons; m && m->num != r->monitor; m = m->next);
			if (m)
				c->mon = m;
		}
	}
	if (ch.res_class)
		XFree(ch.res_class);
	if (ch.res_name)
		XFree(ch.res_name);
	c->tags = c->tags & TAGMASK ? c->tags & TAGMASK : c->mon->tagset[c->mon->seltags];
}

int
applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact)
{
	int baseismin;
	Monitor *m = c->mon;

	/* set minimum possible */
	*w = MAX(1, *w);
	*h = MAX(1, *h);
	if (interact) {
		if (*x > sw)
			*x = sw - WIDTH(c);
		if (*y > sh)
			*y = sh - HEIGHT(c);
		if (*x + *w + 2 * c->bw < 0)
			*x = 0;
		if (*y + *h + 2 * c->bw < 0)
			*y = 0;
	} else {
		if (*x >= m->wx + m->ww)
			*x = m->wx + m->ww - WIDTH(c);
		if (*y >= m->wy + m->wh)
			*y = m->wy + m->wh - HEIGHT(c);
		if (*x + *w + 2 * c->bw <= m->wx)
			*x = m->wx;
		if (*y + *h + 2 * c->bw <= m->wy)
			*y = m->wy;
	}
	if (*h < bh)
		*h = bh;
	if (*w < bh)
		*w = bh;
	if (resizehints || c->isfloating || !c->mon->lt[c->mon->sellt]->arrange) {
		/* see last two sentences in ICCCM 4.1.2.3 */
		baseismin = c->basew == c->minw && c->baseh == c->minh;
		if (!baseismin) { /* temporarily remove base dimensions */
			*w -= c->basew;
			*h -= c->baseh;
		}
		/* adjust for aspect limits */
		if (c->mina > 0 && c->maxa > 0) {
			if (c->maxa < (float)*w / *h)
				*w = *h * c->maxa + 0.5;
			else if (c->mina < (float)*h / *w)
				*h = *w * c->mina + 0.5;
		}
		if (baseismin) { /* increment calculation requires this */
			*w -= c->basew;
			*h -= c->baseh;
		}
		/* adjust for increment value */
		if (c->incw)
			*w -= *w % c->incw;
		if (c->inch)
			*h -= *h % c->inch;
		/* restore base dimensions */
		*w = MAX(*w + c->basew, c->minw);
		*h = MAX(*h + c->baseh, c->minh);
		if (c->maxw)
			*w = MIN(*w, c->maxw);
		if (c->maxh)
			*h = MIN(*h, c->maxh);
	}
	return *x != c->x || *y != c->y || *w != c->w || *h != c->h;
}

void
arrange(Monitor *m)
{
	if (m)
		showhide(m->stack);
	else for (m = mons; m; m = m->next)
		showhide(m->stack);
	if (m) {
		arrangemon(m);
		restack(m);
	} else for (m = mons; m; m = m->next)
		arrangemon(m);
}

void
arrangemon(Monitor *m)
{
	strncpy(m->ltsymbol, m->lt[m->sellt]->symbol, sizeof m->ltsymbol);
	if (m->lt[m->sellt]->arrange)
		m->lt[m->sellt]->arrange(m);
}

void
attach(Client *c)
{
	c->next = c->mon->clients;
	c->mon->clients = c;
}

void
attachstack(Client *c)
{
	c->snext = c->mon->stack;
	c->mon->stack = c;
}

void
buttonpress(XEvent *e)
{
	int click, i, r, mi;
	Arg arg = {0};
	Client *c;
	Monitor *m;
	Bar *bar;
	XButtonPressedEvent *ev = &e->xbutton;
	const BarRule *br;
	BarClickArg carg = { 0, 0, 0, 0 };

	click = ClkRootWin;
	/* focus monitor if necessary */
	if ((m = wintomon(ev->window)) && m != selmon
	) {
		unfocus(selmon->sel, 1);
		selmon = m;
		focus(NULL);
	}

	for (mi = 0, m = mons; m && m != selmon; m = m->next, mi++); // get the monitor index
	for (bar = selmon->bar; bar; bar = bar->next) {
		if (ev->window == bar->win) {
			for (r = 0; r < LENGTH(barrules); r++) {
				br = &barrules[r];
				if (br->bar != bar->idx || (br->monitor == 'A' && m != selmon) || br->clickfunc == NULL)
					continue;
				if (br->monitor != 'A' && br->monitor != -1 && br->monitor != mi)
					continue;
				if (bar->x[r] <= ev->x && ev->x <= bar->x[r] + bar->w[r]) {
					carg.rel_x = ev->x - bar->x[r];
					carg.rel_y = ev->y;
					carg.rel_w = bar->w[r];
					carg.rel_h = bar->bh;
					click = br->clickfunc(bar, &arg, &carg);
					if (click < 0)
						return;
					break;
				}
			}
			break;
		}
	}

	if (click == ClkRootWin && (c = wintoclient(ev->window))) {
		focus(c);
		restack(selmon);
		XAllowEvents(dpy, ReplayPointer, CurrentTime);
		click = ClkClientWin;
	}

	for (i = 0; i < LENGTH(buttons); i++) {
		if (click == buttons[i].click && buttons[i].func && buttons[i].button == ev->button
				&& CLEANMASK(buttons[i].mask) == CLEANMASK(ev->state)) {
			buttons[i].func(click == ClkTagBar && buttons[i].arg.i == 0 ? &arg : &buttons[i].arg);
		}
	}
}

void
checkotherwm(void)
{
	xerrorxlib = XSetErrorHandler(xerrorstart);
	/* this causes an error if some other window manager is running */
	XSelectInput(dpy, DefaultRootWindow(dpy), SubstructureRedirectMask);
	XSync(dpy, False);
	XSetErrorHandler(xerror);
	XSync(dpy, False);
}

void
cleanup(void)
{
	Arg a = {.ui = ~0};
	Layout foo = { "", NULL };
	Monitor *m;
	size_t i;

	view(&a);
	selmon->lt[selmon->sellt] = &foo;
	for (m = mons; m; m = m->next)
		while (m->stack)
			unmanage(m->stack, 0);
	XUngrabKey(dpy, AnyKey, AnyModifier, root);
	while (mons)
		cleanupmon(mons);
	if (showsystray && systray) {
		while (systray->icons)
			removesystrayicon(systray->icons);
		if (systray->win) {
			XUnmapWindow(dpy, systray->win);
			XDestroyWindow(dpy, systray->win);
		}
		free(systray);
	}
	for (i = 0; i < CurLast; i++)
		drw_cur_free(drw, cursor[i]);
	for (i = 0; i < LENGTH(colors); i++)
		free(scheme[i]);
	XDestroyWindow(dpy, wmcheckwin);
	drw_free(drw);
	XSync(dpy, False);
	XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
	XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
}

void
cleanupmon(Monitor *mon)
{
	Monitor *m;
	Bar *bar;

	if (mon == mons)
		mons = mons->next;
	else {
		for (m = mons; m && m->next != mon; m = m->next);
		m->next = mon->next;
	}
	for (bar = mon->bar; bar; bar = mon->bar) {
		XUnmapWindow(dpy, bar->win);
		XDestroyWindow(dpy, bar->win);
		mon->bar = bar->next;
		free(bar);
	}
	free(mon);
}

void
clientmessage(XEvent *e)
{
	XWindowAttributes wa;
	XSetWindowAttributes swa;
	XClientMessageEvent *cme = &e->xclient;
	Client *c = wintoclient(cme->window);

	if (showsystray && systray && cme->window == systray->win && cme->message_type == netatom[NetSystemTrayOP]) {
		/* add systray icons */
		if (cme->data.l[1] == SYSTEM_TRAY_REQUEST_DOCK) {
			if (!(c = (Client *)calloc(1, sizeof(Client))))
				die("fatal: could not malloc() %u bytes\n", sizeof(Client));
			if (!(c->win = cme->data.l[2])) {
				free(c);
				return;
			}

			c->mon = selmon;
			c->next = systray->icons;
			systray->icons = c;
			XGetWindowAttributes(dpy, c->win, &wa);
			c->x = c->oldx = c->y = c->oldy = 0;
			c->w = c->oldw = wa.width;
			c->h = c->oldh = wa.height;
			c->oldbw = wa.border_width;
			c->bw = 0;
			c->isfloating = True;
			/* reuse tags field as mapped status */
			c->tags = 1;
			updatesizehints(c);
			updatesystrayicongeom(c, wa.width, wa.height);
			XAddToSaveSet(dpy, c->win);
			XSelectInput(dpy, c->win, StructureNotifyMask | PropertyChangeMask | ResizeRedirectMask);
			XReparentWindow(dpy, c->win, systray->win, 0, 0);
			/* use parents background color */
			swa.background_pixel = scheme[SchemeNorm][ColBg].pixel;
			XChangeWindowAttributes(dpy, c->win, CWBackPixel, &swa);
			sendevent(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_EMBEDDED_NOTIFY, 0 , systray->win, XEMBED_EMBEDDED_VERSION);
			XSync(dpy, False);
			setclientstate(c, NormalState);
		}
		return;
	}

	if (!c)
		return;
	if (cme->message_type == netatom[NetWMState]) {
		if (cme->data.l[1] == netatom[NetWMFullscreen]
		|| cme->data.l[2] == netatom[NetWMFullscreen]) {
			if (c->fakefullscreen == 2 && c->isfullscreen)
				c->fakefullscreen = 3;
			setfullscreen(c, (cme->data.l[0] == 1 /* _NET_WM_STATE_ADD    */
				|| (cme->data.l[0] == 2 /* _NET_WM_STATE_TOGGLE */ && !c->isfullscreen)));
		}
	} else if (cme->message_type == netatom[NetActiveWindow]) {
		if (c != selmon->sel && !c->isurgent)
			seturgent(c, 1);
	}
}

void
configure(Client *c)
{
	XConfigureEvent ce;

	ce.type = ConfigureNotify;
	ce.display = dpy;
	ce.event = c->win;
	ce.window = c->win;
	ce.x = c->x;
	ce.y = c->y;
	ce.width = c->w;
	ce.height = c->h;
	ce.border_width = c->bw;
	ce.above = None;
	ce.override_redirect = False;
	XSendEvent(dpy, c->win, False, StructureNotifyMask, (XEvent *)&ce);
}

void
configurenotify(XEvent *e)
{
	Monitor *m;
	Bar *bar;
	Client *c;
	XConfigureEvent *ev = &e->xconfigure;
	int dirty;

	/* TODO: updategeom handling sucks, needs to be simplified */
	if (ev->window == root) {
		dirty = (sw != ev->width || sh != ev->height);
		sw = ev->width;
		sh = ev->height;
		if (updategeom() || dirty) {
			drw_resize(drw, sw, bh);
			updatebars();
			for (m = mons; m; m = m->next) {
				for (c = m->clients; c; c = c->next)
					if (c->isfullscreen && c->fakefullscreen != 1)
						resizeclient(c, m->mx, m->my, m->mw, m->mh);
				for (bar = m->bar; bar; bar = bar->next)
					XMoveResizeWindow(dpy, bar->win, bar->bx, bar->by, bar->bw, bar->bh);
			}
			focus(NULL);
			arrange(NULL);
		}
	}
}

void
configurerequest(XEvent *e)
{
	Client *c;
	Monitor *m;
	XConfigureRequestEvent *ev = &e->xconfigurerequest;
	XWindowChanges wc;

	if ((c = wintoclient(ev->window))) {
		if (ev->value_mask & CWBorderWidth)
			c->bw = ev->border_width;
		else if (c->isfloating || !selmon->lt[selmon->sellt]->arrange) {
			if (c->ignorecfgreqpos && c->ignorecfgreqsize)
				return;

			m = c->mon;
			if (!c->ignorecfgreqpos) {
				if (ev->value_mask & CWX) {
					c->oldx = c->x;
					c->x = m->mx + ev->x;
				}
				if (ev->value_mask & CWY) {
					c->oldy = c->y;
					c->y = m->my + ev->y;
				}
			}
			if (!c->ignorecfgreqsize) {
				if (ev->value_mask & CWWidth) {
					c->oldw = c->w;
					c->w = ev->width;
				}
				if (ev->value_mask & CWHeight) {
					c->oldh = c->h;
					c->h = ev->height;
				}
			}
			if ((c->x + c->w) > m->mx + m->mw && c->isfloating)
				c->x = m->mx + (m->mw / 2 - WIDTH(c) / 2); /* center in x direction */
			if ((c->y + c->h) > m->my + m->mh && c->isfloating)
				c->y = m->my + (m->mh / 2 - HEIGHT(c) / 2); /* center in y direction */
			if ((ev->value_mask & (CWX|CWY)) && !(ev->value_mask & (CWWidth|CWHeight)))
				configure(c);
			if (ISVISIBLE(c))
				XMoveResizeWindow(dpy, c->win, c->x, c->y, c->w, c->h);
		} else
			configure(c);
	} else {
		wc.x = ev->x;
		wc.y = ev->y;
		wc.width = ev->width;
		wc.height = ev->height;
		wc.border_width = ev->border_width;
		wc.sibling = ev->above;
		wc.stack_mode = ev->detail;
		XConfigureWindow(dpy, ev->window, ev->value_mask, &wc);
	}
	XSync(dpy, False);
}

Monitor *
createmon(void)
{
	Monitor *m, *mon;
	int i, n, mi, max_bars = 2, istopbar = topbar;

	const BarRule *br;
	Bar *bar;

	m = ecalloc(1, sizeof(Monitor));
	m->tagset[0] = m->tagset[1] = 1;
	m->mfact = mfact;
	m->nmaster = nmaster;
	m->showbar = showbar;

	for (mi = 0, mon = mons; mon; mon = mon->next, mi++); // monitor index

	m->gappih = gappih;
	m->gappiv = gappiv;
	m->gappoh = gappoh;
	m->gappov = gappov;
	m->lt[0] = &layouts[0];
	m->lt[1] = &layouts[1 % LENGTH(layouts)];
	strncpy(m->ltsymbol, layouts[0].symbol, sizeof m->ltsymbol);
	m->pertag = ecalloc(1, sizeof(Pertag));
	m->pertag->curtag = m->pertag->prevtag = 1;

	for (i = 0; i <= LENGTH(tags); i++) {
		m->pertag->nmasters[i] = m->nmaster;
		m->pertag->mfacts[i] = m->mfact;

		#if PERTAG_VANITYGAPS
		m->pertag->enablegaps[i] = enablegaps;
		#endif // PERTAG_VANITYGAPS

		m->pertag->ltidxs[i][0] = m->lt[0];
		m->pertag->ltidxs[i][1] = m->lt[1];
		m->pertag->sellts[i] = m->sellt;

		m->pertag->showbars[i] = m->showbar;
	}


	/* Derive the number of bars for this monitor based on bar rules */
	for (n = -1, i = 0; i < LENGTH(barrules); i++) {
		br = &barrules[i];
		if (br->monitor == 'A' || br->monitor == -1 || br->monitor == mi)
			n = MAX(br->bar, n);
	}

	for (i = 0; i <= n && i < max_bars; i++) {
		bar = ecalloc(1, sizeof(Bar));
		bar->mon = m;
		bar->idx = i;
		bar->next = m->bar;
		bar->topbar = istopbar;
		m->bar = bar;
		istopbar = !istopbar;
	}

	return m;
}

void
cyclelayouts(const Arg *arg) {
	Layout *l;
	for(l = (Layout *)layouts; l != selmon->lt[selmon->sellt]; l++);
	if(arg->i > 0) {
		if(l->symbol && (l + 1)->symbol)
			setlayout(&((Arg) { .v = (l + 1) }));
		else
			setlayout(&((Arg) { .v = layouts }));
	} else {
		if(l != layouts && (l - 1)->symbol)
			setlayout(&((Arg) { .v = (l - 1) }));
		else
			setlayout(&((Arg) { .v = &layouts[LENGTH(layouts) - 2] }));
	}
}

void
destroynotify(XEvent *e)
{
	Client *c;
	XDestroyWindowEvent *ev = &e->xdestroywindow;

	if ((c = wintoclient(ev->window)))
		unmanage(c, 1);
	else {
		if ((c = swallowingclient(ev->window)))
			unmanage(c->swallowing, 1);
		else if (showsystray && (c = wintosystrayicon(ev->window))) {
			removesystrayicon(c);
			drawbarwin(systray->bar);
		}
	}
}

void
detach(Client *c)
{
	Client **tc;

	for (tc = &c->mon->clients; *tc && *tc != c; tc = &(*tc)->next);
	*tc = c->next;
}

void
detachstack(Client *c)
{
	Client **tc, *t;

	for (tc = &c->mon->stack; *tc && *tc != c; tc = &(*tc)->snext);
	*tc = c->snext;

	if (c == c->mon->sel) {
		for (t = c->mon->stack; t && !ISVISIBLE(t); t = t->snext);
		c->mon->sel = t;
	}
}

Monitor *
dirtomon(int dir)
{
	Monitor *m = NULL;

	if (dir > 0) {
		if (!(m = selmon->next))
			m = mons;
	} else if (selmon == mons)
		for (m = mons; m->next; m = m->next);
	else
		for (m = mons; m->next != selmon; m = m->next);
	return m;
}

void
distributetags(const Arg *arg)
{
        unsigned int ui = 1;
        int i = 0;
        for (Client *c = selmon->clients; c; c = c->next) {
                c->tags = (ui << i) & TAGMASK;
                i = (i + 1) % LENGTH(tags);
        }
        focus(NULL);
        arrange(selmon);
}

void
dragcfact(const Arg *arg)
{
	int prev_x, prev_y, dist_x, dist_y;
	float fact;
	Client *c;
	XEvent ev;
	Time lasttime = 0;

	if (!(c = selmon->sel))
		return;
	if (c->isfloating) {
		resizemouse(arg);
		return;
	}
	#if !FAKEFULLSCREEN_PATCH
	#if FAKEFULLSCREEN_CLIENT_PATCH
	if (c->isfullscreen && !c->fakefullscreen) /* no support resizing fullscreen windows by mouse */
		return;
	#else
	if (c->isfullscreen) /* no support resizing fullscreen windows by mouse */
		return;
	#endif // FAKEFULLSCREEN_CLIENT_PATCH
	#endif // !FAKEFULLSCREEN_PATCH
	restack(selmon);

	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
		None, cursor[CurIronCross]->cursor, CurrentTime) != GrabSuccess)
		return;
	XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w/2, c->h/2);

	prev_x = prev_y = -999999;

	do {
		XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
		switch(ev.type) {
		case ConfigureRequest:
		case Expose:
		case MapRequest:
			handler[ev.type](&ev);
			break;
		case MotionNotify:
			if ((ev.xmotion.time - lasttime) <= (1000 / 60))
				continue;
			lasttime = ev.xmotion.time;
			if (prev_x == -999999) {
				prev_x = ev.xmotion.x_root;
				prev_y = ev.xmotion.y_root;
			}

			dist_x = ev.xmotion.x - prev_x;
			dist_y = ev.xmotion.y - prev_y;

			if (abs(dist_x) > abs(dist_y)) {
				fact = (float) 4.0 * dist_x / c->mon->ww;
			} else {
				fact = (float) -4.0 * dist_y / c->mon->wh;
			}

			if (fact)
				setcfact(&((Arg) { .f = fact }));

			prev_x = ev.xmotion.x;
			prev_y = ev.xmotion.y;
			break;
		}
	} while (ev.type != ButtonRelease);


	XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w/2, c->h/2);

	XUngrabPointer(dpy, CurrentTime);
	while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
}

void
dragmfact(const Arg *arg)
{
	unsigned int n;
	int py, px; // pointer coordinates
	int ax, ay, aw, ah; // area position, width and height
	int center = 0, horizontal = 0, mirror = 0, fixed = 0; // layout configuration
	double fact;
	Monitor *m;
	XEvent ev;
	Time lasttime = 0;

	m = selmon;

	#if VANITYGAPS_PATCH
	int oh, ov, ih, iv;
	getgaps(m, &oh, &ov, &ih, &iv, &n);
	#else
	Client *c;
	for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++);
	#endif // VANITYGAPS_PATCH

	ax = m->wx;
	ay = m->wy;
	ah = m->wh;
	aw = m->ww;

	if (!n)
		return;
	#if FLEXTILE_DELUXE_LAYOUT
	else if (m->lt[m->sellt]->arrange == &flextile) {
		int layout = m->ltaxis[LAYOUT];
		if (layout < 0) {
			mirror = 1;
			layout *= -1;
		}
		if (layout > FLOATING_MASTER) {
			layout -= FLOATING_MASTER;
			fixed = 1;
		}

		if (layout == SPLIT_HORIZONTAL || layout == SPLIT_HORIZONTAL_DUAL_STACK)
			horizontal = 1;
		else if (layout == SPLIT_CENTERED_VERTICAL && (fixed || n - m->nmaster > 1))
			center = 1;
		else if (layout == FLOATING_MASTER) {
			center = 1;
			if (aw < ah)
				horizontal = 1;
		}
		else if (layout == SPLIT_CENTERED_HORIZONTAL) {
			if (fixed || n - m->nmaster > 1)
				center = 1;
			horizontal = 1;
		}
	}
	#endif // FLEXTILE_DELUXE_LAYOUT
	#if CENTEREDMASTER_LAYOUT
	else if (m->lt[m->sellt]->arrange == &centeredmaster && (fixed || n - m->nmaster > 1))
		center = 1;
	#endif // CENTEREDMASTER_LAYOUT
	#if CENTEREDFLOATINGMASTER_LAYOUT
	else if (m->lt[m->sellt]->arrange == &centeredfloatingmaster)
		center = 1;
	#endif // CENTEREDFLOATINGMASTER_LAYOUT
	#if BSTACK_LAYOUT
	else if (m->lt[m->sellt]->arrange == &bstack)
		horizontal = 1;
	#endif // BSTACK_LAYOUT
	#if BSTACKHORIZ_LAYOUT
	else if (m->lt[m->sellt]->arrange == &bstackhoriz)
		horizontal = 1;
	#endif // BSTACKHORIZ_LAYOUT

	/* do not allow mfact to be modified under certain conditions */
	if (!m->lt[m->sellt]->arrange                            // floating layout
		|| (!fixed && m->nmaster && n <= m->nmaster)         // no master
		#if MONOCLE_LAYOUT
		|| m->lt[m->sellt]->arrange == &monocle
		#endif // MONOCLE_LAYOUT
		#if GRIDMODE_LAYOUT
		|| m->lt[m->sellt]->arrange == &grid
		#endif // GRIDMODE_LAYOUT
		#if HORIZGRID_LAYOUT
		|| m->lt[m->sellt]->arrange == &horizgrid
		#endif // HORIZGRID_LAYOUT
		#if GAPPLESSGRID_LAYOUT
		|| m->lt[m->sellt]->arrange == &gaplessgrid
		#endif // GAPPLESSGRID_LAYOUT
		#if NROWGRID_LAYOUT
		|| m->lt[m->sellt]->arrange == &nrowgrid
		#endif // NROWGRID_LAYOUT
		#if FLEXTILE_DELUXE_LAYOUT
		|| (m->lt[m->sellt]->arrange == &flextile && m->ltaxis[LAYOUT] == NO_SPLIT)
		#endif // FLEXTILE_DELUXE_LAYOUT
	)
		return;

	#if VANITYGAPS_PATCH
	ay += oh;
	ax += ov;
	aw -= 2*ov;
	ah -= 2*oh;
	#endif // VANITYGAPS_PATCH

	if (center) {
		if (horizontal) {
			px = ax + aw / 2;
			#if VANITYGAPS_PATCH
			py = ay + ah / 2 + (ah - 2*ih) * (m->mfact / 2.0) + ih / 2;
			#else
			py = ay + ah / 2 + ah * m->mfact / 2.0;
			#endif // VANITYGAPS_PATCH
		} else { // vertical split
			#if VANITYGAPS_PATCH
			px = ax + aw / 2 + (aw - 2*iv) * m->mfact / 2.0 + iv / 2;
			#else
			px = ax + aw / 2 + aw * m->mfact / 2.0;
			#endif // VANITYGAPS_PATCH
			py = ay + ah / 2;
		}
	} else if (horizontal) {
		px = ax + aw / 2;
		if (mirror)
			#if VANITYGAPS_PATCH
			py = ay + (ah - ih) * (1.0 - m->mfact) + ih / 2;
			#else
			py = ay + (ah * (1.0 - m->mfact));
			#endif // VANITYGAPS_PATCH
		else
			#if VANITYGAPS_PATCH
			py = ay + ((ah - ih) * m->mfact) + ih / 2;
			#else
			py = ay + (ah * m->mfact);
			#endif // VANITYGAPS_PATCH
	} else { // vertical split
		if (mirror)
			#if VANITYGAPS_PATCH
			px = ax + (aw - iv) * (1.0 - m->mfact) + iv / 2;
			#else
			px = ax + (aw * m->mfact);
			#endif // VANITYGAPS_PATCH
		else
			#if VANITYGAPS_PATCH
			px = ax + ((aw - iv) * m->mfact) + iv / 2;
			#else
			px = ax + (aw * m->mfact);
			#endif // VANITYGAPS_PATCH
		py = ay + ah / 2;
	}

	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
		None, cursor[horizontal ? CurResizeVertArrow : CurResizeHorzArrow]->cursor, CurrentTime) != GrabSuccess)
		return;
	XWarpPointer(dpy, None, root, 0, 0, 0, 0, px, py);

	do {
		XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
		switch(ev.type) {
		case ConfigureRequest:
		case Expose:
		case MapRequest:
			handler[ev.type](&ev);
			break;
		case MotionNotify:
			if ((ev.xmotion.time - lasttime) <= (1000 / 40))
				continue;
			if (lasttime != 0) {
				px = ev.xmotion.x;
				py = ev.xmotion.y;
			}
			lasttime = ev.xmotion.time;

			#if VANITYGAPS_PATCH
			if (center)
				if (horizontal)
					if (py - ay > ah / 2)
						fact = (double) 1.0 - (ay + ah - py - ih / 2) * 2 / (double) (ah - 2*ih);
					else
						fact = (double) 1.0 - (py - ay - ih / 2) * 2 / (double) (ah - 2*ih);
				else
					if (px - ax > aw / 2)
						fact = (double) 1.0 - (ax + aw - px - iv / 2) * 2 / (double) (aw - 2*iv);
					else
						fact = (double) 1.0 - (px - ax - iv / 2) * 2 / (double) (aw - 2*iv);
			else
				if (horizontal)
					fact = (double) (py - ay - ih / 2) / (double) (ah - ih);
				else
					fact = (double) (px - ax - iv / 2) / (double) (aw - iv);
			#else
			if (center)
				if (horizontal)
					if (py - ay > ah / 2)
						fact = (double) 1.0 - (ay + ah - py) * 2 / (double) ah;
					else
						fact = (double) 1.0 - (py - ay) * 2 / (double) ah;
				else
					if (px - ax > aw / 2)
						fact = (double) 1.0 - (ax + aw - px) * 2 / (double) aw;
					else
						fact = (double) 1.0 - (px - ax) * 2 / (double) aw;
			else
				if (horizontal)
					fact = (double) (py - ay) / (double) ah;
				else
					fact = (double) (px - ax) / (double) aw;
			#endif // VANITYGAPS_PATCH

			if (!center && mirror)
				fact = 1.0 - fact;

			setmfact(&((Arg) { .f = 1.0 + fact }));
			px = ev.xmotion.x;
			py = ev.xmotion.y;
			break;
		}
	} while (ev.type != ButtonRelease);

	XUngrabPointer(dpy, CurrentTime);
	while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
}

void
drawbar(Monitor *m)
{
	Bar *bar;
	for (bar = m->bar; bar; bar = bar->next)
		drawbarwin(bar);
}

void
drawbars(void)
{
	Monitor *m;

	for (m = mons; m; m = m->next)
		drawbar(m);
}

void
drawbarwin(Bar *bar)
{
	if (!bar->win)
		return;
	Monitor *mon;
	int r, w, mi;
	int rx, lx, rw, lw; // bar size, split between left and right if a center module is added
	const BarRule *br;
	BarWidthArg warg = { 0 };
	BarDrawArg darg  = { 0, 0 };

	for (mi = 0, mon = mons; mon && mon != bar->mon; mon = mon->next, mi++); // get the monitor index
	rw = lw = bar->bw;
	rx = lx = 0;

	drw_setscheme(drw, scheme[SchemeNorm]);
	drw_rect(drw, lx, 0, lw, bh, 1, 1);
	for (r = 0; r < LENGTH(barrules); r++) {
		br = &barrules[r];
		if (br->bar != bar->idx || br->drawfunc == NULL || (br->monitor == 'A' && bar->mon != selmon))
			continue;
		if (br->monitor != 'A' && br->monitor != -1 && br->monitor != mi)
			continue;
		drw_setscheme(drw, scheme[SchemeNorm]);
		warg.max_width = (br->alignment < BAR_ALIGN_RIGHT_LEFT ? lw : rw);
		w = br->widthfunc(bar, &warg);
		w = MIN(warg.max_width, w);

		if (lw <= 0) { // if left is exhausted then switch to right side, and vice versa
			lw = rw;
			lx = rx;
		} else if (rw <= 0) {
			rw = lw;
			rx = lx;
		}

		switch(br->alignment) {
		default:
		case BAR_ALIGN_NONE:
		case BAR_ALIGN_LEFT_LEFT:
		case BAR_ALIGN_LEFT:
			bar->x[r] = lx;
			if (lx == rx) {
				rx += w;
				rw -= w;
			}
			lx += w;
			lw -= w;
			break;
		case BAR_ALIGN_LEFT_RIGHT:
		case BAR_ALIGN_RIGHT:
			bar->x[r] = lx + lw - w;
			if (lx == rx)
				rw -= w;
			lw -= w;
			break;
		case BAR_ALIGN_LEFT_CENTER:
		case BAR_ALIGN_CENTER:
			bar->x[r] = lx + lw / 2 - w / 2;
			if (lx == rx) {
				rw = rx + rw - bar->x[r] - w;
				rx = bar->x[r] + w;
			}
			lw = bar->x[r] - lx;
			break;
		case BAR_ALIGN_RIGHT_LEFT:
			bar->x[r] = rx;
			if (lx == rx) {
				lx += w;
				lw -= w;
			}
			rx += w;
			rw -= w;
			break;
		case BAR_ALIGN_RIGHT_RIGHT:
			bar->x[r] = rx + rw - w;
			if (lx == rx)
				lw -= w;
			rw -= w;
			break;
		case BAR_ALIGN_RIGHT_CENTER:
			bar->x[r] = rx + rw / 2 - w / 2;
			if (lx == rx) {
				lw = lx + lw - bar->x[r] + w;
				lx = bar->x[r] + w;
			}
			rw = bar->x[r] - rx;
			break;
		}
		bar->w[r] = w;
		darg.x = bar->x[r];
		darg.w = bar->w[r];
		br->drawfunc(bar, &darg);
	}
	drw_map(drw, bar->win, 0, 0, bar->bw, bar->bh);
}

void
enqueue(Client *c)
{
	Client *l;
	for (l = c->mon->clients; l && l->next; l = l->next);
	if (l) {
		l->next = c;
		c->next = NULL;
	}
}

void
enqueuestack(Client *c)
{
	Client *l;
	for (l = c->mon->stack; l && l->snext; l = l->snext);
	if (l) {
		l->snext = c;
		c->snext = NULL;
	}
}

void
enternotify(XEvent *e)
{
	Client *c;
	Monitor *m;
	XCrossingEvent *ev = &e->xcrossing;

	if ((ev->mode != NotifyNormal || ev->detail == NotifyInferior) && ev->window != root)
		return;
	c = wintoclient(ev->window);
	m = c ? c->mon : wintomon(ev->window);
	if (m != selmon) {
		unfocus(selmon->sel, 1);
		selmon = m;
	} else if (!c || c == selmon->sel)
		return;
	focus(c);
}

void
expose(XEvent *e)
{
	Monitor *m;
	XExposeEvent *ev = &e->xexpose;

	if (ev->count == 0 && (m = wintomon(ev->window)))
		drawbar(m);
}

void
focus(Client *c)
{
	if (!c || !ISVISIBLE(c))
		for (c = selmon->stack; c && !ISVISIBLE(c); c = c->snext);
	if (selmon->sel && selmon->sel != c) {
		losefullscreen(c);
		unfocus(selmon->sel, 0);
	}
	if (c) {
		if (c->mon != selmon)
			selmon = c->mon;
		if (c->isurgent)
			seturgent(c, 0);
		detachstack(c);
		attachstack(c);
		grabbuttons(c, 1);
		XSetWindowBorder(dpy, c->win, scheme[SchemeSel][ColBorder].pixel);
		setfocus(c);
	} else {
		XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
		XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
	}
	selmon->sel = c;
	drawbars();
}

/* there are some broken focus acquiring clients needing extra handling */
void
focusin(XEvent *e)
{
	XFocusChangeEvent *ev = &e->xfocus;

	if (selmon->sel && ev->window != selmon->sel->win)
		setfocus(selmon->sel);
}

void
focusmon(const Arg *arg)
{
	Monitor *m;

	if (!mons->next)
		return;
	if ((m = dirtomon(arg->i)) == selmon)
		return;
	unfocus(selmon->sel, 0);
	selmon = m;
	focus(NULL);
}

void
focusstack(const Arg *arg)
{
	Client *c = NULL, *i;

	/* Note that this patch is made for dwm 6.2. If applying this on top of the latest master
	 * then the https://dwm.suckless.org/patches/alwaysfullscreen/ patch has been merged
	 * upstream, which prevents focus to drift from windows that are in fake fullscreen.
	 * To address this issue use this if statement instead:
	 *   if (!selmon->sel || (selmon->sel->isfullscreen && selmon->sel->fakefullscreen != 1))
	 */
	if (!selmon->sel)
		return;
	if (arg->i > 0) {
		for (c = selmon->sel->next; c && !ISVISIBLE(c); c = c->next);
		if (!c)
			for (c = selmon->clients; c && !ISVISIBLE(c); c = c->next);
	} else {
		for (i = selmon->clients; i != selmon->sel; i = i->next)
			if (ISVISIBLE(i))
				c = i;
		if (!c)
			for (; i; i = i->next)
				if (ISVISIBLE(i))
					c = i;
	}
	if (c) {
		focus(c);
		restack(selmon);
	}
}

static void
focusurgent(const Arg *arg) {
	Client *c;
	int i;
	for(c=selmon->clients; c && !c->isurgent; c=c->next);
	if(c) {
		for(i=0; i < LENGTH(tags) && !((1 << i) & c->tags); i++);
		if(i < LENGTH(tags)) {
			const Arg a = {.ui = 1 << i};
			view(&a);
			focus(c);
		}
	}
}

Atom
getatomprop(Client *c, Atom prop)
{
	int di;
	unsigned long dl;
	unsigned char *p = NULL;
	Atom da, atom = None;

	Atom req = XA_ATOM;
	if (prop == xatom[XembedInfo])
		req = xatom[XembedInfo];

	if (XGetWindowProperty(dpy, c->win, prop, 0L, sizeof atom, False, req,
		&da, &di, &dl, &dl, &p) == Success && p) {
		atom = *(Atom *)p;
		if (da == xatom[XembedInfo] && dl == 2)
			atom = ((Atom *)p)[1];
		XFree(p);
	}
	return atom;
}

void
getgaps(Monitor *m, int *oh, int *ov, int *ih, int *iv, unsigned int *nc)
{
	unsigned int n, oe, ie;
	#if PERTAG_VANITYGAPS
	oe = ie = selmon->pertag->enablegaps[selmon->pertag->curtag];
	#else
	oe = ie = enablegaps;
	#endif // PERTAG_VANITYGAPS
	Client *c;

	for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++);
	if (smartgaps && n == 1) {
		oe = 0; // outer gaps disabled when only one client
	}

	*oh = m->gappoh*oe; // outer horizontal gap
	*ov = m->gappov*oe; // outer vertical gap
	*ih = m->gappih*ie; // inner horizontal gap
	*iv = m->gappiv*ie; // inner vertical gap
	*nc = n;            // number of clients
}

pid_t
getparentprocess(pid_t p)
{
	unsigned int v = 0;

#ifdef __linux__
	FILE *f;
	char buf[256];
	snprintf(buf, sizeof(buf) - 1, "/proc/%u/stat", (unsigned)p);

	if (!(f = fopen(buf, "r")))
		return 0;

	fscanf(f, "%*u %*s %*c %u", &v);
	fclose(f);
#endif /* __linux__*/

#ifdef __OpenBSD__
	int n;
	kvm_t *kd;
	struct kinfo_proc *kp;

	kd = kvm_openfiles(NULL, NULL, NULL, KVM_NO_FILES, NULL);
	if (!kd)
		return 0;

	kp = kvm_getprocs(kd, KERN_PROC_PID, p, sizeof(*kp), &n);
	v = kp->p_ppid;
#endif /* __OpenBSD__ */

	return (pid_t)v;
}

int
getrootptr(int *x, int *y)
{
	int di;
	unsigned int dui;
	Window dummy;

	return XQueryPointer(dpy, root, &dummy, &dummy, x, y, &di, &di, &dui);
}

long
getstate(Window w)
{
	int format;
	long result = -1;
	unsigned char *p = NULL;
	unsigned long n, extra;
	Atom real;

	if (XGetWindowProperty(dpy, w, wmatom[WMState], 0L, 2L, False, wmatom[WMState],
		&real, &format, &n, &extra, (unsigned char **)&p) != Success)
		return -1;
	if (n != 0)
		result = *p;
	XFree(p);
	return result;
}

int
gettextprop(Window w, Atom atom, char *text, unsigned int size)
{
	char **list = NULL;
	int n;
	XTextProperty name;

	if (!text || size == 0)
		return 0;
	text[0] = '\0';
	if (!XGetTextProperty(dpy, w, &name, atom) || !name.nitems)
		return 0;
	if (name.encoding == XA_STRING)
		strncpy(text, (char *)name.value, size - 1);
	else {
		if (XmbTextPropertyToTextList(dpy, &name, &list, &n) >= Success && n > 0 && *list) {
			strncpy(text, *list, size - 1);
			XFreeStringList(list);
		}
	}
	text[size - 1] = '\0';
	XFree(name.value);
	return 1;
}

void
grabbuttons(Client *c, int focused)
{
	updatenumlockmask();
	{
		unsigned int i, j;
		unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
		XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
		if (!focused)
			XGrabButton(dpy, AnyButton, AnyModifier, c->win, False,
				BUTTONMASK, GrabModeSync, GrabModeSync, None, None);
		for (i = 0; i < LENGTH(buttons); i++)
			if (buttons[i].click == ClkClientWin)
				for (j = 0; j < LENGTH(modifiers); j++)
					XGrabButton(dpy, buttons[i].button,
						buttons[i].mask | modifiers[j],
						c->win, False, BUTTONMASK,
						GrabModeAsync, GrabModeSync, None, None);
	}
}

void
grabkeys(void)
{
	updatenumlockmask();
	{
		unsigned int i, j;
		unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
		KeyCode code;

		XUngrabKey(dpy, AnyKey, AnyModifier, root);
		for (i = 0; i < LENGTH(keys); i++)
			if ((code = XKeysymToKeycode(dpy, keys[i].keysym)))
				for (j = 0; j < LENGTH(modifiers); j++)
					XGrabKey(dpy, code, keys[i].mod | modifiers[j], root,
						True, GrabModeAsync, GrabModeAsync);
	}
}

int
isdescprocess(pid_t p, pid_t c)
{
	while (p != c && c != 0)
		c = getparentprocess(c);

	return (int)c;
}

void
incnmaster(const Arg *arg)
{
	selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag] = MAX(selmon->nmaster + arg->i, 0);
	arrange(selmon);
}

#ifdef XINERAMA
static int
isuniquegeom(XineramaScreenInfo *unique, size_t n, XineramaScreenInfo *info)
{
	while (n--)
		if (unique[n].x_org == info->x_org && unique[n].y_org == info->y_org
		&& unique[n].width == info->width && unique[n].height == info->height)
			return 0;
	return 1;
}
#endif /* XINERAMA */

void
keypress(XEvent *e)
{
	unsigned int i;
	KeySym keysym;
	XKeyEvent *ev;

	ev = &e->xkey;
	keysym = XKeycodeToKeysym(dpy, (KeyCode)ev->keycode, 0);
	for (i = 0; i < LENGTH(keys); i++)
		if (keysym == keys[i].keysym
		&& CLEANMASK(keys[i].mod) == CLEANMASK(ev->state)
		&& keys[i].func)
			keys[i].func(&(keys[i].arg));
}

void
killclient(const Arg *arg)
{
	if (!selmon->sel)
		return;
	if (!sendevent(selmon->sel->win, wmatom[WMDelete], NoEventMask, wmatom[WMDelete], CurrentTime, 0, 0, 0)) {
		XGrabServer(dpy);
		XSetErrorHandler(xerrordummy);
		XSetCloseDownMode(dpy, DestroyAll);
		XKillClient(dpy, selmon->sel->win);
		XSync(dpy, False);
		XSetErrorHandler(xerror);
		XUngrabServer(dpy);
	}
}

void
loadxresources(void)
{
	Display *display;
	char *resm;
	XrmDatabase db;
	ResourcePref *p;

	display = XOpenDisplay(NULL);
	resm = XResourceManagerString(display);
	if (!resm)
		return;

	db = XrmGetStringDatabase(resm);
	for (p = resources; p < resources + LENGTH(resources); p++)
		resourceload(db, p->name, p->type, p->dst);
	XCloseDisplay(display);
}

void
losefullscreen(Client *next)
{
	Client *sel = selmon->sel;
	if (!sel || !next)
		return;
	if (sel->isfullscreen && sel->fakefullscreen != 1 && ISVISIBLE(sel) && sel->mon == next->mon && !next->isfloating)
		setfullscreen(sel, 0);
}

void
manage(Window w, XWindowAttributes *wa)
{
	Client *c, *t = NULL, *term = NULL;
	Window trans = None;
	XWindowChanges wc;
	int focusclient = 1;

	c = ecalloc(1, sizeof(Client));
	c->win = w;
	c->pid = winpid(w);
	/* geometry */
	c->x = c->oldx = wa->x;
	c->y = c->oldy = wa->y;
	c->w = c->oldw = wa->width;
	c->h = c->oldh = wa->height;
	c->oldbw = wa->border_width;
	c->cfact = 1.0;

	updatetitle(c);
	if (XGetTransientForHint(dpy, w, &trans) && (t = wintoclient(trans))) {
		c->mon = t->mon;
		c->tags = t->tags;
	} else {
		c->mon = selmon;
		applyrules(c);
		term = termforwin(c);
	}

	if (c->x + WIDTH(c) > c->mon->mx + c->mon->mw)
		c->x = c->mon->mx + c->mon->mw - WIDTH(c);
	if (c->y + HEIGHT(c) > c->mon->my + c->mon->mh)
		c->y = c->mon->my + c->mon->mh - HEIGHT(c);
	c->x = MAX(c->x, c->mon->mx);
	/* only fix client y-offset, if the client center might cover the bar */
	c->y = MAX(c->y, ((c->mon->bar->by == c->mon->my) && (c->x + (c->w / 2) >= c->mon->wx)
		&& (c->x + (c->w / 2) < c->mon->wx + c->mon->ww)) ? bh : c->mon->my);
	c->bw = borderpx;

	wc.border_width = c->bw;
	XConfigureWindow(dpy, w, CWBorderWidth, &wc);
	XSetWindowBorder(dpy, w, scheme[SchemeNorm][ColBorder].pixel);
	configure(c); /* propagates border_width, if size doesn't change */
	updatewindowtype(c);
	updatesizehints(c);
	updatewmhints(c);
	c->x = c->mon->mx + (c->mon->mw - WIDTH(c)) / 2;
	c->y = c->mon->my + (bh + c->mon->mh - HEIGHT(c)) / 2;
	c->sfx = c->x;
	c->sfy = c->y;
	c->sfw = c->w;
	c->sfh = c->h;
	XSelectInput(dpy, w, EnterWindowMask|FocusChangeMask|PropertyChangeMask|StructureNotifyMask);
	grabbuttons(c, 0);
	if (!c->isfloating)
		c->isfloating = c->oldstate = trans != None || c->isfixed;
	if (c->isfloating)
		XRaiseWindow(dpy, c->win);

	/* Do not attach client if it is being swallowed */
	if (term && swallow(term, c)) {
		/* Do not let swallowed client steal focus unless the terminal has focus */
		focusclient = (term == selmon->sel);
	} else {
		attach(c);

		if (focusclient || !c->mon->sel || !c->mon->stack)
			attachstack(c);
		else {
			c->snext = c->mon->sel->snext;
			c->mon->sel->snext = c;
		}
	}

	XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32, PropModeAppend,
		(unsigned char *) &(c->win), 1);
	XMoveResizeWindow(dpy, c->win, c->x + 2 * sw, c->y, c->w, c->h); /* some windows require this */
	setclientstate(c, NormalState);
	if (focusclient) {
		if (c->mon == selmon) {
			unfocus(selmon->sel, 0);
			losefullscreen(c);
		}
		c->mon->sel = c;
	}
	arrange(c->mon);
	XMapWindow(dpy, c->win);
	if (focusclient)
		focus(NULL);
}

void
mappingnotify(XEvent *e)
{
	XMappingEvent *ev = &e->xmapping;

	XRefreshKeyboardMapping(ev);
	if (ev->request == MappingKeyboard)
		grabkeys();
}

void
maprequest(XEvent *e)
{
	Client *i;
	static XWindowAttributes wa;
	XMapRequestEvent *ev = &e->xmaprequest;

	if (showsystray && systray && (i = wintosystrayicon(ev->window))) {
		sendevent(i->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_WINDOW_ACTIVATE, 0, systray->win, XEMBED_EMBEDDED_VERSION);
		drawbarwin(systray->bar);
	}

	if (!XGetWindowAttributes(dpy, ev->window, &wa))
		return;
	if (wa.override_redirect)
		return;
	if (!wintoclient(ev->window))
		manage(ev->window, &wa);
}

void
monocle(Monitor *m)
{
	unsigned int n = 0;
	Client *c;

	for (c = m->clients; c; c = c->next)
		if (ISVISIBLE(c))
			n++;
	if (n > 0) /* override layout symbol */
		snprintf(m->ltsymbol, sizeof m->ltsymbol, "[%d]", n);
	for (c = nexttiled(m->clients); c; c = nexttiled(c->next))
		resize(c, m->wx, m->wy, m->ww - 2 * c->bw, m->wh - 2 * c->bw, 0);
}

void
motionnotify(XEvent *e)
{
	static Monitor *mon = NULL;
	Monitor *m;
	XMotionEvent *ev = &e->xmotion;

	if (ev->window != root)
		return;
	if ((m = recttomon(ev->x_root, ev->y_root, 1, 1)) != mon && mon) {
		unfocus(selmon->sel, 1);
		selmon = m;
		focus(NULL);
	}
	mon = m;
}

void
movemouse(const Arg *arg)
{
	int x, y, ocx, ocy, nx, ny;
	Client *c;
	Monitor *m;
	XEvent ev;
	Time lasttime = 0;

	if (!(c = selmon->sel))
		return;
	if (c->isfullscreen && c->fakefullscreen != 1) /* no support moving fullscreen windows by mouse */
		return;
	restack(selmon);
	ocx = c->x;
	ocy = c->y;
	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
		None, cursor[CurMove]->cursor, CurrentTime) != GrabSuccess)
		return;
	if (!getrootptr(&x, &y))
		return;
	do {
		XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
		switch(ev.type) {
		case ConfigureRequest:
		case Expose:
		case MapRequest:
			handler[ev.type](&ev);
			break;
		case MotionNotify:
			if ((ev.xmotion.time - lasttime) <= (1000 / 60))
				continue;
			lasttime = ev.xmotion.time;

			nx = ocx + (ev.xmotion.x - x);
			ny = ocy + (ev.xmotion.y - y);
			if (abs(selmon->wx - nx) < snap)
				nx = selmon->wx;
			else if (abs((selmon->wx + selmon->ww) - (nx + WIDTH(c))) < snap)
				nx = selmon->wx + selmon->ww - WIDTH(c);
			if (abs(selmon->wy - ny) < snap)
				ny = selmon->wy;
			else if (abs((selmon->wy + selmon->wh) - (ny + HEIGHT(c))) < snap)
				ny = selmon->wy + selmon->wh - HEIGHT(c);
			if (!c->isfloating && selmon->lt[selmon->sellt]->arrange
			&& (abs(nx - c->x) > snap || abs(ny - c->y) > snap))
				togglefloating(NULL);
			if (!selmon->lt[selmon->sellt]->arrange || c->isfloating)
				resize(c, nx, ny, c->w, c->h, 1);
			break;
		}
	} while (ev.type != ButtonRelease);
	XUngrabPointer(dpy, CurrentTime);
	if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
		sendmon(c, m);
		selmon = m;
		focus(NULL);
	}
}

void
moveorplace(const Arg *arg) {
	if ((!selmon->lt[selmon->sellt]->arrange || (selmon->sel && selmon->sel->isfloating)))
		movemouse(arg);
	else
		placemouse(arg);
}

Client *
nexttiled(Client *c)
{
	for (; c && (c->isfloating || !ISVISIBLE(c)); c = c->next);
	return c;
}

void
placemouse(const Arg *arg)
{
	int x, y, px, py, ocx, ocy, nx = -9999, ny = -9999, freemove = 0;
	Client *c, *r = NULL, *at, *prevr;
	Monitor *m;
	XEvent ev;
	XWindowAttributes wa;
	Time lasttime = 0;
	int attachmode, prevattachmode;
	attachmode = prevattachmode = -1;

	if (!(c = selmon->sel) || !c->mon->lt[c->mon->sellt]->arrange) /* no support for placemouse when floating layout is used */
		return;
	if (c->isfullscreen) /* no support placing fullscreen windows by mouse */
		return;
	restack(selmon);
	prevr = c;
	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
		None, cursor[CurMove]->cursor, CurrentTime) != GrabSuccess)
		return;

	c->isfloating = 0;
	c->beingmoved = 1;

	XGetWindowAttributes(dpy, c->win, &wa);
	ocx = wa.x;
	ocy = wa.y;

	if (arg->i == 2) // warp cursor to client center
		XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, WIDTH(c) / 2, HEIGHT(c) / 2);

	if (!getrootptr(&x, &y))
		return;

	do {
		XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
		switch (ev.type) {
		case ConfigureRequest:
		case Expose:
		case MapRequest:
			handler[ev.type](&ev);
			break;
		case MotionNotify:
			if ((ev.xmotion.time - lasttime) <= (1000 / 60))
				continue;
			lasttime = ev.xmotion.time;

			nx = ocx + (ev.xmotion.x - x);
			ny = ocy + (ev.xmotion.y - y);

			if (!freemove && (abs(nx - ocx) > snap || abs(ny - ocy) > snap))
				freemove = 1;

			if (freemove)
				XMoveWindow(dpy, c->win, nx, ny);

			if ((m = recttomon(ev.xmotion.x, ev.xmotion.y, 1, 1)) && m != selmon)
				selmon = m;

			if (arg->i == 1) { // tiled position is relative to the client window center point
				px = nx + wa.width / 2;
				py = ny + wa.height / 2;
			} else { // tiled position is relative to the mouse cursor
				px = ev.xmotion.x;
				py = ev.xmotion.y;
			}

			r = recttoclient(px, py, 1, 1);

			if (!r || r == c)
				break;

			attachmode = 0; // below
			if (((float)(r->y + r->h - py) / r->h) > ((float)(r->x + r->w - px) / r->w)) {
				if (abs(r->y - py) < r->h / 2)
					attachmode = 1; // above
			} else if (abs(r->x - px) < r->w / 2)
				attachmode = 1; // above

			if ((r && r != prevr) || (attachmode != prevattachmode)) {
				detachstack(c);
				detach(c);
				if (c->mon != r->mon) {
					arrangemon(c->mon);
					c->tags = r->mon->tagset[r->mon->seltags];
				}

				c->mon = r->mon;
				r->mon->sel = r;

				if (attachmode) {
					if (r == r->mon->clients)
						attach(c);
					else {
						for (at = r->mon->clients; at->next != r; at = at->next);
						c->next = at->next;
						at->next = c;
					}
				} else {
					c->next = r->next;
					r->next = c;
				}

				attachstack(c);
				arrangemon(r->mon);
				prevr = r;
				prevattachmode = attachmode;
			}
			break;
		}
	} while (ev.type != ButtonRelease);
	XUngrabPointer(dpy, CurrentTime);

	if ((m = recttomon(ev.xmotion.x, ev.xmotion.y, 1, 1)) && m != c->mon) {
		detach(c);
		detachstack(c);
		arrangemon(c->mon);
		c->mon = m;
		c->tags = m->tagset[m->seltags];
		attach(c);
		attachstack(c);
		selmon = m;
	}

	focus(c);
	c->beingmoved = 0;

	if (nx != -9999)
		resize(c, nx, ny, c->w, c->h, 0);
	arrangemon(c->mon);
}

void
pop(Client *c)
{
	detach(c);
	attach(c);
	focus(c);
	arrange(c->mon);
}

void
propertynotify(XEvent *e)
{
	Client *c;
	Window trans;
	XPropertyEvent *ev = &e->xproperty;

	if (showsystray && (c = wintosystrayicon(ev->window))) {
		if (ev->atom == XA_WM_NORMAL_HINTS) {
			updatesizehints(c);
			updatesystrayicongeom(c, c->w, c->h);
		}
		else
			updatesystrayiconstate(c, ev);
		drawbarwin(systray->bar);
	}

	if ((ev->window == root) && (ev->atom == XA_WM_NAME))
		updatestatus();
	else if (ev->state == PropertyDelete)
		return; /* ignore */
	else if ((c = wintoclient(ev->window))) {
		switch(ev->atom) {
		default: break;
		case XA_WM_TRANSIENT_FOR:
			if (!c->isfloating && (XGetTransientForHint(dpy, c->win, &trans)) &&
				(c->isfloating = (wintoclient(trans)) != NULL))
				arrange(c->mon);
			break;
		case XA_WM_NORMAL_HINTS:
			updatesizehints(c);
			break;
		case XA_WM_HINTS:
			updatewmhints(c);
			if (c->isurgent)
				drawbars();
			break;
		}
		if (ev->atom == XA_WM_NAME || ev->atom == netatom[NetWMName]) {
			updatetitle(c);
			if (c == c->mon->sel)
				drawbar(c->mon);
		}
		if (ev->atom == netatom[NetWMWindowType])
			updatewindowtype(c);
	}
}

void
quit(const Arg *arg)
{
	if(arg->i) restart = 1;
	running = 0;
}

Client *
recttoclient(int x, int y, int w, int h)
{
	Client *c, *r = NULL;
	int a, area = 0;

	for (c = nexttiled(selmon->clients); c; c = nexttiled(c->next)) {
		if ((a = INTERSECTC(x, y, w, h, c)) > area) {
			area = a;
			r = c;
		}
	}
	return r;
}

Monitor *
recttomon(int x, int y, int w, int h)
{
	Monitor *m, *r = selmon;
	int a, area = 0;

	for (m = mons; m; m = m->next)
		if ((a = INTERSECT(x, y, w, h, m)) > area) {
			area = a;
			r = m;
		}
	return r;
}

void
removescratch(const Arg *arg)
{
	Client *c = selmon->sel;
	if (!c)
		return;
	c->scratchkey = 0;
}

void
reorganizetags(const Arg *arg) {
	Client *c;
	unsigned int occ, unocc, i;
	unsigned int tagdest[LENGTH(tags)];

	occ = 0;
	for (c = selmon->clients; c; c = c->next)
		occ |= (1 << (ffs(c->tags)-1));
	unocc = 0;
	for (i = 0; i < LENGTH(tags); ++i) {
		while (unocc < i && (occ & (1 << unocc)))
			unocc++;
		if (occ & (1 << i)) {
			tagdest[i] = unocc;
			occ &= ~(1 << i);
			occ |= 1 << unocc;
		}
	}

	for (c = selmon->clients; c; c = c->next)
		c->tags = 1 << tagdest[ffs(c->tags)-1];
	if (selmon->sel)
		selmon->tagset[selmon->seltags] = selmon->sel->tags;
	arrange(selmon);
}

void
replaceclient(Client *old, Client *new)
{
	Client *c = NULL;
	Monitor *mon = old->mon;

	new->mon = mon;
	new->tags = old->tags;
	new->isfloating = old->isfloating;

	new->next = old->next;
	new->snext = old->snext;

	if (old == mon->clients)
		mon->clients = new;
	else {
		for (c = mon->clients; c && c->next != old; c = c->next);
		c->next = new;
	}

	if (old == mon->stack)
		mon->stack = new;
	else {
		for (c = mon->stack; c && c->snext != old; c = c->snext);
		c->snext = new;
	}

	old->next = NULL;
	old->snext = NULL;

	XMoveWindow(dpy, old->win, WIDTH(old) * -2, old->y);

	if (ISVISIBLE(new)) {
		if (new->isfloating)
			resize(new, old->x, old->y, new->w - 2*new->bw, new->h - 2*new->bw, 0);
		else
			resize(new, old->x, old->y, old->w - 2*new->bw, old->h - 2*new->bw, 0);
	}
}

void
resize(Client *c, int x, int y, int w, int h, int interact)
{
	if (applysizehints(c, &x, &y, &w, &h, interact))
		resizeclient(c, x, y, w, h);
}

void
resizeclient(Client *c, int x, int y, int w, int h)
{
	XWindowChanges wc;

	c->oldx = c->x; c->x = wc.x = x;
	c->oldy = c->y; c->y = wc.y = y;
	c->oldw = c->w; c->w = wc.width = w;
	c->oldh = c->h; c->h = wc.height = h;

	if (c->beingmoved)
		return;

	wc.border_width = c->bw;
	XConfigureWindow(dpy, c->win, CWX|CWY|CWWidth|CWHeight|CWBorderWidth, &wc);
	configure(c);
	XSync(dpy, False);
}

void
resizemouse(const Arg *arg)
{
	int opx, opy, ocx, ocy, och, ocw, nx, ny, nw, nh;
	Client *c;
	Monitor *m;
	XEvent ev;
	int horizcorner, vertcorner;
	unsigned int dui;
	Window dummy;
	Time lasttime = 0;

	if (!(c = selmon->sel))
		return;
	if (c->isfullscreen && c->fakefullscreen != 1) /* no support resizing fullscreen windows by mouse */
		return;
	restack(selmon);
	ocx = c->x;
	ocy = c->y;
	och = c->h;
	ocw = c->w;
	if (!XQueryPointer(dpy, c->win, &dummy, &dummy, &opx, &opy, &nx, &ny, &dui))
		return;
	horizcorner = nx < c->w / 2;
	vertcorner  = ny < c->h / 2;
	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
		None, cursor[horizcorner | (vertcorner << 1)]->cursor, CurrentTime) != GrabSuccess)
		return;
	do {
		XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
		switch(ev.type) {
		case ConfigureRequest:
		case Expose:
		case MapRequest:
			handler[ev.type](&ev);
			break;
		case MotionNotify:
			if ((ev.xmotion.time - lasttime) <= (1000 / 60))
				continue;
			lasttime = ev.xmotion.time;

			nx = horizcorner ? (ocx + ev.xmotion.x - opx) : c->x;
			ny = vertcorner ? (ocy + ev.xmotion.y - opy) : c->y;
			nw = MAX(horizcorner ? (ocx + ocw - nx) : (ocw + (ev.xmotion.x - opx)), 1);
			nh = MAX(vertcorner ? (ocy + och - ny) : (och + (ev.xmotion.y - opy)), 1);
			if (c->mon->wx + nw >= selmon->wx && c->mon->wx + nw <= selmon->wx + selmon->ww
			&& c->mon->wy + nh >= selmon->wy && c->mon->wy + nh <= selmon->wy + selmon->wh)
			{
				if (!c->isfloating && selmon->lt[selmon->sellt]->arrange
				&& (abs(nw - c->w) > snap || abs(nh - c->h) > snap))
					togglefloating(NULL);
			}
			if (!selmon->lt[selmon->sellt]->arrange || c->isfloating)
				resizeclient(c, nx, ny, nw, nh);
			break;
		}
	} while (ev.type != ButtonRelease);
	XUngrabPointer(dpy, CurrentTime);
	while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
	if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
		sendmon(c, m);
		selmon = m;
		focus(NULL);
	}
}

void
resourceload(XrmDatabase db, char *name, enum resource_type rtype, void *dst)
{
	char *sdst = NULL;
	int *idst = NULL;
	float *fdst = NULL;

	sdst = dst;
	idst = dst;
	fdst = dst;

	char fullname[256];
	char *type;
	XrmValue ret;

	snprintf(fullname, sizeof(fullname), "%s.%s", "dwm", name);
	fullname[sizeof(fullname) - 1] = '\0';

	XrmGetResource(db, fullname, "*", &type, &ret);
	if (!(ret.addr == NULL || strncmp("String", type, 64)))
	{
		switch (rtype) {
		case STRING:
			strcpy(sdst, ret.addr);
			break;
		case INTEGER:
			*idst = strtoul(ret.addr, NULL, 10);
			break;
		case FLOAT:
			*fdst = strtof(ret.addr, NULL);
			break;
		}
	}
}

void
restack(Monitor *m)
{
	Client *c;
	XEvent ev;
	XWindowChanges wc;

	drawbar(m);
	if (!m->sel)
		return;
	if (m->sel->isfloating || !m->lt[m->sellt]->arrange)
		XRaiseWindow(dpy, m->sel->win);
	if (m->lt[m->sellt]->arrange) {
		wc.stack_mode = Below;
		wc.sibling = m->bar->win;
		for (c = m->stack; c; c = c->snext)
			if (!c->isfloating && ISVISIBLE(c)) {
				XConfigureWindow(dpy, c->win, CWSibling|CWStackMode, &wc);
				wc.sibling = c->win;
			}
	}
	XSync(dpy, False);
	while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
}

void
rotatestack(const Arg *arg)
{
	Client *c = NULL, *f;

	if (!selmon->sel)
		return;
	f = selmon->sel;
	if (arg->i > 0) {
		for (c = nexttiled(selmon->clients); c && nexttiled(c->next); c = nexttiled(c->next));
		if (c){
			detach(c);
			attach(c);
			detachstack(c);
			attachstack(c);
		}
	} else {
		if ((c = nexttiled(selmon->clients))){
			detach(c);
			enqueue(c);
			detachstack(c);
			enqueuestack(c);
		}
	}
	if (c){
		arrange(selmon);
		//unfocus(f, 1);
		focus(f);
		restack(selmon);
	}
}

void
run(void)
{
	XEvent ev;
	/* main event loop */
	XSync(dpy, False);
	while (running && !XNextEvent(dpy, &ev))
		if (handler[ev.type])
			handler[ev.type](&ev); /* call handler */
}

void
runautostart(void)
{
	char *pathpfx;
	char *path;
	char *xdgconfighome;
	char *home;
	struct stat sb;

	if ((home = getenv("HOME")) == NULL)
		/* this is almost impossible */
		return;

	/* if $XDG_CONFIG_HOME is set and not empty, use $XDG_CONFIG_HOME/dwm,
	 * otherwise use ~/.config/dwm as autostart script directory
	 */
	xdgconfighome = getenv("XDG_CONFIG_HOME");
	if (xdgconfighome != NULL && *xdgconfighome != '\0') {
		/* space for path segments, separators and nul */
		pathpfx = ecalloc(1, strlen(xdgconfighome) + strlen(dwmdir) + 2);

		if (sprintf(pathpfx, "%s/%s", xdgconfighome, dwmdir) <= 0) {
			free(pathpfx);
			return;
		}
	} else {
		/* space for path segments, separators and nul */
		pathpfx = ecalloc(1, strlen(home) + strlen(pathfromhome)
		                     + strlen(dwmdir) + 3);

		if (sprintf(pathpfx, "%s/%s/%s", home, pathfromhome, dwmdir) < 0) {
			free(pathpfx);
			return;
		}
	}

	/* check if the autostart script directory exists */
	if (! (stat(pathpfx, &sb) == 0 && S_ISDIR(sb.st_mode))) {
		/* the XDG conformant path does not exist or is no directory
		 * so we try ~/.dwm instead
		 */
		char *pathpfx_new = realloc(pathpfx, strlen(home) + strlen(dwmdir) + 3);
		if(pathpfx_new == NULL) {
			free(pathpfx);
			return;
		}
		pathpfx = pathpfx_new;

		if (sprintf(pathpfx, "%s/.%s", home, dwmdir) <= 0) {
			free(pathpfx);
			return;
		}
	}

	/* try the wait script first */
	path = ecalloc(1, strlen(pathpfx) + strlen(autostartwaitsh) + 2);
	if (sprintf(path, "%s/%s", pathpfx, autostartwaitsh) <= 0) {
		free(path);
		free(pathpfx);
	}

	if (access(path, X_OK) == 0)
		system(path);

	/* now the non-blocking script */
	if (sprintf(path, "%s/%s", pathpfx, autostartsh) <= 0) {
		free(path);
		free(pathpfx);
	}

	if (access(path, X_OK) == 0)
		system(strcat(path, " &"));

	free(pathpfx);
	free(path);
}

void
scan(void)
{
	unsigned int i, num;
	Window d1, d2, *wins = NULL;
	XWindowAttributes wa;

	if (XQueryTree(dpy, root, &d1, &d2, &wins, &num)) {
		for (i = 0; i < num; i++) {
			if (!XGetWindowAttributes(dpy, wins[i], &wa)
			|| wa.override_redirect || XGetTransientForHint(dpy, wins[i], &d1))
				continue;
			if (wa.map_state == IsViewable || getstate(wins[i]) == IconicState)
				manage(wins[i], &wa);
		}
		for (i = 0; i < num; i++) { /* now the transients */
			if (!XGetWindowAttributes(dpy, wins[i], &wa))
				continue;
			if (XGetTransientForHint(dpy, wins[i], &d1)
			&& (wa.map_state == IsViewable || getstate(wins[i]) == IconicState))
				manage(wins[i], &wa);
		}
		if (wins)
			XFree(wins);
	}
}

void
sendmon(Client *c, Monitor *m)
{
	if (c->mon == m)
		return;
	int hadfocus = (c == selmon->sel);
	unfocus(c, 1);
	detach(c);
	detachstack(c);
	arrange(c->mon);
	c->mon = m;
	c->tags = m->tagset[m->seltags]; /* assign tags of target monitor */
	attach(c);
	attachstack(c);
	arrange(m);
	if (hadfocus) {
		focus(c);
		restack(m);
	} else
		focus(NULL);
}

void
setclientstate(Client *c, long state)
{
	long data[] = { state, None };

	XChangeProperty(dpy, c->win, wmatom[WMState], wmatom[WMState], 32,
		PropModeReplace, (unsigned char *)data, 2);
}

int
sendevent(Window w, Atom proto, int mask, long d0, long d1, long d2, long d3, long d4)
{
	int n;
	Atom *protocols;
	Atom mt;
	int exists = 0;
	XEvent ev;

	if (proto == wmatom[WMTakeFocus] || proto == wmatom[WMDelete]) {
		mt = wmatom[WMProtocols];
		if (XGetWMProtocols(dpy, w, &protocols, &n)) {
			while (!exists && n--)
				exists = protocols[n] == proto;
			XFree(protocols);
		}
	} else {
		exists = True;
		mt = proto;
	}
	if (exists) {
		ev.type = ClientMessage;
		ev.xclient.window = w;
		ev.xclient.message_type = mt;
		ev.xclient.format = 32;
		ev.xclient.data.l[0] = d0;
		ev.xclient.data.l[1] = d1;
		ev.xclient.data.l[2] = d2;
		ev.xclient.data.l[3] = d3;
		ev.xclient.data.l[4] = d4;
		XSendEvent(dpy, w, False, mask, &ev);
	}
	return exists;
}

void
setfocus(Client *c)
{
	if (!c->neverfocus) {
		XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
		XChangeProperty(dpy, root, netatom[NetActiveWindow],
			XA_WINDOW, 32, PropModeReplace,
			(unsigned char *) &(c->win), 1);
	}
	sendevent(c->win, wmatom[WMTakeFocus], NoEventMask, wmatom[WMTakeFocus], CurrentTime, 0, 0, 0);
}

void
setfullscreen(Client *c, int fullscreen)
{
	XEvent ev;
	int savestate = 0, restorestate = 0, restorefakefullscreen = 0;

	if ((c->fakefullscreen == 0 && fullscreen && !c->isfullscreen) // normal fullscreen
			|| (c->fakefullscreen == 2 && fullscreen)) // fake fullscreen --> actual fullscreen
		savestate = 1; // go actual fullscreen
	else if ((c->fakefullscreen == 0 && !fullscreen && c->isfullscreen) // normal fullscreen exit
			|| (c->fakefullscreen >= 2 && !fullscreen)) // fullscreen exit --> fake fullscreen
		restorestate = 1; // go back into tiled

	/* If leaving fullscreen and the window was previously fake fullscreen (2), then restore
	 * that while staying in fullscreen. The exception to this is if we are in said state, but
	 * the client itself disables fullscreen (3) then we let the client go out of fullscreen
	 * while keeping fake fullscreen enabled (as otherwise there will be a mismatch between the
	 * client and the window manager's perception of the client's fullscreen state). */
	if (c->fakefullscreen == 2 && !fullscreen && c->isfullscreen) {
		restorefakefullscreen = 1;
		c->isfullscreen = 1;
		fullscreen = 1;
	}

	if (fullscreen != c->isfullscreen) { // only send property change if necessary
		if (fullscreen)
			XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
				PropModeReplace, (unsigned char*)&netatom[NetWMFullscreen], 1);
		else
			XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
				PropModeReplace, (unsigned char*)0, 0);
	}

	c->isfullscreen = fullscreen;

	/* Some clients, e.g. firefox, will send a client message informing the window manager
	 * that it is going into fullscreen after receiving the above signal. This has the side
	 * effect of this function (setfullscreen) sometimes being called twice when toggling
	 * fullscreen on and off via the window manager as opposed to the application itself.
	 * To protect against obscure issues where the client settings are stored or restored
	 * when they are not supposed to we add an additional bit-lock on the old state so that
	 * settings can only be stored and restored in that precise order. */
	if (savestate && !(c->oldstate & (1 << 1))) {
		c->oldbw = c->bw;
		c->oldstate = c->isfloating | (1 << 1);
		c->bw = 0;
		c->isfloating = 1;
		resizeclient(c, c->mon->mx, c->mon->my, c->mon->mw, c->mon->mh);
		XRaiseWindow(dpy, c->win);
	} else if (restorestate && (c->oldstate & (1 << 1))) {
		c->bw = c->oldbw;
		c->isfloating = c->oldstate = c->oldstate & 1;
		if (restorefakefullscreen || c->fakefullscreen == 3)
			c->fakefullscreen = 1;
		/* The client may have been moved to another monitor whilst in fullscreen which if tiled
		 * we address by doing a full arrange of tiled clients. If the client is floating then the
		 * height and width may be larger than the monitor's window area, so we cap that by
		 * ensuring max / min values. */
		if (c->isfloating) {
			c->x = MAX(c->mon->wx, c->oldx);
			c->y = MAX(c->mon->wy, c->oldy);
			c->w = MIN(c->mon->ww - c->x - 2*c->bw, c->oldw);
			c->h = MIN(c->mon->wh - c->y - 2*c->bw, c->oldh);
			resizeclient(c, c->x, c->y, c->w, c->h);
			restack(c->mon);
		} else
			arrange(c->mon);
	} else
		resizeclient(c, c->x, c->y, c->w, c->h);

	/* Exception: if the client was in actual fullscreen and we exit out to fake fullscreen
	 * mode, then the focus would sometimes drift to whichever window is under the mouse cursor
	 * at the time. To avoid this we ask X for all EnterNotify events and just ignore them.
	 */
	if (!c->isfullscreen)
		while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
}

void
setlayout(const Arg *arg)
{
	if (!arg || !arg->v || arg->v != selmon->lt[selmon->sellt])
		selmon->sellt = selmon->pertag->sellts[selmon->pertag->curtag] ^= 1;
	if (arg && arg->v)
		selmon->lt[selmon->sellt] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt] = (Layout *)arg->v;
	strncpy(selmon->ltsymbol, selmon->lt[selmon->sellt]->symbol, sizeof selmon->ltsymbol);
	if (selmon->sel)
		arrange(selmon);
	else
		drawbar(selmon);
}

void
setcfact(const Arg *arg)
{
	float f;
	Client *c;

	c = selmon->sel;

	if (!arg || !c || !selmon->lt[selmon->sellt]->arrange)
		return;
	if (!arg->f)
		f = 1.0;
	else if (arg->f > 4.0) // set fact absolutely
		f = arg->f - 4.0;
	else
		f = arg->f + c->cfact;
	if (f < 0.25)
		f = 0.25;
	else if (f > 4.0)
		f = 4.0;
	c->cfact = f;
	arrange(selmon);
}

/* arg > 1.0 will set mfact absolutely */
void
setmfact(const Arg *arg)
{
	float f;

	if (!arg || !selmon->lt[selmon->sellt]->arrange)
		return;
	f = arg->f < 1.0 ? arg->f + selmon->mfact : arg->f - 1.0;
	if (f < 0.05 || f > 0.95)
		return;
	selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag] = f;
	arrange(selmon);
}

void
setscratch(const Arg *arg)
{
	Client *c = selmon->sel;
	if (!c)
		return;

	c->scratchkey = ((char**)arg->v)[0][0];
}

void
setup(void)
{
	int i;
	XSetWindowAttributes wa;
	Atom utf8string;

	/* clean up any zombies immediately */
	sigchld(0);

	signal(SIGHUP, sighup);
	signal(SIGTERM, sigterm);

	/* init screen */
	screen = DefaultScreen(dpy);
	sw = DisplayWidth(dpy, screen);
	sh = DisplayHeight(dpy, screen);
	root = RootWindow(dpy, screen);
	drw = drw_create(dpy, screen, root, sw, sh);
	if (!drw_fontset_create(drw, fonts, LENGTH(fonts)))
		die("no fonts could be loaded.");
	lrpad = drw->fonts->h;
	bh = barheight ? barheight : drw->fonts->h + 5;
	updategeom();
	/* init atoms */
	utf8string = XInternAtom(dpy, "UTF8_STRING", False);
	wmatom[WMProtocols] = XInternAtom(dpy, "WM_PROTOCOLS", False);
	wmatom[WMDelete] = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	wmatom[WMState] = XInternAtom(dpy, "WM_STATE", False);
	wmatom[WMTakeFocus] = XInternAtom(dpy, "WM_TAKE_FOCUS", False);
	netatom[NetActiveWindow] = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
	netatom[NetSupported] = XInternAtom(dpy, "_NET_SUPPORTED", False);
	netatom[NetSystemTray] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_S0", False);
	netatom[NetSystemTrayOP] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_OPCODE", False);
	netatom[NetSystemTrayOrientation] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_ORIENTATION", False);
	netatom[NetSystemTrayOrientationHorz] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_ORIENTATION_HORZ", False);
	netatom[NetSystemTrayVisual] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_VISUAL", False);
	netatom[NetWMWindowTypeDock] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
	xatom[Manager] = XInternAtom(dpy, "MANAGER", False);
	xatom[Xembed] = XInternAtom(dpy, "_XEMBED", False);
	xatom[XembedInfo] = XInternAtom(dpy, "_XEMBED_INFO", False);
	netatom[NetWMName] = XInternAtom(dpy, "_NET_WM_NAME", False);
	netatom[NetWMState] = XInternAtom(dpy, "_NET_WM_STATE", False);
	netatom[NetWMCheck] = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
	netatom[NetWMFullscreen] = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
	netatom[NetWMWindowType] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
	netatom[NetWMWindowTypeDialog] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
	netatom[NetClientList] = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
	/* init cursors */
	cursor[CurNormal] = drw_cur_create(drw, XC_left_ptr);
	cursor[CurResize] = drw_cur_create(drw, XC_sizing);
	cursor[CurResizeBR] = drw_cur_create(drw, XC_bottom_right_corner);
	cursor[CurResizeBL] = drw_cur_create(drw, XC_bottom_left_corner);
	cursor[CurResizeTR] = drw_cur_create(drw, XC_top_right_corner);
	cursor[CurResizeTL] = drw_cur_create(drw, XC_top_left_corner);
	cursor[CurResizeHorzArrow] = drw_cur_create(drw, XC_sb_h_double_arrow);
	cursor[CurResizeVertArrow] = drw_cur_create(drw, XC_sb_v_double_arrow);
	cursor[CurIronCross] = drw_cur_create(drw, XC_iron_cross);
	cursor[CurMove] = drw_cur_create(drw, XC_fleur);
	/* init appearance */
	scheme = ecalloc(LENGTH(colors), sizeof(Clr *));
	for (i = 0; i < LENGTH(colors); i++)
		scheme[i] = drw_scm_create(drw, colors[i], 3);
	/* init bars */
	updatebars();
	updatestatus();
	/* supporting window for NetWMCheck */
	wmcheckwin = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
	XChangeProperty(dpy, wmcheckwin, netatom[NetWMCheck], XA_WINDOW, 32,
		PropModeReplace, (unsigned char *) &wmcheckwin, 1);
	XChangeProperty(dpy, wmcheckwin, netatom[NetWMName], utf8string, 8,
		PropModeReplace, (unsigned char *) "dwm", 3);
	XChangeProperty(dpy, root, netatom[NetWMCheck], XA_WINDOW, 32,
		PropModeReplace, (unsigned char *) &wmcheckwin, 1);
	/* EWMH support per view */
	XChangeProperty(dpy, root, netatom[NetSupported], XA_ATOM, 32,
		PropModeReplace, (unsigned char *) netatom, NetLast);
	XDeleteProperty(dpy, root, netatom[NetClientList]);
	/* select events */
	wa.cursor = cursor[CurNormal]->cursor;
	wa.event_mask = SubstructureRedirectMask|SubstructureNotifyMask
		|ButtonPressMask|PointerMotionMask|EnterWindowMask
		|LeaveWindowMask|StructureNotifyMask|PropertyChangeMask;
	XChangeWindowAttributes(dpy, root, CWEventMask|CWCursor, &wa);
	XSelectInput(dpy, root, wa.event_mask);
	grabkeys();
	focus(NULL);
}


void
seturgent(Client *c, int urg)
{
	XWMHints *wmh;

	c->isurgent = urg;
	if (!(wmh = XGetWMHints(dpy, c->win)))
		return;
	wmh->flags = urg ? (wmh->flags | XUrgencyHint) : (wmh->flags & ~XUrgencyHint);
	XSetWMHints(dpy, c->win, wmh);
	XFree(wmh);
}

void
shiftviewclients(const Arg *arg)
{
	Arg shifted;
	Client *c;
	unsigned int tagmask = 0;

	for (c = selmon->clients; c; c = c->next)
		#if SCRATCHPADS_PATCH
		if (!(c->tags & SPTAGMASK))
			tagmask = tagmask | c->tags;
		#else
		tagmask = tagmask | c->tags;
		#endif // SCRATCHPADS_PATCH

	#if SCRATCHPADS_PATCH
	shifted.ui = selmon->tagset[selmon->seltags] & ~SPTAGMASK;
	#else
	shifted.ui = selmon->tagset[selmon->seltags];
	#endif // SCRATCHPADS_PATCH
	if (arg->i > 0) // left circular shift
		do {
			shifted.ui = (shifted.ui << arg->i)
			   | (shifted.ui >> (LENGTH(tags) - arg->i));
			#if SCRATCHPADS_PATCH
			shifted.ui &= ~SPTAGMASK;
			#endif // SCRATCHPADS_PATCH
		} while (tagmask && !(shifted.ui & tagmask));
	else // right circular shift
		do {
			shifted.ui = (shifted.ui >> (- arg->i)
			   | shifted.ui << (LENGTH(tags) + arg->i));
			#if SCRATCHPADS_PATCH
			shifted.ui &= ~SPTAGMASK;
			#endif // SCRATCHPADS_PATCH
		} while (tagmask && !(shifted.ui & tagmask));

	view(&shifted);
}

void
showhide(Client *c)
{
	if (!c)
		return;
	if (ISVISIBLE(c)) {
		/* show clients top down */
		XMoveWindow(dpy, c->win, c->x, c->y);
		if ((!c->mon->lt[c->mon->sellt]->arrange || c->isfloating) && !c->isfullscreen)
			resize(c, c->x, c->y, c->w, c->h, 0);
		showhide(c->snext);
	} else {
		/* optional: auto-hide scratchpads when moving to other tags */
		if (c->scratchkey != 0 && !(c->tags & c->mon->tagset[c->mon->seltags]))
			c->tags = 0;
		/* hide clients bottom up */
		showhide(c->snext);
		XMoveWindow(dpy, c->win, WIDTH(c) * -2, c->y);
	}
}

void
sigchld(int unused)
{
	if (signal(SIGCHLD, sigchld) == SIG_ERR)
		die("can't install SIGCHLD handler:");
	while (0 < waitpid(-1, NULL, WNOHANG));
}

void
sighup(int unused)
{
	Arg a = {.i = 1};
	quit(&a);
}

void
sigterm(int unused)
{
	Arg a = {.i = 0};
	quit(&a);
}

void
spawn(const Arg *arg)
{
	if (fork() == 0) {
		if (dpy)
			close(ConnectionNumber(dpy));
		setsid();
		execvp(((char **)arg->v)[0], (char **)arg->v);
		fprintf(stderr, "dwm: execvp %s", ((char **)arg->v)[0]);
		perror(" failed");
		exit(EXIT_SUCCESS);
	}
}

void spawnscratch(const Arg *arg)
{
	if (fork() == 0) {
		if (dpy)
			close(ConnectionNumber(dpy));
		setsid();
		execvp(((char **)arg->v)[1], ((char **)arg->v)+1);
		fprintf(stderr, "dwm: execvp %s", ((char **)arg->v)[1]);
		perror(" failed");
		exit(EXIT_SUCCESS);
	}
}

int
swallow(Client *t, Client *c)
{
	if (c->noswallow || c->isterminal)
		return 0;
	if (!swallowfloating && c->isfloating)
		return 0;

	replaceclient(t, c);
	c->ignorecfgreqpos = 1;
	c->swallowing = t;

	return 1;
}

Client *
swallowingclient(Window w)
{
	Client *c;
	Monitor *m;

	for (m = mons; m; m = m->next) {
		for (c = m->clients; c; c = c->next) {
			if (c->swallowing && c->swallowing->win == w)
				return c;
		}
	}

	return NULL;
}

void
tag(const Arg *arg)
{
	if (selmon->sel && arg->ui & TAGMASK) {
		selmon->sel->tags = arg->ui & TAGMASK;
		focus(NULL);
		arrange(selmon);
		if(VIEWONTAG && ((arg->ui & TAGMASK) != TAGMASK))
			view(arg);
	}
}

void
tagmon(const Arg *arg)
{
	Client *c = selmon->sel;
	if (!c || !mons->next)
		return;
	if (c->isfullscreen) {
		c->isfullscreen = 0;
		sendmon(c, dirtomon(arg->i));
		c->isfullscreen = 1;
		if (c->fakefullscreen != 1) {
			resizeclient(c, c->mon->mx, c->mon->my, c->mon->mw, c->mon->mh);
			XRaiseWindow(dpy, c->win);
		}
	} else
		sendmon(c, dirtomon(arg->i));
}

Client *
termforwin(const Client *w)
{
	Client *c;
	Monitor *m;

	if (!w->pid || w->isterminal)
		return NULL;

	for (m = mons; m; m = m->next) {
		for (c = m->clients; c; c = c->next) {
			if (c->isterminal && !c->swallowing && c->pid && isdescprocess(c->pid, w->pid))
				return c;
		}
	}

	return NULL;
}

void
togglebar(const Arg *arg)
{
	Bar *bar;
	selmon->showbar = selmon->pertag->showbars[selmon->pertag->curtag] = !selmon->showbar;
	updatebarpos(selmon);
	for (bar = selmon->bar; bar; bar = bar->next)
		XMoveResizeWindow(dpy, bar->win, bar->bx, bar->by, bar->bw, bar->bh);
	arrange(selmon);
}

void
togglefakefullscreen(const Arg *arg)
{
	Client *c = selmon->sel;
	if (!c)
		return;

	if (c->fakefullscreen != 1 && c->isfullscreen) { // exit fullscreen --> fake fullscreen
		c->fakefullscreen = 2;
		setfullscreen(c, 0);
	} else if (c->fakefullscreen == 1) {
		setfullscreen(c, 0);
		c->fakefullscreen = 0;
	} else {
		c->fakefullscreen = 1;
		setfullscreen(c, 1);
	}
}

void
togglefloating(const Arg *arg)
{
	Client *c = selmon->sel;
	if (!c)
		return;
	if (c->isfullscreen && c->fakefullscreen != 1) /* no support for fullscreen windows */
		return;
	c->isfloating = !c->isfloating || c->isfixed;
	if (selmon->sel->isfloating)
		/* restore last known float dimensions */
		resize(c, c->sfx, c->sfy, c->sfw, c->sfh, False);
	else {
		/* save last known float dimensions */
		c->sfx = c->x;
		c->sfy = c->y;
		c->sfw = c->w;
		c->sfh = c->h;
	}
	arrange(c->mon);
}

void
togglefullscreen(const Arg *arg)
{
	Client *c = selmon->sel;
	if (!c)
		return;

	if (c->fakefullscreen == 1) { // fake fullscreen --> fullscreen
		c->fakefullscreen = 2;
		setfullscreen(c, 1);
	} else
		setfullscreen(c, !c->isfullscreen);
}

void
togglegaps(const Arg *arg)
{
	#if PERTAG_VANITYGAPS
	selmon->pertag->enablegaps[selmon->pertag->curtag] = !selmon->pertag->enablegaps[selmon->pertag->curtag];
	#else
	enablegaps = !enablegaps;
	#endif // PERTAG_VANITYGAPS
	arrange(NULL);
}

void
togglescratch(const Arg *arg)
{
	Client *c, *next, *last = NULL, *found = NULL, *monclients = NULL;
	Monitor *mon;
	int scratchvisible = 0; // whether the scratchpads are currently visible or not
	int multimonscratch = 0; // whether we have scratchpads that are placed on multiple monitors
	int scratchmon = -1; // the monitor where the scratchpads exist
	int numscratchpads = 0; // count of scratchpads

	/* Looping through monitors and client's twice, the first time to work out whether we need
	   to move clients across from one monitor to another or not */
	for (mon = mons; mon; mon = mon->next)
		for (c = mon->clients; c; c = c->next) {
			if (c->scratchkey != ((char**)arg->v)[0][0])
				continue;
			if (scratchmon != -1 && scratchmon != mon->num)
				multimonscratch = 1;
			if (c->mon->tagset[c->mon->seltags] & c->tags) // && !HIDDEN(c)
				++scratchvisible;
			scratchmon = mon->num;
			++numscratchpads;
		}

	/* Now for the real deal. The logic should go like:
	    - hidden scratchpads will be shown
	    - shown scratchpads will be hidden, unless they are being moved to the current monitor
	    - the scratchpads will be moved to the current monitor if they all reside on the same monitor
	    - multiple scratchpads residing on separate monitors will be left in place
	 */
	for (mon = mons; mon; mon = mon->next) {
		for (c = mon->stack; c; c = next) {
			next = c->snext;
			if (c->scratchkey != ((char**)arg->v)[0][0])
				continue;

			/* awesomebar / wintitleactions compatibility, unhide scratchpad if hidden
			if (HIDDEN(c)) {
				XMapWindow(dpy, c->win);
				setclientstate(c, NormalState);
			}
			*/

			/* Record the first found scratchpad client for focus purposes, but prioritise the
			   scratchpad on the current monitor if one exists */
			if (!found || (mon == selmon && c->mon != selmon))
				found = c;

			unfocus(c, 0); // unfocus to avoid client border discrepancies

			/* If scratchpad clients reside on another monitor and we are moving them across then
			   as we are looping through monitors we could be moving a client to a monitor that has
			   not been processed yet, hence we could be processing a scratchpad twice. To avoid
			   this we detach them and add them to a temporary list (monclients) which is to be
			   processed later. */
			if (!multimonscratch && c->mon != selmon) {
				detach(c);
				detachstack(c);
				c->next = NULL;
				/* Note that we are adding clients at the end of the list, this is to preserve the
				   order of clients as they were on the adjacent monitor (relevant when tiled) */
				if (last)
					last = last->next = c;
				else
					last = monclients = c;
			} else if (scratchvisible == numscratchpads) {
				c->tags = 0;
			} else {
				c->tags = c->mon->tagset[c->mon->seltags];
				if (c->isfloating)
					XRaiseWindow(dpy, c->win);
			}
		}
	}

	/* Attach moved scratchpad clients on the selected monitor */
	for (c = monclients; c; c = next) {
		next = c->next;
		mon = c->mon;
		c->mon = selmon;
		c->tags = selmon->tagset[selmon->seltags];
		/* Attach scratchpad clients from other monitors at the bottom of the stack */
		if (selmon->clients) {
			for (last = selmon->clients; last && last->next; last = last->next);
			last->next = c;
		} else
			selmon->clients = c;
		c->next = NULL;
		attachstack(c);

		/* Center floating scratchpad windows when moved from one monitor to another */
		if (c->isfloating) {
			if (c->w > selmon->ww)
				c->w = selmon->ww - c->bw * 2;
			if (c->h > selmon->wh)
				c->h = selmon->wh - c->bw * 2;

			if (numscratchpads > 1) {
				c->x = c->mon->wx + (c->x - mon->wx) * ((double)(abs(c->mon->ww - WIDTH(c))) / MAX(abs(mon->ww - WIDTH(c)), 1));
				c->y = c->mon->wy + (c->y - mon->wy) * ((double)(abs(c->mon->wh - HEIGHT(c))) / MAX(abs(mon->wh - HEIGHT(c)), 1));
			} else if (c->x < c->mon->mx || c->x > c->mon->mx + c->mon->mw ||
			           c->y < c->mon->my || c->y > c->mon->my + c->mon->mh)	{
				c->x = c->mon->wx + (c->mon->ww / 2 - WIDTH(c) / 2);
				c->y = c->mon->wy + (c->mon->wh / 2 - HEIGHT(c) / 2);
			}
			resizeclient(c, c->x, c->y, c->w, c->h);
			XRaiseWindow(dpy, c->win);
		}
	}

	if (found) {
		focus(ISVISIBLE(found) ? found : NULL);
		arrange(NULL);
		if (found->isfloating)
			XRaiseWindow(dpy, found->win);
	} else {
		spawnscratch(arg);
	}
}

void
togglesticky(const Arg *arg)
{
	if (!selmon->sel)
		return;
	selmon->sel->issticky = !selmon->sel->issticky;
	arrange(selmon);
}

void
toggletag(const Arg *arg)
{
	unsigned int newtags;

	if (!selmon->sel)
		return;
	newtags = selmon->sel->tags ^ (arg->ui & TAGMASK);
	if (newtags) {
		selmon->sel->tags = newtags;
		focus(NULL);
		arrange(selmon);
	}
}

void
toggleview(const Arg *arg)
{
	unsigned int newtagset = selmon->tagset[selmon->seltags] ^ (arg->ui & TAGMASK);
	int i;

	if (newtagset) {
		selmon->tagset[selmon->seltags] = newtagset;

		if (newtagset == ~0) {
			selmon->pertag->prevtag = selmon->pertag->curtag;
			selmon->pertag->curtag = 0;
		}

		/* test if the user did not select the same tag */
		if (!(newtagset & 1 << (selmon->pertag->curtag - 1))) {
			selmon->pertag->prevtag = selmon->pertag->curtag;
			for (i = 0; !(newtagset & 1 << i); i++) ;
			selmon->pertag->curtag = i + 1;
		}

		/* apply settings for this view */
		selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag];
		selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag];
		selmon->sellt = selmon->pertag->sellts[selmon->pertag->curtag];
		selmon->lt[selmon->sellt] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt];
		selmon->lt[selmon->sellt^1] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt^1];

		if (selmon->showbar != selmon->pertag->showbars[selmon->pertag->curtag])
			togglebar(NULL);

		focus(NULL);
		arrange(selmon);
	}
}

void
unfocus(Client *c, int setfocus)
{
	if (!c)
		return;
	grabbuttons(c, 0);
	XSetWindowBorder(dpy, c->win, scheme[SchemeNorm][ColBorder].pixel);
	if (setfocus) {
		XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
		XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
	}
}

void
unmanage(Client *c, int destroyed)
{
	Client *s;
	Monitor *m = c->mon;
	XWindowChanges wc;

	if (c->swallowing)
		unswallow(c);

	s = swallowingclient(c->win);
	if (s)
		s->swallowing = NULL;

	detach(c);
	detachstack(c);
	if (!destroyed) {
		wc.border_width = c->oldbw;
		XGrabServer(dpy); /* avoid race conditions */
		XSetErrorHandler(xerrordummy);
		XConfigureWindow(dpy, c->win, CWBorderWidth, &wc); /* restore border */
		XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
		setclientstate(c, WithdrawnState);
		XSync(dpy, False);
		XSetErrorHandler(xerror);
		XUngrabServer(dpy);
	}
	free(c);
	focus(NULL);
	updateclientlist();
	arrange(m);
}

void
unmapnotify(XEvent *e)
{
	Client *c;
	XUnmapEvent *ev = &e->xunmap;

	if ((c = wintoclient(ev->window))) {
		if (ev->send_event)
			setclientstate(c, WithdrawnState);
		else
			unmanage(c, 0);
	} else if (showsystray && (c = wintosystrayicon(ev->window))) {
		/* KLUDGE! sometimes icons occasionally unmap their windows, but do
		 * _not_ destroy them. We map those windows back */
		XMapRaised(dpy, c->win);
		drawbarwin(systray->bar);
	}
}

void
updatebars(void)
{
	Bar *bar;
	Monitor *m;
	XSetWindowAttributes wa = {
		.override_redirect = True,
		#if BAR_ALPHA_PATCH
		.background_pixel = 0,
		.border_pixel = 0,
		.colormap = cmap,
		#else
		.background_pixmap = ParentRelative,
		#endif // BAR_ALPHA_PATCH
		.event_mask = ButtonPressMask|ExposureMask
	};
	XClassHint ch = {"dwm", "dwm"};
	for (m = mons; m; m = m->next) {
		for (bar = m->bar; bar; bar = bar->next) {
			if (!bar->win) {
				#if BAR_ALPHA_PATCH
				bar->win = XCreateWindow(dpy, root, bar->bx, bar->by, bar->bw, bar->bh, 0, depth,
						InputOutput, visual,
						CWOverrideRedirect|CWBackPixel|CWBorderPixel|CWColormap|CWEventMask, &wa);
				#else
				bar->win = XCreateWindow(dpy, root, bar->bx, bar->by, bar->bw, bar->bh, 0, DefaultDepth(dpy, screen),
						CopyFromParent, DefaultVisual(dpy, screen),
						CWOverrideRedirect|CWBackPixmap|CWEventMask, &wa);
				#endif // BAR_ALPHA_PATCH
				XDefineCursor(dpy, bar->win, cursor[CurNormal]->cursor);
				XMapRaised(dpy, bar->win);
				XSetClassHint(dpy, bar->win, &ch);
			}
		}
	}
}

void
unswallow(Client *c)
{
	replaceclient(c, c->swallowing);
	c->swallowing = NULL;
}

void
updatebarpos(Monitor *m)
{
	m->wy = m->my;
	m->wh = m->mh;
	int num_bars;
	Bar *bar;
	int y_pad = 0;
	int x_pad = 0;

	for (bar = m->bar; bar; bar = bar->next) {
		bar->bx = m->mx + x_pad;
		bar->bw = m->ww - 2 * x_pad;
		bar->bh = bh;
	}

	if (!m->showbar) {
		for (bar = m->bar; bar; bar = bar->next)
			bar->by = -bh - y_pad;
		return;
	}

	for (num_bars = 0, bar = m->bar; bar; bar = bar->next, num_bars++)
		if (bar->topbar)
			m->wy = m->my + bh + y_pad;
	m->wh = m->wh - y_pad * num_bars - bh * num_bars;

	for (bar = m->bar; bar; bar = bar->next)
		bar->by = (bar->topbar ? m->wy - bh : m->wy + m->wh);
}

void
updateclientlist()
{
	Client *c;
	Monitor *m;

	XDeleteProperty(dpy, root, netatom[NetClientList]);
	for (m = mons; m; m = m->next)
		for (c = m->clients; c; c = c->next)
			XChangeProperty(dpy, root, netatom[NetClientList],
				XA_WINDOW, 32, PropModeAppend,
				(unsigned char *) &(c->win), 1);
}

int
updategeom(void)
{
	int dirty = 0;

#ifdef XINERAMA
	if (XineramaIsActive(dpy)) {
		int i, j, n, nn;
		Client *c;
		Monitor *m;
		XineramaScreenInfo *info = XineramaQueryScreens(dpy, &nn);
		XineramaScreenInfo *unique = NULL;

		for (n = 0, m = mons; m; m = m->next, n++);
		/* only consider unique geometries as separate screens */
		unique = ecalloc(nn, sizeof(XineramaScreenInfo));
		for (i = 0, j = 0; i < nn; i++)
			if (isuniquegeom(unique, j, &info[i]))
				memcpy(&unique[j++], &info[i], sizeof(XineramaScreenInfo));
		XFree(info);
		nn = j;
		if (n <= nn) { /* new monitors available */
			for (i = 0; i < (nn - n); i++) {
				for (m = mons; m && m->next; m = m->next);
				if (m)
					m->next = createmon();
				else
					mons = createmon();
			}
			for (i = 0, m = mons; i < nn && m; m = m->next, i++)
				if (i >= n
				|| unique[i].x_org != m->mx || unique[i].y_org != m->my
				|| unique[i].width != m->mw || unique[i].height != m->mh)
				{
					dirty = 1;
					m->num = i;
					m->mx = m->wx = unique[i].x_org;
					m->my = m->wy = unique[i].y_org;
					m->mw = m->ww = unique[i].width;
					m->mh = m->wh = unique[i].height;
					updatebarpos(m);
				}
		} else { /* less monitors available nn < n */
			for (i = nn; i < n; i++) {
				for (m = mons; m && m->next; m = m->next);
				while ((c = m->clients)) {
					dirty = 1;
					m->clients = c->next;
					detachstack(c);
					c->mon = mons;
					attach(c);
					attachstack(c);
				}
				if (m == selmon)
					selmon = mons;
				cleanupmon(m);
			}
		}
		free(unique);
	} else
#endif /* XINERAMA */
	{ /* default monitor setup */
		if (!mons)
			mons = createmon();
		if (mons->mw != sw || mons->mh != sh) {
			dirty = 1;
			mons->mw = mons->ww = sw;
			mons->mh = mons->wh = sh;
			updatebarpos(mons);
		}
	}
	if (dirty) {
		selmon = mons;
		selmon = wintomon(root);
	}
	return dirty;
}

void
updatenumlockmask(void)
{
	unsigned int i, j;
	XModifierKeymap *modmap;

	numlockmask = 0;
	modmap = XGetModifierMapping(dpy);
	for (i = 0; i < 8; i++)
		for (j = 0; j < modmap->max_keypermod; j++)
			if (modmap->modifiermap[i * modmap->max_keypermod + j]
				== XKeysymToKeycode(dpy, XK_Num_Lock))
				numlockmask = (1 << i);
	XFreeModifiermap(modmap);
}

void
updatesizehints(Client *c)
{
	long msize;
	XSizeHints size;

	if (!XGetWMNormalHints(dpy, c->win, &size, &msize))
		/* size is uninitialized, ensure that size.flags aren't used */
		size.flags = PSize;
	if (size.flags & PBaseSize) {
		c->basew = size.base_width;
		c->baseh = size.base_height;
	} else if (size.flags & PMinSize) {
		c->basew = size.min_width;
		c->baseh = size.min_height;
	} else
		c->basew = c->baseh = 0;
	if (size.flags & PResizeInc) {
		c->incw = size.width_inc;
		c->inch = size.height_inc;
	} else
		c->incw = c->inch = 0;
	if (size.flags & PMaxSize) {
		c->maxw = size.max_width;
		c->maxh = size.max_height;
	} else
		c->maxw = c->maxh = 0;
	if (size.flags & PMinSize) {
		c->minw = size.min_width;
		c->minh = size.min_height;
	} else if (size.flags & PBaseSize) {
		c->minw = size.base_width;
		c->minh = size.base_height;
	} else
		c->minw = c->minh = 0;
	if (size.flags & PAspect) {
		c->mina = (float)size.min_aspect.y / size.min_aspect.x;
		c->maxa = (float)size.max_aspect.x / size.max_aspect.y;
	} else
		c->maxa = c->mina = 0.0;
	c->isfixed = (c->maxw && c->maxh && c->maxw == c->minw && c->maxh == c->minh);
}

void
updatestatus(void)
{
	Monitor *m;
	if (!gettextprop(root, XA_WM_NAME, stext, sizeof(stext)))
		strcpy(stext, "dwm-"VERSION);
	for (m = mons; m; m = m->next)
		drawbar(m);
}

void
updatetitle(Client *c)
{
	if (!gettextprop(c->win, netatom[NetWMName], c->name, sizeof c->name))
		gettextprop(c->win, XA_WM_NAME, c->name, sizeof c->name);
	if (c->name[0] == '\0') /* hack to mark broken clients */
		strcpy(c->name, broken);
}

void
updatewindowtype(Client *c)
{
	Atom state = getatomprop(c, netatom[NetWMState]);
	Atom wtype = getatomprop(c, netatom[NetWMWindowType]);

	if (state == netatom[NetWMFullscreen])
		setfullscreen(c, 1);
	if (wtype == netatom[NetWMWindowTypeDialog])
		c->isfloating = 1;
}

void
updatewmhints(Client *c)
{
	XWMHints *wmh;

	if ((wmh = XGetWMHints(dpy, c->win))) {
		if (c == selmon->sel && wmh->flags & XUrgencyHint) {
			wmh->flags &= ~XUrgencyHint;
			XSetWMHints(dpy, c->win, wmh);
		} else {
			c->isurgent = (wmh->flags & XUrgencyHint) ? 1 : 0;
			if (c->isurgent)
				XSetWindowBorder(dpy, c->win, scheme[SchemeUrg][ColBorder].pixel);
		}
		if (wmh->flags & InputHint)
			c->neverfocus = !wmh->input;
		else
			c->neverfocus = 0;
		XFree(wmh);
	}
}

void
view(const Arg *arg)
{
	int i;
	unsigned int tmptag;

	if ((arg->ui & TAGMASK) == selmon->tagset[selmon->seltags])
		return;
	selmon->seltags ^= 1; /* toggle sel tagset */
	if (arg->ui & TAGMASK) {
		selmon->tagset[selmon->seltags] = arg->ui & TAGMASK;
		selmon->pertag->prevtag = selmon->pertag->curtag;

		if (arg->ui == ~0)
			selmon->pertag->curtag = 0;
		else {
			for (i = 0; !(arg->ui & 1 << i); i++) ;
			selmon->pertag->curtag = i + 1;
		}
	} else {
		tmptag = selmon->pertag->prevtag;
		selmon->pertag->prevtag = selmon->pertag->curtag;
		selmon->pertag->curtag = tmptag;
	}

	selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag];
	selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag];
	selmon->sellt = selmon->pertag->sellts[selmon->pertag->curtag];
	selmon->lt[selmon->sellt] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt];
	selmon->lt[selmon->sellt^1] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt^1];

	if (selmon->showbar != selmon->pertag->showbars[selmon->pertag->curtag])
		togglebar(NULL);

	focus(NULL);
	arrange(selmon);
}

pid_t
winpid(Window w)
{
	pid_t result = 0;

#ifdef __linux__
	xcb_res_client_id_spec_t spec = {0};
	spec.client = w;
	spec.mask = XCB_RES_CLIENT_ID_MASK_LOCAL_CLIENT_PID;

	xcb_generic_error_t *e = NULL;
	xcb_res_query_client_ids_cookie_t c = xcb_res_query_client_ids(xcon, 1, &spec);
	xcb_res_query_client_ids_reply_t *r = xcb_res_query_client_ids_reply(xcon, c, &e);

	if (!r)
		return (pid_t)0;

	xcb_res_client_id_value_iterator_t i = xcb_res_query_client_ids_ids_iterator(r);
	for (; i.rem; xcb_res_client_id_value_next(&i)) {
		spec = i.data->spec;
		if (spec.mask & XCB_RES_CLIENT_ID_MASK_LOCAL_CLIENT_PID) {
			uint32_t *t = xcb_res_client_id_value_value(i.data);
			result = *t;
			break;
		}
	}

	free(r);

	if (result == (pid_t)-1)
		result = 0;
#endif /* __linux__ */
#ifdef __OpenBSD__
	Atom type;
	int format;
	unsigned long len, bytes;
	unsigned char *prop;
	pid_t ret;

	if (XGetWindowProperty(dpy, w, XInternAtom(dpy, "_NET_WM_PID", 0), 0, 1, False, AnyPropertyType, &type, &format, &len, &bytes, &prop) != Success || !prop)
		return 0;

	ret = *(pid_t*)prop;
	XFree(prop);
	result = ret;
#endif /* __OpenBSD__ */

	return result;
}

Client *
wintoclient(Window w)
{
	Client *c;
	Monitor *m;

	for (m = mons; m; m = m->next)
		for (c = m->clients; c; c = c->next)
			if (c->win == w)
				return c;
	return NULL;
}

Monitor *
wintomon(Window w)
{
	int x, y;
	Client *c;
	Monitor *m;
	Bar *bar;

	if (w == root && getrootptr(&x, &y))
		return recttomon(x, y, 1, 1);
	for (m = mons; m; m = m->next)
		for (bar = m->bar; bar; bar = bar->next)
			if (w == bar->win)
				return m;
	if ((c = wintoclient(w)))
		return c->mon;
	return selmon;
}

/* There's no way to check accesses to destroyed windows, thus those cases are
 * ignored (especially on UnmapNotify's). Other types of errors call Xlibs
 * default error handler, which may call exit. */
int
xerror(Display *dpy, XErrorEvent *ee)
{
	if (ee->error_code == BadWindow
	|| (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
	|| (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable)
	|| (ee->request_code == X_PolyFillRectangle && ee->error_code == BadDrawable)
	|| (ee->request_code == X_PolySegment && ee->error_code == BadDrawable)
	|| (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
	|| (ee->request_code == X_GrabButton && ee->error_code == BadAccess)
	|| (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
	|| (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
		return 0;
	fprintf(stderr, "dwm: fatal error: request code=%d, error code=%d\n",
		ee->request_code, ee->error_code);
	return xerrorxlib(dpy, ee); /* may call exit */
}

int
xerrordummy(Display *dpy, XErrorEvent *ee)
{
	return 0;
}

/* Startup Error handler to check if another window manager
 * is already running. */
int
xerrorstart(Display *dpy, XErrorEvent *ee)
{
	die("dwm: another window manager is already running");
	return -1;
}

void
xrdb(const Arg *arg)
{
	loadxresources();
	int i;
	for (i = 0; i < LENGTH(colors); i++)
		scheme[i] = drw_scm_create(drw, colors[i], 3);
	focus(NULL);
	arrange(NULL);
}

void
zoom(const Arg *arg)
{
	Client *c = selmon->sel;

	if (!selmon->lt[selmon->sellt]->arrange
	|| (selmon->sel && selmon->sel->isfloating))
		return;
	if (c == nexttiled(selmon->clients))
		if (!c || !(c = nexttiled(c->next)))
			return;
	pop(c);
}

int
main(int argc, char *argv[])
{
	if (argc == 2 && !strcmp("-v", argv[1]))
		die("dwm-"VERSION);
	else if (argc != 1)
		die("usage: dwm [-v]");
	if (!setlocale(LC_CTYPE, "") || !XSupportsLocale())
		fputs("warning: no locale support\n", stderr);
	if (!(dpy = XOpenDisplay(NULL)))
		die("dwm: cannot open display");
	if (!(xcon = XGetXCBConnection(dpy)))
		die("dwm: cannot get xcb connection\n");
	checkotherwm();
	XrmInitialize();
	loadxresources();
	setup();
#ifdef __OpenBSD__
	if (pledge("stdio rpath proc exec ps", NULL) == -1)
		die("pledge");
#endif /* __OpenBSD__ */
	scan();
	runautostart();
	run();
	if(restart) execvp(argv[0], argv);
	cleanup();
	XCloseDisplay(dpy);
	return EXIT_SUCCESS;
}
