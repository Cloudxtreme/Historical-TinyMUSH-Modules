/* codelib.c - Module for loading offline softcode */
/* $Id$ */

#include "../../api.h"

#define MOD_CODELIB_CREATE	1

CF_HAND(cf_hash)
{
	char *key, *value, *p, *tokst;

	key = strtok_r(str, " \t=,", &tokst);
	value = strtok_r(NULL, " \t=,", &tokst);
	if (value) {
		if (((HASHTAB *)vp)->flags & HT_KEYREF) {
			/* hashadd won't copy it, so we do that here */
			p = key;
			key = XSTRDUP(p, "cf_hash");
		}
		p = value;
		value = XSTRDUP(p, "cf_hash");
		
		return hashadd(key, (int *)value, (HASHTAB *) vp, 0);
	} else {
		cf_log_syntax(player, cmd, "No value specified for key %s",
			      key);
		return -1;
	}
}

/* Conf table */

struct mod_codelib_confstorage {
	HASHTAB libraries;
	char *pathname;
} mod_codelib_config;

MODHASHES mod_codelib_hashtable[] = {
{ "libraries",	&mod_codelib_config.libraries,	5,	8},
{ NULL,		NULL,				0,	0}};

CONF mod_codelib_conftable[] = {
{(char *)"codelib",		cf_hash,	CA_GOD,	CA_GOD,	(int *)&mod_codelib_config.libraries,	(long)"Codelib library"},
{(char *)"codelib_path",	cf_string,	CA_GOD,	CA_GOD,	(int *)&mod_codelib_config.pathname,	MBUF_SIZE},
{ NULL,			NULL,		0,	0,		NULL,					0}};


