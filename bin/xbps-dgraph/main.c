/*-
 * Copyright (c) 2010-2012 Juan Romero Pardines.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <unistd.h>
#include <inttypes.h>

#include <xbps_api.h>

#ifndef __arraycount
# define __arraycount(a) (sizeof(a) / sizeof(*(a)))
#endif

#define _DGRAPH_CFFILE	"xbps-dgraph.conf"

/*
 * Object key for optional objects in package dictionary.
 */
static const char *optional_objs[] = {
	"conflicts", "conf_files", "replaces", "run_depends", "preserve",
	"requiredby", "provides", "homepage", "license"
};

/*
 * Properties written to default configuration file.
 */
struct defprops {
	const char *sect;
	const char *prop;
	const char *val;
} dfprops[] = {
	{ .sect = "graph", .prop = "rankdir", .val = "LR" },
	{ .sect = "graph", .prop = "ranksep", .val = ".1" },
	{ .sect = "graph", .prop = "nodesep", .val = ".1" },
	{ .sect = "graph", .prop = "pack", .val = "true" },
	{ .sect = "graph", .prop = "splines", .val = "polyline" },
	{ .sect = "graph", .prop = "ratio", .val = "compress" },

	{ .sect = "edge", .prop = "constraint", .val = "true" },
	{ .sect = "edge", .prop = "arrowhead", .val = "vee" },
	{ .sect = "edge", .prop = "arrowsize", .val = ".4" },
	{ .sect = "edge", .prop = "fontname", .val = "Sans" },
	{ .sect = "edge", .prop = "fontsize", .val = "8" },

	{ .sect = "node", .prop = "height", .val = ".1" },
	{ .sect = "node", .prop = "width", .val = ".1" },
	{ .sect = "node", .prop = "shape", .val = "box" },
	{ .sect = "node", .prop = "fontname", .val = "Sans" },
	{ .sect = "node", .prop = "fontsize", .val = "8" },

	{ .sect = "node-sub", .prop = "main-style", .val = "filled" },
	{ .sect = "node-sub", .prop = "main-fillcolor", .val = "darksalmon" },
	{ .sect = "node-sub", .prop = "style", .val = "filled" },
	{ .sect = "node-sub", .prop = "fillcolor", .val = "yellowgreen" },
	{ .sect = "node-sub", .prop = "opt-style", .val = "filled" },
	{ .sect = "node-sub", .prop = "opt-fillcolor", .val = "grey" }
};

static void __attribute__((noreturn))
die(const char *fmt, ...)
{
	va_list ap;
	int save_errno = errno;

	va_start(ap, fmt);
	fprintf(stderr, "xbps-dgraph: ERROR ");
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, " (%s)\n", strerror(save_errno));
	va_end(ap);
	exit(EXIT_FAILURE);
}

static void __attribute__((noreturn))
usage(void)
{
	fprintf(stdout,
	"Usage: xbps-dgraph [options] <pkgname>\n\n"
	" Options\n"
	"    -c\t\tPath to configuration file\n"
	"    -g\t\tGenerate a default config file\n"
	"    -o\t\tOutput to this file (<pkgname>.dot set by default)\n"
	"    -R\t\tAlso generate reverse dependencies in the graph\n"
	"    -r\t\t<rootdir>\n\n");
	exit(EXIT_FAILURE);
}

static const char *
convert_proptype_to_string(prop_object_t obj)
{
	switch (prop_object_type(obj)) {
	case PROP_TYPE_ARRAY:
		return "array";
	case PROP_TYPE_BOOL:
		return "bool";
	case PROP_TYPE_DICTIONARY:
		return "dictionary";
	case PROP_TYPE_DICT_KEYSYM:
		return "dictionary key";
	case PROP_TYPE_NUMBER:
		return "integer";
	case PROP_TYPE_STRING:
		return "string";
	default:
		return NULL;
	}
}

