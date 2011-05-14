#include "../../include/mid.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>

enum { Theight = 32, Twidth = 32 };

enum { Collide = 1<<0,
       Bkgrnd = 1<<1,
       Water = 1<<2,
};

typedef struct Tinfo Tinfo;
struct Tinfo {
	char *file;
	Anim *anim;
	unsigned int flags;
};

static Tinfo *tiles[] = {
	[' '] = &(Tinfo){ .file = "anim/blank/anim", .flags = Bkgrnd },
	['l'] = &(Tinfo){ .file = "anim/land/anim", .flags = Collide|Bkgrnd },
	['w'] = &(Tinfo){ .file = "anim/water/anim", .flags = Water },
};

const int ntiles = sizeof(tiles) / sizeof(tiles[0]);

bool istile(int t)
{
	return t >= 0 && t < ntiles && tiles[t];
}

struct Lvl {
	int d, w, h;
	char tiles[];
};

Lvl *lvlnew(int d, int w, int h)
{
	Lvl *l = malloc(sizeof(*l) + sizeof(char[d * w * h]));
	if (!l)
		return NULL;
	l->d = d;
	l->w = w;
	l->h = h;
	return l;
}

void lvlfree(Lvl *l)
{
	free(l);
}

int tileread(FILE *f)
{
	int c = fgetc(f);
	if (c == EOF) {
		seterrstr("Unexpected EOF");
		goto err;
	}

	if (!istile(c)) {
		seterrstr("Invalid tile: %c\n", c);
		goto err;
	}
	return c;
err:
	return -1;
}


Lvl *lvlread(FILE *f)
{
	int w, h, d;
	fscanf(f, " %d %d %d",&d, &w, &h);
	Lvl *l = lvlnew(d, w, h);
	if (!l)
		return NULL;
	int x, y, z;
	x = y = z = 0;
	if (fgetc(f) != '\n')
		goto errnl;
	for (z = 0; z < d; z++) {
		int base = z * w * h;
		for (y = 0; y < h; y++) {
			for (x = 0; x < w; x++){
				int ind = base + x * h + y;
				int c = tileread(f);
				if (c < 0)
					goto err;
				l->tiles[ind] = c;
			}
			if (fgetc(f) != '\n')
				goto errnl;
		}
		if (z < d - 1 && fgetc(f) != '\n')
			goto errnl;
	}
	return l;
errnl:
	seterrstr("Expected newline in level file: z=%d, x=%d, y=%d", z, x, y);
err:
	free(l);
	return NULL;
}

Lvl *lvlload(const char *path)
{
	FILE *f = fopen(path, "r");
	if (!f)
		return NULL;
	Lvl *l = lvlread(f);
	int err = errno;
	fclose(f);
	errno = err;
	return  l;
}

void drawtile(Gfx *g, Rtab *anims, int t, Point pt)
{
	if (tiles[t]->anim == NULL)
		tiles[t]->anim = resrcacq(anims, tiles[t]->file, NULL);
	if (!tiles[t]->anim)
		abort();
	animdraw(g, tiles[t]->anim, pt);
}

void bkgrnddraw(Gfx *g, Rtab *anims, int t, Point pt)
{
	if (!tiles[t])
		return;
	if (!(tiles[t]->flags & Bkgrnd)) {
		/* White background behind foreground tiles so
		 * blending works correctly. */
		Rect r = (Rect){{pt.x, pt.y}, {pt.x + Twidth, pt.y + Theight}};
		gfxfillrect(g, r, (Color){255,255,255,255});
		return;
	}
	drawtile(g, anims, t, pt);
}
void fgrnddraw(Gfx *g, Rtab *anims, int t, Point pt)
{
	if (!tiles[t] || tiles[t]->flags & Bkgrnd)
		return;
	drawtile(g, anims, t, pt);
}

void lvldraw(Gfx *g, Rtab *anims, Lvl *l, int z, bool bkgrnd, Point offs)
{
	int w = l->w, h = l->h;
	int base = z * w * h;
	for (int x = 0; x < w; x++){
		int pxx = offs.x + x * Twidth;
		for (int y = 0; y < h; y++) {
			int ind = base + x * h + y;
			int t = l->tiles[ind];
			Point pt = (Point){ pxx, offs.y + y * Theight };
			if (bkgrnd)
				bkgrnddraw(g, anims, t, pt);
			else
				fgrnddraw(g, anims, t, pt);
		}
	}
}

void lvlupdate(Rtab *anims, Lvl *l)
{
	for (int i = 0; i < sizeof(tiles) / sizeof(tiles[0]); i++)
		if (tiles[i] && tiles[i]->anim)
			animupdate(tiles[i]->anim, 1);
}

Rect tilebbox(int x, int y)
{
	Point a = (Point) {x * Twidth, y * Theight};
	Point b = (Point) {(x + 1) * Twidth, (y + 1) * Theight};
	return (Rect){ a, b };
}

Isect tileisect(int t, int x, int y, Rect r)
{
	assert(tiles[t]);
	if (!(tiles[t]->flags & Collide))
		return (Isect){ .is = false };
	return minisect(tilebbox(x, y), r);
}

Isect tilesisect(Lvl *l, int z, int xmin, int ymin, int xmax, int ymax, Rect r)
{
	bool isect = false;
	float dx = 0.0, dy = 0.0;
	for (int x = xmin; x <= xmax; x++) {
		for (int y = ymin; y <= ymax; y++) {
			int i = z * l->h * l->w + x * l->h + y;
			Isect m = tileisect(l->tiles[i], x, y, r);
			if (!m.is)
				continue;
			isect = true;
			printf("m.dx=%g, m.dy=%g, dx=%g, dy=%g\n", m.dx, m.dy,
			       dx, dy);
			if (m.dx > 0 && m.dx > dx)
				dx = m.dx;
			if (m.dy > 0 && m.dy > dy)
				dy = m.dy;
		}
	}

	return (Isect) { .is = isect, dx = dx, dy = dy };
}

Isect lvlisect(Lvl *l, int z, Rect r)
{
	float x0 = r.a.x < r.b.x ? r.a.x : r.b.x;
	float x1 = r.a.x > r.b.x ? r.a.x : r.b.x;
	float y0 = r.a.y < r.b.y ? r.a.y : r.b.y;
	float y1 = r.a.y > r.b.y ? r.a.y : r.b.y;
	int xmin = x0 / Twidth, xmax = (x1 + 1) / Twidth;
	int ymin = y0 / Theight, ymax = (y1 + 1) / Theight;
	return tilesisect(l, z, xmin, ymin, xmax, ymax, r);
}