void mod_codelib_upload_file(char *file, dbref object, dbref player) {
	FILE *f;
	char *attrtxt, *attrnam, *buf, *nam, *s, *p, *q, *tokst;
	int anum, type;
	dbref *np;
	FLAGENT *fp;
	POWERENT *pp;
	
	nam = alloc_mbuf("mod_codelib.upload_file.nam");
	buf = alloc_mbuf("mod_codelib.upload_file.buf");
	p = attrtxt = alloc_lbuf("mod_codelib.upload_file.attrtxt");
	attrnam = alloc_sbuf("mod_codelib.upload_file.attrnam");

	/* Do not allow code uploads to GOD */
	
	if (object == GOD) {
		notify(player, "Permission denied.");
		return;
	}

	/* Prepend the pathname and open the file */

	snprintf(nam, MBUF_SIZE, "%s/%s", mod_codelib_config.pathname,
		file);

 	if ((f = fopen(nam, "r")) == NULL) {
		STARTLOG(LOG_ALWAYS, "MOD", "CODELIB")
			log_printf("%s could not be opened for reading", s);
		ENDLOG
		if (player != NOTHING)
			notify(player, tprintf("%s could not be opened for reading.", s));
		return;
	}

	/* Preserve the object name */
	
	strncpy(nam, Name(object), MBUF_SIZE);

	/* Set the name of the object */

	s_Name(object, nam);

	/* Preserve the type of the object */
	
	type = Typeof(object);
	
	/* Clear the object's flags */
	
	s_Flags(object, 0);
	s_Flags2(object, 0);
	s_Flags3(object, 0);

	/* Clear the object's powers */
	
	s_Powers(object, 0);
	s_Powers2(object, 0);
	
	/* Set the object's type */
	
	s_Flags(object, type);
	
	/* Add the CODELIB attribute containing the filename */

	strcpy(attrnam, "CODELIB");
	anum = mkattr(attrnam);
	atr_add(object, anum, file, player, AF_GOD);

	/* Add an Nref for this object */

	snprintf(nam, MBUF_SIZE, "_%s", file);
	np = (int *) hashfind(nam, &mudstate.nref_htab);
	if (np) {
		XFREE(np, "nref");
		np = (int *) XMALLOC(sizeof(int), "nref");
		*np = object;
		hashrepl(nam, np, &mudstate.nref_htab);
	} else {
		np = (int *) XMALLOC(sizeof(int), "nref");
		*np = object;
		hashadd(nam, np, &mudstate.nref_htab, 0);
	}

	while (fgets(buf, MBUF_SIZE, f) != NULL) {
		s = buf;
		
		/* Skip over comments and blank lines*/
		
		if ((*buf == '#') || (*buf == '\n') || (*buf == '\0'))
			continue;
		
		/* If we see 'flags', set flags on the object */
		
		if ((*buf == 'f') || (*buf == 'F')) {
			/* Cut through to the first space, tab, or NULL */
			while ((*s != ' ') && (*s != '\t') && (*s != '\0') &&
			       (*s != '\n')) s++;
			*s = '\0';
			
			if (!strcasecmp("flags", buf)) {
				/* Skip over white space */
			
				s++;
				while (((*s == ' ') || (*s == '\t')) && (*s != '\0') &&
				        (*s != '\n')) s++;
		
				/* Skip over to the newline, and nix it */
			
				q = s;
				while ((*q != '\n') && (*q != '\0')) q++;
				*q = '\0';
				
				for (q = s; *q; q++)
					*q = toupper(*q);

				/* Iterate through the list of flags */
				
				q = strtok_r(s, " \t", &tokst);
				
				while (q != NULL) {
					/* Set the appropriate bit */
					
					fp = (FLAGENT *)hashfind(q, &mudstate.flags_htab); 
					if (fp != NULL) {
						if (fp->flagflag & FLAG_WORD3) {
							s_Flags3(object, Flags3(object) | fp->flagvalue);
						} else if (fp->flagflag & FLAG_WORD2) {
							s_Flags2(object, Flags2(object) | fp->flagvalue);
						} else {
							s_Flags(object, Flags(object) | fp->flagvalue);
						}
					}
                                	q = strtok_r(NULL, " \t", &tokst);
                                }
			}
			continue;
		}

		/* If we see 'powers', set powers on the object */
		
		if ((*buf == 'p') || (*buf == 'P')) {
			/* Cut through to the first space, tab, or NULL */
			while ((*s != ' ') && (*s != '\t') && (*s != '\0') &&
			       (*s != '\n')) s++;
			*s = '\0';
			
			if (!strcasecmp("powers", buf)) {
				/* Skip over white space */
			
				s++;
				while (((*s == ' ') || (*s == '\t')) && (*s != '\0') &&
				       (*s != '\n')) s++;
		
				/* Skip over to the newline, and nix it */
			
				q = s;
				while ((*q != '\n') && (*q != '\0')) q++;
				*q = '\0';
				
				for (q = s; *q; q++)
					*q = tolower(*q);

				/* Iterate through the list of powers */
				
				q = strtok_r(s, " \t", &tokst);
				
				while (q != NULL) {
					/* Set the appropriate bit */
					
					pp = (POWERENT *)hashfind(q, &mudstate.powers_htab); 
					if (pp != NULL) {
						if (pp->powerpower & POWER_EXT) {
							s_Powers2(object, Powers2(object) | pp->powervalue);
						} else {
							s_Powers(object, Powers(object) | pp->powervalue);
						}
					}
                                	q = strtok_r(NULL, " \t", &tokst);
                                }
			}
			continue;
		}

		/* If we see 'name', set name on the object */
		
		if ((*buf == 'n') || (*buf == 'N')) {
			/* Cut through to the first space, tab, or NULL */
			while ((*s != ' ') && (*s != '\t') && (*s != '\0') &&
			       (*s != '\n')) s++;
			*s = '\0';
			
			if (!strcasecmp("name", buf)) {
				/* Skip over white space */
			
				s++;
				while (((*s == ' ') || (*s == '\t')) && (*s != '\0') &&
				       (*s != '\n')) s++;
		
				/* Skip over to the newline, and nix it */
			
				q = s;
				while ((*q != '\n') && (*q != '\0')) q++;
				*q = '\0';

				/* Set the name on the object */
				
				s_Name(object, s);
			}
			continue;
		}

		/* If we see a '-', end the attribute */
		
		if (*buf == '-') {
			*p = '\0';
			
			if (*attrnam == '@') {
				/* Execute a command */
				(void)process_command(object, player, 1,
					attrtxt, (char **)NULL, 0);
				*attrnam = '\0';
				p = attrtxt;
			} else {
				/* Create the attribute name and set it */
			
				anum = mkattr(attrnam + 1);
				*attrnam = '\0';
	
				/* Check for a good attribute number */
	
				if (anum > 0)
					atr_add(object, anum, attrtxt, player, 0);
				p = attrtxt;
			}

			continue;
		}
		
		/* If we see a '&' or '@', it's the beginning of an
		   attribute or command */
		
		if ((*buf == '&') || (*buf == '@')) {
			p = attrtxt;
			
			/* Cut through to the first space, tab, \n, or NULL */

			while ((*s != ' ') && (*s != '\t') && (*s != '\0')
			       && (*s != '\n')) s++;
			*s = '\0';
			strncpy(attrnam, buf, SBUF_SIZE);

			/* Skip over white space */
			
			s++;
			while (((*s == ' ') || (*s == '\t')) && (*s != '\0') &&
			       (*s != '\n')) s++;

			/* Skip over to the newline, and nix it */
			
			q = s;
			while ((*q != '\n') && (*q != '\0')) q++;
			*q = '\0';

			/* Copy the rest of the line to attribute text */
			
			if (*attrnam == '@') {
				safe_str(attrnam, attrtxt, &p);
				safe_chr(' ', attrtxt, &p);
			}

			if (*s) {
				safe_str(s, attrtxt, &p);
			}

			continue;
		}
			
		/* If we are in the middle of setting an attribute, copy
		 * any strings to attribute text */
		
		if (*attrnam) {
			/* Skip over to the newline, and nix it */
			
			q = s;
			while ((*q != '\n') && (*q != '\0')) q++;
			*q = '\0';

			/* Strip leading white space */
		
			while (((*s == ' ') || (*s == '\t')) && (*s != '\0') &&
			        (*s != '\n')) s++;
		
			/* Copy the rest of the line to attribute text */
		
			if (*s)
				safe_str(s, attrtxt, &p);
		}
		
		/* The line didn't match anything, discard it */
	}

	if (player != NOTHING)
		notify(player, "Code library uploaded.");
	fclose(f);
	
	free_mbuf(nam);
	free_lbuf(attrtxt);
	free_sbuf(attrnam);
	free_mbuf(buf);
}