static void
generate_conf_file(void)
{
	prop_dictionary_t d, d2;
	struct defprops *dfp;
	size_t i;
	const char *outfile = "xbps-dgraph.conf";

	d = prop_dictionary_create();

	d2 = prop_dictionary_create();
	prop_dictionary_set(d, "graph", d2);
	prop_object_release(d2);

	d2 = prop_dictionary_create();
	prop_dictionary_set(d, "edge", d2);
	prop_object_release(d2);

	d2 = prop_dictionary_create();
	prop_dictionary_set(d, "node", d2);
	prop_object_release(d2);

	d2 = prop_dictionary_create();
	prop_dictionary_set(d, "node-sub", d2);
	prop_object_release(d2);

	for (i = 0; i < __arraycount(dfprops); i++) {
		dfp = &dfprops[i];
		d2 = prop_dictionary_get(d, dfp->sect);
		prop_dictionary_set_cstring_nocopy(d2, dfp->prop, dfp->val);
	}

	if (prop_dictionary_externalize_to_file(d, outfile) == false) {
		prop_object_release(d);
		die("couldn't write conf_file to %s", outfile);
	}
	prop_object_release(d);
	printf("Wrote configuration file: %s\n", _DGRAPH_CFFILE);
}

static void
write_conf_property_on_stream(FILE *f,
			      const char *section,
			      prop_dictionary_t confd)
{
	prop_array_t allkeys, allkeys2;
	prop_dictionary_keysym_t dksym, dksym2;
	prop_object_t keyobj, keyobj2;
	size_t i, x;
	const char *cf_val, *keyname, *keyname2;

	/*
	 * Iterate over the main dictionary.
	 */
	allkeys = prop_dictionary_all_keys(confd);
	for (i = 0; i < prop_array_count(allkeys); i++) {
		dksym = prop_array_get(allkeys, i);
		keyname = prop_dictionary_keysym_cstring_nocopy(dksym);
		keyobj = prop_dictionary_get_keysym(confd, dksym);
		if (strcmp(keyname, section))
			continue;

		/*
		 * Iterate over the dictionary sections [edge/graph/node].
		 */
		allkeys2 = prop_dictionary_all_keys(keyobj);
		for (x = 0; x < prop_array_count(allkeys2); x++) {
			dksym2 = prop_array_get(allkeys2, x);
			keyname2 = prop_dictionary_keysym_cstring_nocopy(dksym2);
			keyobj2 = prop_dictionary_get_keysym(keyobj, dksym2);

			cf_val = prop_string_cstring_nocopy(keyobj2);
			fprintf(f, "%s=\"%s\"", keyname2, cf_val);
			if (x + 1 >= prop_array_count(allkeys2))
				continue;

			fprintf(f, ",");
		}
	}
}

static char *
strip_dashes_from_key(const char *str)
{
	char *p;
	size_t i;

	p = strdup(str);
	if (p == NULL)
		die("%s alloc p", __func__);

	for (i = 0; i < strlen(p); i++) {
		if (p[i] == '-')
			p[i] = '_';
	}
	return p;
}

