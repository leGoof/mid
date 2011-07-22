// Copyright © 2011 Steve McCoy and Ethan Burns
// Licensed under the MIT License. See LICENSE for details.
#include <assert.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../../include/mid.h"
#include "../../include/log.h"
#include "../../include/rng.h"
#include "game.h"
#include <stdio.h>
#include "../../include/os.h"

static const char *zonedir = "_zones";
enum { Bufsz = 1024 };

typedef struct Pipe {
	int n;
	char cmd[Bufsz];
} Pipe;

static FILE *inzone = NULL;

static void ensuredir();
static char *zonefile(int);
static FILE *zpipe(Rng *r);
static void pipeadd(struct Pipe *, char *, ...);

void zonestdin()
{
	inzone = stdin;
}

Zone *zonegen(Rng *r)
{
	ignframetime();
	FILE *fin = inzone;

	if (!fin)
		fin = zpipe(r);
	Zone *z = zoneread(fin);
	if (!z)
		fatal("Failed to read the zone: %s", miderrstr());

	if (inzone) {
		fclose(fin);
		inzone = NULL;
	} else {
		int ret = pipeclose(fin);
		if (ret == -1)
			fatal("Zone gen pipeline exited with failure: %s", miderrstr());
	}

	return z;
}

Zone *zoneget(int znum)
{
	ignframetime();

	char *zfile = zonefile(znum);
	FILE *f = fopen(zfile, "r");
	if (!f)
		fatal("Unable to open the zone file [%s]: %s", zfile, miderrstr());

	Zone *zn = zoneread(f);
	if (!zn)
		fatal("Failed to read the zone file [%s]: %s", zfile, miderrstr());

	fclose(f);
	return zn;
}

void zoneput(Zone *zn, int znum)
{
	ignframetime();
	ensuredir();
	
	char *zfile = zonefile(znum);
	FILE *f = fopen(zfile, "w");
	if (!f)
		fatal("Failed to open zone file for writing [%s]: %s", zfile, miderrstr());

	zonewrite(f, zn);
	fclose(f);
}

Tileinfo zonedstairs(Zone *zn)
{
	Tileinfo bi;

	for (int z = 0; z < zn->lvl->d; z++) {
	for (int x = 0; x < zn->lvl->w; x++) {
	for (int y = 0; y < zn->lvl->h; y++) {
		bi = tileinfo(zn->lvl, x, y, z);
		if (bi.flags & Tiledown)
			return bi;
	}
	}
	}
	fatal("No down stairs found in this zone");
}

void zonecleanup(int zmax)
{
	for (int i = 0; i < zmax; i++) {
		char *zfile = zonefile(i);
		if (!fsexists(zfile))
			continue;
		if (unlink(zfile) < 0)
			pr("Failed to remove zone file [%s]: %s", zfile, miderrstr());
	}
}

// Non re-entrant
static char *zonefile(int znum)
{
	static char zfile[Bufsz];
	snprintf(zfile, Bufsz, "%s/%d.zone", zonedir, znum);
	return zfile;
}

static void ensuredir()
{
	struct stat sb;
	if (stat(zonedir, &sb) < 0) {
		if (makedir(zonedir) < 0)
			fatal("Failed to make the zone directory [%s]: %s", zonedir, miderrstr());
	} else if (!S_ISDIR(sb.st_mode)) {
		fatal("Zone directory [%s] is not a directory", zonedir);
	}
}

static FILE *zpipe(Rng *r)
{
	Pipe p = {};
	pipeadd(&p, "./cmd/lvlgen/lvlgen -s %u 25 25 3", rngint(r));
	pipeadd(&p, " | ./cmd/itmgen/itmgen -s %u 1 1", rngint(r));
	pipeadd(&p, " | ./cmd/itmgen/itmgen -s %u 2 50", rngint(r));
	pipeadd(&p, " | ./cmd/itmgen/itmgen -s %u 3 10", rngint(r));
	pipeadd(&p, " | ./cmd/envgen/envgen -s %u 1 1", rngint(r));
	pipeadd(&p, " | ./cmd/enmgen/enmgen -s %u 1 50", rngint(r));

	pipeadd(&p, " | tee cur.lvl");
	pr("lvlgen pipeline: [%s]", p.cmd);

	FILE *fin = piperead(p.cmd);
	if (!fin)
		fatal("Unable to execute zone gen pipeline: %s", miderrstr());

	return fin;
}

static void pipeadd(Pipe *p, char *fmt, ...)
{
	char buf[Bufsz];
	va_list ap;

	va_start(ap, fmt);
	int n = vsnprintf(buf, Bufsz, fmt, ap);
	va_end(ap);

	if (n > Bufsz - p->n)
		fatal("Buffer is too small");

	strncat(p->cmd + p->n, buf, n);
	p->n += n;
}
