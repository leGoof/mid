// Copyright © 2011 Steve McCoy and Ethan Burns
// Licensed under the MIT License. See LICENSE for details.
#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include "../../include/mid.h"
#include "../../include/log.h"
#include "lvlgen.h"

static bool segok(Lvl *l, Path *p, Seg s);
static bool segconfl(Lvl *l, Path *p, Seg s);
static bool segclr(Lvl *l, Seg s);
static bool beenthere(Lvl *l, Path *p, Seg s);

Seg segmk(Loc l, Mv *m)
{
	Seg s;
	s.l0 = l;
	s.l1 = (Loc) { l.x + m->dx, l.y + m->dy };
	s.mv = m;
	return s;
}

void segpr(Seg s)
{
	pr("seg: %d,%d -> %d,%d", s.l0.x, s.l0.y, s.l1.x, s.l1.y);
}

Path *pathnew(Lvl *l)
{
	Path *p = xalloc(1, sizeof(*p));
	p->maxsegs = l->w * l->h;
	p->segs = xalloc(p->maxsegs, sizeof(p->segs[0]));
	return p;
}

void pathfree(Path *p)
{
	free(p->segs);
	free(p);
}

bool pathadd(Lvl *l, Path *p, Seg s)
{
	if (p->nsegs == p->maxsegs || !segok(l, p, s))
		return false;

	mvblit(s.mv, l, s.l0);

	p->segs[p->nsegs] = s;
	p->nsegs++;

	return true;
}

static bool segok(Lvl *l, Path *p, Seg s)
{
	assert (s.l0.x > 0 && s.l0.x < l->w - 1);
	assert (s.l0.y > 0 && s.l0.y < l->h - 1);

	if (p->nsegs == 0 && !startonblk(s.mv))
		return false;

	bool inrange = s.l1.x > 0 && s.l1.x < l->w - 1
		&& s.l1.y > 0 && s.l1.y < l->h - 1;

	return inrange && segclr(l, s) && !segconfl(l, p, s)
		&& !beenthere(l, p, s);
}

/* Conflict with other path segments? */
static bool segconfl(Lvl *l, Path *p, Seg s)
{
	for (int i = 0; i < s.mv->nblkd; i++) {
		Loc *bp = &s.mv->blkd[i];
		Loc blk = (Loc) { bp->x + s.l0.x, bp->y + s.l0.y };
		if (blk.x < 0 || blk.x >= l->w || blk.y < 0 || blk.y >= l->h)
			return false;
		if (used(l, blk))
			return true;
	}

	return false;
}

/* Test if the segment area is clr. */
static bool segclr(Lvl *l, Seg s)
{
	for (int i = 0; i < s.mv->nclr; i++) {
		Loc *bp = &s.mv->clr[i];
		Loc blk = (Loc) { bp->x + s.l0.x, bp->y + s.l0.y };
		if (blk.x < 0 || blk.x >= l->w || blk.y < 0 || blk.y >= l->h)
			return false;
		Tileinfo b = tileinfo(l, blk.x, blk.y, l->z);
		if (b.flags & Tilecollide)
			return false;
	}

	return true;
}

static bool beenthere(Lvl *l, Path *p, Seg s)
{
	for (int i = 0; i < p->nsegs; i++) {
		if (used(l, s.l1))
			return true;
	}
	return false;
}

bool used(Lvl *l, Loc loc)
{
	int t = blk(l, loc.x, loc.y, l->z)->tile;
	return t == ' ' || t == '<' || t == '>';
}