static void
parse_array_in_pkg_dictionary(FILE *f, prop_dictionary_t plistd,
			      prop_dictionary_t sub_confd,
			      prop_array_t allkeys,
			      bool parse_pkgdb)
{
	prop_dictionary_keysym_t dksym;
	prop_object_t keyobj, sub_keyobj;
	size_t i, x;
	const char *tmpkeyname, *cfprop, *optnodetmp;
	char *optnode, *keyname;

	for (i = 0; i < prop_array_count(allkeys); i++) {
		dksym = prop_array_get(allkeys, i);
		tmpkeyname = prop_dictionary_keysym_cstring_nocopy(dksym);

		/*
		 * While parsing package's dictionary from pkgdb, we are
		 * only interested in the "automatic-install" and "requiredby"
		 * objects.
		 */
		if (parse_pkgdb &&
		    (strcmp(tmpkeyname, "automatic-install")) &&
		    (strcmp(tmpkeyname, "requiredby")))
			continue;

		keyobj = prop_dictionary_get_keysym(plistd, dksym);
		keyname = strip_dashes_from_key(tmpkeyname);
		optnode = NULL;

		fprintf(f, "	main -> %s [label=\"%s\"];\n",
		    keyname, convert_proptype_to_string(keyobj));

		prop_dictionary_get_cstring_nocopy(sub_confd, "opt-style", &cfprop);
		/* Check if object is optional and fill it in */
		for (x = 0; x < __arraycount(optional_objs); x++) {
			if (strcmp(keyname, optional_objs[x]) == 0) {
				optnode = xbps_xasprintf("[style=\"%s\"",
				    cfprop);
				break;
			}
		}
		optnodetmp = optnode;

		/*
		 * We can assume that all arrays only contain strings, so
		 * this can be simplified.
		 */
		prop_dictionary_get_cstring_nocopy(sub_confd, "style", &cfprop);
		if (prop_object_type(keyobj) == PROP_TYPE_ARRAY) {
			if (optnodetmp)
				fprintf(f, "	%s %s];\n", keyname,
				    optnodetmp);

			for (x = 0; x < prop_array_count(keyobj); x++) {
				sub_keyobj = prop_array_get(keyobj, x);
				fprintf(f, "	%s -> %s_%zu_string "
				    "[label=\"string\"];\n",
				    keyname, keyname, x);
				prop_dictionary_get_cstring_nocopy(sub_confd,
				    "style", &cfprop);
				fprintf(f, "	%s_%zu_string [style=\"%s\",",
				    keyname, x, cfprop);
				prop_dictionary_get_cstring_nocopy(sub_confd,
				    "fillcolor", &cfprop);
				fprintf(f, "fillcolor=\"%s\","
				    "label=\"%s\"];\n", cfprop,
				    prop_string_cstring_nocopy(sub_keyobj));
			}
			if (optnode)
				free(optnode);
			free(keyname);
			continue;
		}

		if (optnodetmp) {
			fprintf(f, "	%s %s];\n", keyname, optnodetmp);
			fprintf(f, "	%s -> %s_value %s];\n", keyname, keyname,
		    	    optnode);
		} else
			fprintf(f, "	%s -> %s_value;\n", keyname, keyname);

		prop_dictionary_get_cstring_nocopy(sub_confd, "style", &cfprop);
		fprintf(f, "	%s_value [style=\"%s\",", keyname, cfprop);
		prop_dictionary_get_cstring_nocopy(sub_confd,
		    "fillcolor", &cfprop);
		fprintf(f, "fillcolor=\"%s\"", cfprop);

		/*
		 * Process all other object types...
		 */
		switch (prop_object_type(keyobj)) {
		case PROP_TYPE_BOOL:
			fprintf(f, ",label=\"%s\"",
			    prop_bool_true(keyobj) ? "true" : "false");
			break;
		case PROP_TYPE_NUMBER:
			fprintf(f, ",label=\"%"PRIu64"\"",
			    prop_number_unsigned_integer_value(keyobj));
			break;
		case PROP_TYPE_STRING:
			if (strcmp(keyname, "long_desc") == 0) {
				/*
				 * Do not print this obj, too large!
				 */
				fprintf(f, ",label=\"...\"");
			} else {
				fprintf(f, ",label=\"%s\"",
			    	    prop_string_cstring_nocopy(keyobj));
			}
			break;
		default:
			break;
		}
		fprintf(f, "];\n");

		free(keyname);
		if (optnode)
			free(optnode);
	}
}