void mod_codelib_cleanup_startup()
{
	char *p, *s, *dbrefstr;
	dbref thing;
	FILE *f;
	int i;
	
	/* Go through the hash table read from the config file and load 
	 * up the code */
	
	s = alloc_mbuf("mod_codelib_cleanup_startup");

	for (p = hash_firstkey(&mod_codelib_config.libraries); p;
	     p = hash_nextkey(&mod_codelib_config.libraries)) {
		dbrefstr = (char *)hashfind(p, &mod_codelib_config.libraries);

		if (dbrefstr)
			thing = atoi(dbrefstr);
		else
			thing = -1;

			
		if (!Good_obj(thing) || isGarbage(thing)) {
			STARTLOG(LOG_ALWAYS, "MOD", "CODELIB")
				log_printf("#%d is not a valid object", thing);
			ENDLOG
			continue;
		}

		
		/* Upload the softcode */
						
		mod_codelib_upload_file(p, thing, NOTHING);
	}
	free_mbuf(s);
}

DO_CMD_TWO_ARG(mod_codelib_do_codelib)
{
	dbref object;
	char *s;

	s = alloc_mbuf("mod_codelib_do_codelib");
	
	if (arg1 && *arg1) {
		snprintf(s, MBUF_SIZE, "Codelib: %s", arg1);
	} else {
		notify(player, "You must specify a library name.");
		return;
	}
		
	if (key & MOD_CODELIB_CREATE) {
		object = create_obj(player, TYPE_THING, s, 0);
		if (object == NOTHING)
			return;
			
		move_via_generic(object, player, NOTHING, 0);
		s_Home(object, new_home(player));
		notify(player, tprintf("Object created as #%d.", object));
	} else {
		if (!arg2 || !*arg2) {
			notify(player, "You must specify an object.");
		}

		/* Look for the object */
        
		init_match(player, arg2, NOTYPE);
		match_everything(MAT_EXIT_PARENTS);
		object = match_result();

		if (!Good_obj(object)) {
			notify(player, "No such object.");
			return;
		}
	}
	
	/* Upload the softcode */
						
	mod_codelib_upload_file(arg1, object, player);
}

NAMETAB mod_codelib_codelib_sw[] = {
{(char *)"create",	2,	CA_WIZARD,	MOD_CODELIB_CREATE},
{ NULL,                 0,              0,              0}};

CMDENT mod_codelib_cmdtable[] = {
{(char *)"@codelib",	mod_codelib_codelib_sw,	CA_WIZARD,
	0,		CS_TWO_ARG,
	NULL,		NULL,	NULL,	mod_codelib_do_codelib},
{(char *)NULL,		NULL,		0,
	0,		0,
	NULL,		NULL,	NULL,	{NULL}}};
                

void mod_codelib_init()
{
	mod_codelib_config.pathname = XSTRDUP(".", "cf_string");
	register_hashtables(mod_codelib_hashtable, NULL);
	register_commands(mod_codelib_cmdtable);
}
