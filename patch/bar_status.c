int
width_status(Bar *bar, BarWidthArg *a)
{
	return TEXTW(stext);
}


int
draw_status(Bar *bar, BarDrawArg *a)
{
	return drw_text(drw, a->x, 0, a->w, bh, lrpad / 2, stext, 0);
}


int
click_status(Bar *bar, Arg *arg, BarClickArg *a)
{
	return ClkStatusText;
}