static void
create_dot_graph(struct xbps_handle *xhp,
		 FILE *f,
		 prop_dictionary_t plistd,
		 prop_dictionary_t confd,
		 bool revdeps)
{
	prop_dictionary_t sub_confd, regpkgd = NULL;
	prop_array_t allkeys;
	const char *pkgver, *pkgn, *cfprop;

	prop_dictionary_get_cstring_nocopy(plistd, "pkgver", &pkgver);
	prop_dictionary_get_cstring_nocopy(plistd, "pkgname", &pkgn);

	/*
	 * Start filling the output file...
	 */
	fprintf(f, "/* Graph created for %s by xbps-graph %s */\n\n",
	    pkgver, XBPS_RELVER);
	fprintf(f, "digraph pkg_dictionary {\n");

	/*
	 * Process the graph section in config file.
	 */
	fprintf(f, "	graph [");
	write_conf_property_on_stream(f, "graph", confd);
	fprintf(f, ",label=\"[XBPS] %s metadata properties\"];\n", pkgver);

	/*
	 * Process the edge section in config file.
	 */
	fprintf(f, "	edge [");
	write_conf_property_on_stream(f, "edge", confd);
	fprintf(f, "];\n");

	/*
	 * Process the node section in config file.
	 */
	fprintf(f, "	node [");
	write_conf_property_on_stream(f, "node", confd);
	fprintf(f, "];\n");

	/*
	 * Process the node-sub section in config file.
	 */
	fprintf(f, "	main [");
	sub_confd = prop_dictionary_get(confd, "node-sub");
	prop_dictionary_get_cstring_nocopy(sub_confd, "main-style", &cfprop);
	if (cfprop)
		fprintf(f, "style=%s,", cfprop);
	prop_dictionary_get_cstring_nocopy(sub_confd, "main-fillcolor", &cfprop);
	if (cfprop)
		fprintf(f, "fillcolor=\"%s\",", cfprop);
	fprintf(f, "label=\"Dictionary\"];\n");

	/*
	 * Process all objects in package's dictionary from its metadata
	 * property list file, aka XBPS_META_PATH/metadata/<pkgname>/XBPS_PKGPROPS.
	 */
	allkeys = prop_dictionary_all_keys(plistd);
	parse_array_in_pkg_dictionary(f, plistd, sub_confd, allkeys, false);

	/*
	 * Process all objects in package's dictionary from pkgdb property
	 * list file, aka XBPS_META_PATH/XBPS_PKGDB.
	 */
	if (revdeps) {
		regpkgd = xbps_find_pkg_dict_installed(xhp, pkgn, false);
		if (regpkgd == NULL)
			die("cannot find '%s' dictionary on %s!",
			    pkgn, XBPS_PKGDB);

		allkeys = prop_dictionary_all_keys(regpkgd);
		parse_array_in_pkg_dictionary(f, regpkgd, sub_confd,
		    allkeys, true);
	}
	/*
	 * Terminate the stream...
	 */
	fprintf(f, "}\n");
	fflush(f);
	fclose(f);
}

int
main(int argc, char **argv)
{
	prop_dictionary_t plistd, confd = NULL;
	struct xbps_handle xh;
	FILE *f = NULL;
	char *outfile = NULL;
	const char *conf_file = NULL, *rootdir = NULL;
	int c, rv;
	bool revdeps = false;

	while ((c = getopt(argc, argv, "c:gRr:o:")) != -1) {
		switch (c) {
		case 'c':
			/* Configuration file. */
			conf_file = optarg;
			break;
		case 'g':
			/* Generate auto conf file. */
			generate_conf_file();
			exit(EXIT_SUCCESS);
		case 'o':
			/* Output to this file. */
			outfile = optarg;
			break;
		case 'R':
			/* Also create graphs for reverse deps. */
			revdeps = true;
			break;
		case 'r':
			/* Set different rootdir. */
			rootdir = optarg;
			break;
		case '?':
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();

	/* Initialize libxbps */
	memset(&xh, 0, sizeof(xh));
	xh.rootdir = rootdir;
	if ((rv = xbps_init(&xh)) != 0)
		die("failed to initialize libxbps: %s", strerror(rv));

	/*
	 * Output file will be <pkgname>.dot if not specified.
	 */
	if (outfile == NULL) {
		outfile = xbps_xasprintf("%s.dot", argv[0]);
	}

	/*
	 * If -c not set, try to read it from cwd.
	 */
	if (conf_file == NULL)
		conf_file = _DGRAPH_CFFILE;

	/*
	 * Internalize the configuration file.
	 */
	confd = prop_dictionary_internalize_from_zfile(conf_file);
	if (confd == NULL)
		die("cannot read conf file `%s'", conf_file);

	/*
	 * Internalize the plist file of the target installed package.
	 */
	plistd = xbps_metadir_get_pkgd(&xh, argv[0]);
	if (plistd == NULL)
		die("cannot internalize %s metadata file", argv[0]);

	/*
	 * Create the output FILE.
	 */
	if ((f = fopen(outfile, "w")) == NULL)
		die("cannot create target file '%s'", outfile);

	/*
	 * Create the dot(1) graph!
	 */
	create_dot_graph(&xh, f, plistd, confd, revdeps);

	prop_object_release(confd);
	xbps_end(&xh);

	exit(EXIT_SUCCESS);
}
