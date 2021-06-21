int
width_wintitle(Bar *bar, BarWidthArg *a)
{
	return a->max_width;
}

int
draw_wintitle(Bar *bar, BarDrawArg *a)
{
	int boxs = drw->fonts->h / 9;
	int boxw = drw->fonts->h / 6 + 2;
	int x = a->x, w = a->w;
	Monitor *m = bar->mon;

	if (m->sel) {
		drw_setscheme(drw, scheme[m == selmon ? SchemeSel : SchemeNorm]);
		drw_text(drw, x, 0, w, bh, lrpad / 2, m->sel->name, 0);
		if (m->sel->isfloating)
			drw_rect(drw, x + boxs, boxs, boxw, boxw, m->sel->isfixed, 0);
	} else {
		drw_setscheme(drw, scheme[SchemeNorm]);
		drw_rect(drw, x, 0, w, bh, 1, 1);
	}
	return x + w;
}

int
click_wintitle(Bar *bar, Arg *arg, BarClickArg *a)
{
	return ClkWinTitle;
}
