/* compile.c - demonstration module */

/* $Id$ */

#include "../../api.h"

extern char qidx_chartab[256];  /* from funvars.c */
extern char token_chartab[256]; /* from eval.c */
extern char *ansi_chartab[256]; /* from eval.c */

unsigned int dbtype;

char mod_compile_compile_special_chartab[256] =
{
    1,1,1,1,1,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,1,0,0,0,0,
    0,0,0,0,0,0,0,0, 1,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,1,0,0,0,0,
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,1,0,0,0,0,
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,1
};

char mod_compile_exec_special_chartab[256] =
{
    1,1,1,1,1,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,1,0,0,0,0,
    1,0,0,1,0,1,0,0, 1,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,1,1,0,0,0,
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,1,0,0,0,0,
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,1
};

typedef struct tcache_ent TCENT;
struct tcache_ent {
        char *orig;
        char *result;
        struct tcache_ent *next;
} *tcache_head;
int tcache_top, tcache_count;

static void tcache_add(orig, result)
char *orig, *result;
{
	char *tp;
	TCENT *xp;

	if (strcmp(orig, result)) {
		tcache_count++;
		if (tcache_count <= mudconf.trace_limit) {
			xp = (TCENT *) alloc_sbuf("tcache_add.sbuf");
			tp = alloc_lbuf("tcache_add.lbuf");
			strcpy(tp, result);
			xp->orig = orig;
			xp->result = tp;
			xp->next = tcache_head;
			tcache_head = xp;
		} else {
			free_lbuf(orig);
		}
	} else {
		free_lbuf(orig);
	}
}

static void tcache_finish(player)
dbref player;
{
	TCENT *xp;
	NUMBERTAB *np;
	dbref target;

	if (H_Redirect(player)) {
	    np = (NUMBERTAB *) nhashfind(player, &mudstate.redir_htab);
	    if (np) {
		target = np->num; 
	    } else {
		/* Ick. If we have no pointer, we should have no flag. */
		s_Flags3(player, Flags3(player) & ~HAS_REDIRECT);
		target = Owner(player);
	    }
	} else {
	    target = Owner(player);
	}

	while (tcache_head != NULL) {
		xp = tcache_head;
		tcache_head = xp->next;
		notify(target,
		       tprintf("%s(#%d)} '%s' -> '%s'", Name(player), player,
			       xp->orig, xp->result));
		free_lbuf(xp->orig);
		free_lbuf(xp->result);
		free_sbuf(xp);
	}
	tcache_top = 1;
	tcache_count = 0;
}

#define MOD_COMPILE_BYTECODE	0xFF
#define MOD_COMPILE_END		0x01
#define MOD_COMPILE_FUN		0x02
#define MOD_COMPILE_UFUN	0x03
#define MOD_COMPILE_BRACES	0x04

/* ---------------------------------------------------------------------------
 * mod_compile_compile_arglist: Parse a line into an argument list.
 */

#define	NFARGS	30

void mod_compile_compile_arglist(buff, bufc, dstr, nfargs)
char *dstr, *buff, **bufc;
int nfargs;
{
	char *rstr, *tstr, *bp, *str;
	int arg, i, len;
	char *fargs[NFARGS - 1];
	int arglen[NFARGS - 1];
	
	for (arg = 0; arg < nfargs; arg++) {
		fargs[arg] = NULL;
		arglen[arg] = 0;
	}
	
	if (dstr == NULL)
		return;
	rstr = dstr;
	arg = 0;

	while ((arg < nfargs) && rstr) {
		if (arg < (nfargs - 1))
			tstr = parse_to(&rstr, ',', 0);
		else
			tstr = parse_to(&rstr, '\0', 0);

		bp = fargs[arg] = alloc_lbuf("parse_arglist");
		str = tstr;
		mod_compile_compile(fargs[arg], &bp, &str);
		*bp = '\0';
		arglen[arg] = bp - fargs[arg];
		arg++;
	}

	/* Copy the number of arguments */
	
	safe_copy_thing(&arg, buff, bufc, sizeof(int));

	/* Copy the size of the argument, followed by the argument */
	
	for(i = 0; i < arg; i++) {
		safe_copy_thing(&arglen[i], buff, bufc, sizeof(int));
		safe_copy_thing(fargs[i], buff, bufc, arglen[i]);
		safe_chr(MOD_COMPILE_END, buff, bufc);
		free_lbuf(fargs[i]);
	}
}

char *mod_compile_parse_arglist(player, caller, cause, dstr, eval,
		    fargs, nfargs, cargs, ncargs)
dbref player, caller, cause, eval, nfargs, ncargs;
char *dstr, *fargs[], *cargs[];
{
	char *rstr, *tstr, *bp, *str;
	int arg, peval, i, len;

	for (arg = 0; arg < nfargs; arg++) {
		fargs[arg] = NULL;
	}

	if (dstr == NULL)
		return NULL;
		
	rstr = dstr;

	/* Get the number of arguments */
	
	memcpy(&arg, dstr, sizeof(int));
	dstr += sizeof(int);
	
	peval = (eval & ~EV_EVAL);

	for (i = 0; i < arg; i++) {
		bp = fargs[i] = alloc_lbuf("parse_arglist");
		
		/* Get the length of the argument */
		
		memcpy(&len, dstr, sizeof(int));
		dstr += sizeof(int);
		str = tstr = dstr;
		*(dstr + len) = '\0';
		mod_compile_exec(fargs[i], &bp, player, caller, cause,
		     eval | EV_FCHECK, &str, cargs, ncargs);
		dstr += len + 1;
	}
	return dstr;
}

void mod_compile_exec(buff, bufc, player, caller, cause, eval, dstr, cargs, ncargs)
char *buff, **bufc;
dbref player, caller, cause;
int eval, ncargs;
unsigned char **dstr;
char *cargs[];
{
	char *real_fargs[NFARGS + 1], *preserve[MAX_GLOBAL_REGS];
	char **fargs = real_fargs + 1;
	char *tstr, *tbuf, *savepos, *atr_gotten, *start, *oldp;
	char savec, ch, *savestr, *str, *xptr, *mundane, *p;
	char *realbuff = NULL, *realbp = NULL;
	char xtbuf[SBUF_SIZE], *xtp;
	dbref aowner;
	int at_space, nfargs, gender, i, j, alldone, aflags, alen, feval;
	int is_trace, is_top, save_count, preserve_len[MAX_GLOBAL_REGS];
	int ansi, nchar, navail, len;
	FUN *fp;
	UFUN *ufp;
	VARENT *xvar;
	ATTR *ap;

	static const char *subj[5] =
	{"", "it", "she", "he", "they"};
	static const char *poss[5] =
	{"", "its", "her", "his", "their"};
	static const char *obj[5] =
	{"", "it", "her", "him", "them"};
	static const char *absp[5] =
	{"", "its", "hers", "his", "theirs"};


	fp = NULL;
	ufp = NULL;
	
	if (*dstr == NULL)
		return;

	at_space = 1;
	gender = -1;
	alldone = 0;
	ansi = 0;

	is_trace = Trace(player) && !(eval & EV_NOTRACE); is_top = 0;

	/* Extend the buffer if we need to. */
	
	if (((*bufc) - buff) > (LBUF_SIZE - SBUF_SIZE)) {
		realbuff = buff;
		realbp = *bufc;
		buff = (char *)XMALLOC(LBUF_SIZE, "exec.buff_extend");
		*bufc = buff;
	}

	oldp = start = *bufc;
	
	/* If we are tracing, save a copy of the starting buffer */

	savestr = NULL;
	if (is_trace) {
		is_top = tcache_empty();
		savestr = alloc_lbuf("exec.save");
		strcpy(savestr, *dstr);
	}
	while (**dstr && !alldone) {

	    /* We adjust the special table every time we go around this
	     * loop, in order to avoid always treating '#' like a special
	     * character, as it gets used a whole heck of a lot.
	     */
	    mod_compile_exec_special_chartab[(unsigned char) '#'] =
		(mudstate.in_loop || mudstate.in_switch) ? 1 : 0;

	    if (!mod_compile_exec_special_chartab[(unsigned char) **dstr]) {
		/* Mundane characters are the most common. There are usually
		 * a bunch in a row. We should just copy them.
		 */
		mundane = *dstr;
		nchar = 0;
		do {
		    nchar++;
		} while (!mod_compile_exec_special_chartab[(unsigned char) *(++mundane)]);
		p = *bufc;
		navail = LBUF_SIZE - 1 - (p - buff);
		nchar = (nchar > navail) ? navail : nchar;
		memcpy(p, *dstr, nchar);
		*bufc = p + nchar;
		*dstr = mundane;
		at_space = 0;
	    }

	    /* We must have a special character at this point. */

	    if (**dstr == '\0')
		break;

	    switch (**dstr) {
		case ' ':
			/* A space.  Add a space if not compressing or if
			 * previous char was not a space 
			 */

			if (!(mudconf.space_compress && at_space) ||
			    (eval & EV_NO_COMPRESS)) {
				safe_chr(' ', buff, bufc);
				at_space = 1;
			}
			break;
		case '\\':
			/* General escape.  Add the following char without
			 * special processing 
			 */

			at_space = 0;
			(*dstr)++;
			if (**dstr) {
				safe_chr(**dstr, buff, bufc);
			} else
				(*dstr)--;
			break;
		case MOD_COMPILE_BYTECODE:
			/* A 'byte-compiled' string. Usually saves us a couple
			 * of parse_to's and hashtable lookup */

			at_space = 0; 
			(*dstr)++;
			
			if (**dstr == MOD_COMPILE_BRACES) {
				(*dstr)++;
				memcpy(&len, *dstr, sizeof(int));
				*dstr += sizeof(int);
				tbuf = *dstr;
				*dstr += len;
				**dstr = '\0';
								
				if (!(eval & EV_STRIP)) {
					safe_chr('{', buff, bufc);
				}
				/* Preserve leading spaces (Felan) */

				if (*tbuf == ' ') {
					safe_chr(' ', buff, bufc);
					tbuf++;
				}
				str = tbuf;
				exec(buff, bufc, player, caller, cause,
				     (eval & ~(EV_STRIP | EV_FCHECK)),
				     &str, cargs, ncargs);
				if (!(eval & EV_STRIP)) {
					safe_chr('}', buff, bufc);
				}
				break;
			} else if (**dstr == MOD_COMPILE_UFUN) {
				(*dstr)++;
				memcpy(&ufp, *dstr, sizeof(UFUN *));
				*dstr += sizeof(UFUN *);
			} else if (**dstr == MOD_COMPILE_FUN) {
				(*dstr)++;
				memcpy(&fp, *dstr, sizeof(FUN *));
				*dstr += sizeof(FUN *);
			}
			
			tbuf = tstr = *dstr;
			
			if (ufp)
				nfargs = NFARGS;
			else if (fp->nargs < 0)
				nfargs = -fp->nargs;
			else
				nfargs = NFARGS;
			tstr = *dstr;
			if ((fp && (fp->flags & FN_NO_EVAL)) ||
			    (ufp && (ufp->flags & FN_NO_EVAL)))
				feval = (eval & ~EV_EVAL) | EV_STRIP_ESC;
			else
				feval = eval;
				
			*dstr = mod_compile_parse_arglist(player, caller, cause, tbuf,
					      feval, fargs, nfargs,
					      cargs, ncargs);

			
			/* Count number of args returned */

			j = 0;
			for (i = 0; i < nfargs; i++)
				if (fargs[i] != NULL)
					j = i + 1;
			nfargs = j;

			/* If it's a user-defined function, perform it now. */

			if (ufp) {
				mudstate.func_nest_lev++;
				mudstate.func_invk_ctr++;
				if (mudstate.func_nest_lev >=
				    mudconf.func_nest_lim) {
					safe_str("#-1 FUNCTION RECURSION LIMIT EXCEEDED", buff, bufc);
				} else if (mudstate.func_invk_ctr >=
					   mudconf.func_invk_lim) {
					safe_str("#-1 FUNCTION INVOCATION LIMIT EXCEEDED", buff, bufc);
				} else if ((mudconf.func_cpu_lim > 0) &&
					   (clock() - mudstate.cputime_base >
					    mudconf.func_cpu_lim)) {
				        safe_str("#-1 FUNCTION CPU LIMIT EXCEEDED", buff, bufc);
				} else if (Going(player)) {
				        safe_str("#-1 BAD INVOKER", buff, bufc);
				} else if (!check_access(player, ufp->perms)) {
					safe_noperm(buff, bufc);
				} else {
					tstr = atr_get(ufp->obj, ufp->atr,
						       &aowner, &aflags,
						       &alen);
					if (ufp->flags & FN_PRIV)
						i = ufp->obj;
					else
						i = player;
					str = tstr;
					
					if (ufp->flags & FN_PRES) {
					    save_global_regs("eval_save",
							     preserve,
							     preserve_len);
					}
					
					exec(buff, bufc, i, player, cause,
					     ((ufp->flags & FN_NO_EVAL) ?
					      (EV_FCHECK | EV_EVAL) : feval),
					     &str, fargs, nfargs);
					
					if (ufp->flags & FN_PRES) {
					    restore_global_regs("eval_restore",
								preserve,
								preserve_len);
					}

					free_lbuf(tstr);
				}

				/* Return the space allocated for the args */

				mudstate.func_nest_lev--;
				for (i = 0; i < nfargs; i++)
					if (fargs[i] != NULL)
						free_lbuf(fargs[i]);
				eval &= ~EV_FCHECK;
				break;
			}
			/* If the number of args is right, perform the func.
			 * Otherwise return an error message.  Note
			 * that parse_arglist returns zero args as one
			 * null arg, so we have to handle that case
			 * specially. 
			 */

			if ((fp->nargs == 0) && (nfargs == 1)) {
				if (!*fargs[0]) {
					free_lbuf(fargs[0]);
					fargs[0] = NULL;
					nfargs = 0;
				}
			}
			if ((nfargs == fp->nargs) ||
			    (nfargs == -fp->nargs) ||
			    (fp->flags & FN_VARARGS)) {

				/* Check recursion limit */

				mudstate.func_nest_lev++;
				mudstate.func_invk_ctr++;
				if (mudstate.func_nest_lev >=
				    mudconf.func_nest_lim) {
					safe_str("#-1 FUNCTION RECURSION LIMIT EXCEEDED", buff, bufc);
				} else if (mudstate.func_invk_ctr >=
					   mudconf.func_invk_lim) {
					safe_str("#-1 FUNCTION INVOCATION LIMIT EXCEEDED", buff, bufc);
				} else if ((mudconf.func_cpu_lim > 0) &&
					   (clock() - mudstate.cputime_base >
					    mudconf.func_cpu_lim)) {
					safe_str("#-1 FUNCTION CPU LIMIT EXCEEDED", buff, bufc);
				} else if (Going(player)) {
					/* Deal with the peculiar case of the
					 * calling object being destroyed
					 * mid-function sequence, such as
					 * with a command()/@destroy combo...
					 */
					safe_str("#-1 BAD INVOKER", buff, bufc);
				} else if (!Check_Func_Access(player, fp)) {
					safe_noperm(buff, bufc);
				} else {
					fargs[-1] = (char *)fp;
					fp->fun( FUNCTION_ARGLIST );
				}
				mudstate.func_nest_lev--;
			} else {
			  safe_tprintf_str(buff, bufc,
					   "#-1 FUNCTION (%s) EXPECTS %d ARGUMENTS",
					   fp->name, fp->nargs);
			}

			/* Return the space allocated for the arguments */

			for (i = 0; i < nfargs; i++)
				if (fargs[i] != NULL)
					free_lbuf(fargs[i]);
			eval &= ~EV_FCHECK;
			break;
		case '%':
			/* Percent-replace start.  Evaluate the chars
			 * following and perform the appropriate
			 * substitution. 
			 */

			at_space = 0;
			(*dstr)++;
			savec = **dstr;
			savepos = *bufc;
			switch (savec) {
			case '\0':	/* Null - all done */
				(*dstr)--;
				break;
			case '|':	/* piped command output */
				safe_str(mudstate.pout, buff, bufc);
				break;
			case '%':	/* Percent - a literal % */
				safe_chr('%', buff, bufc);
				break;
			case 'x':	/* ANSI color */
			case 'X':
				(*dstr)++;
				if (!**dstr) {
				    /*
				     * Note: There is an interesting
				     * bug/misfeature in the implementation
				     * of %v? and %q? -- if the second
				     * character is garbage or non-existent,
				     * it and the leading v or q gets eaten.
				     * In the interests of not changing the
				     * old behavior, this is not getting
				     * "fixed", but in this case, where
				     * moving the pointer back without
				     * exiting on an error condition ends up
				     * turning things black, the behavior
				     * must by necessity be different. So we
				     * do  break out of the switch.
				     */
				    (*dstr)--;
				    break;
				}
				if (!mudconf.ansi_colors) {
				    /* just skip over the characters */
				    break;
				}
				if (!ansi_chartab[(unsigned char) **dstr]) {
				    safe_chr(**dstr, buff, bufc);
				} else {
				    safe_str(ansi_chartab[(unsigned char) **dstr], buff, bufc);
				    ansi = (**dstr == 'n') ? 0 : 1;
				}
				break;
			case 'r':	/* Carriage return */
			case 'R':
				safe_crlf(buff, bufc);
				break;
			case 't':	/* Tab */
			case 'T':
				safe_chr('\t', buff, bufc);
				break;
			case 'B':	/* Blank */
			case 'b':
				safe_chr(' ', buff, bufc);
				break;
			case '0':	/* Command argument number N */
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				i = (**dstr - '0');
				if ((i < ncargs) && (cargs[i] != NULL))
					safe_str(cargs[i], buff, bufc);
				break;
			case '=': /* equivalent of generic v() attr get */
			        (*dstr)++;
				if (**dstr != '<') {
				    (*dstr)--;
				    break;
				}
				xptr = *dstr;
				(*dstr)++;
				if (!**dstr) {
				    *dstr = xptr;
				    break;
				}
				xtp = xtbuf;
				while (**dstr && (**dstr != '>')) {
				    safe_sb_chr(**dstr, xtbuf, &xtp);
				    (*dstr)++;
				}
				if (**dstr != '>') {
				    /* Ran off the end. Back up. */
				    *dstr = xptr;
				    break;
				}
				*xtp = '\0';
				ap = atr_str(xtbuf);
				if (!ap)
				    break;
				atr_pget_info(player, ap->number, 
					      &aowner, &aflags);
				if (See_attr(player, player, ap,
					     aowner, aflags)) {
				    atr_gotten = atr_pget(player, ap->number,
							  &aowner, &aflags,
							  &alen);
				    safe_known_str(atr_gotten, alen,
						   buff, bufc);
				    free_lbuf(atr_gotten);
				}
				break;
			case '_':       /* x-variable */
			        (*dstr)++;
				/* Check for %_<varname> */
				if (**dstr != '<') {
				    ch = tolower(**dstr);
				    if (!**dstr)
					(*dstr)--;
				    if (!isalnum(ch))
					break;
				    xtp = xtbuf;
				    safe_ltos(xtbuf, &xtp, player);
				    safe_chr('.', xtbuf, &xtp);
				    safe_chr(ch, xtbuf, &xtp);
				} else {
				    xptr = *dstr;
				    (*dstr)++;
				    if (!**dstr) {
					*dstr = xptr;
					break;
				    }
				    xtp = xtbuf;
				    safe_ltos(xtbuf, &xtp, player);
				    safe_chr('.', xtbuf, &xtp);
				    while (**dstr && (**dstr != '>')) {
					/* Copy. No interpretation. */
					ch = tolower(**dstr);
					safe_sb_chr(ch, xtbuf, &xtp);
					(*dstr)++;
				    }
				    if (**dstr != '>') {
					/* We ran off the end of the string
					 * without finding a termination
					 * condition. Go back.
					 */
					*dstr = xptr;
					break;
				    }
				}
				*xtp = '\0';
				if ((xvar = (VARENT *) hashfind(xtbuf,
						       &mudstate.vars_htab))) {
				    safe_str(xvar->text, buff, bufc);
				}
				break;
			case 'V':	/* Variable attribute */
			case 'v':
				(*dstr)++;
				ch = toupper(**dstr);
				if (!**dstr)
					(*dstr)--;
				if ((ch < 'A') || (ch > 'Z'))
					break;
				i = 100 + ch - 'A';
				atr_gotten = atr_pget(player, i, &aowner,
						      &aflags, &alen);
				safe_known_str(atr_gotten, alen, buff, bufc);
				free_lbuf(atr_gotten);
				break;
			case 'Q':	/* Local registers */
			case 'q':
				(*dstr)++;
				if (!**dstr) {
					(*dstr)--;
					break;
				}
				i = qidx_chartab[(unsigned char) **dstr];
				if ((i < 0) || (i >= MAX_GLOBAL_REGS))
					break;
				if (mudstate.global_regs[i]) {
					safe_known_str(mudstate.global_regs[i],
						      mudstate.glob_reg_len[i],
						       buff, bufc);
				}
				if (!**dstr)
					(*dstr)--;
				break;
			case 'O':	/* Objective pronoun */
			case 'o':
				if (gender < 0)
					gender = get_gender(cause);
				if (!gender)
					safe_name(cause, buff, bufc);
				else
					safe_str((char *)obj[gender],
						 buff, bufc);
				break;
			case 'P':	/* Personal pronoun */
			case 'p':
				if (gender < 0)
					gender = get_gender(cause);
				if (!gender) {
				        safe_name(cause, buff, bufc);
					safe_chr('s', buff, bufc);
				} else {
					safe_str((char *)poss[gender],
						 buff, bufc);
				}
				break;
			case 'S':	/* Subjective pronoun */
			case 's':
				if (gender < 0)
					gender = get_gender(cause);
				if (!gender)
					safe_name(cause, buff, bufc);
				else
					safe_str((char *)subj[gender],
						 buff, bufc);
				break;
			case 'A':	/* Absolute posessive */
			case 'a':	/* idea from Empedocles */
				if (gender < 0)
					gender = get_gender(cause);
				if (!gender) {
				        safe_name(cause, buff, bufc);
					safe_chr('s', buff, bufc);
				} else {
					safe_str((char *)absp[gender],
						 buff, bufc);
				}
				break;
			case '#':	/* Invoker DB number */
				safe_dbref(buff, bufc, cause);
				break;
			case '!':	/* Executor DB number */
				safe_dbref(buff, bufc, player);
				break;
			case 'N':	/* Invoker name */
			case 'n':
				safe_name(cause, buff, bufc);
				break;
			case 'L':	/* Invoker location db# */
			case 'l':
				if (!(eval & EV_NO_LOCATION)) {
					safe_dbref(buff, bufc, where_is(cause));
				}
				break;
			case '@':	/* Caller dbref */
				safe_dbref(buff, bufc, caller);
				break;
			case 'C':
			case 'c':
			case 'M':
			case 'm':
				safe_str(mudstate.curr_cmd, buff, bufc);
				break;
			default:	/* Just copy */
				safe_chr(**dstr, buff, bufc);
			}
			if (isupper(savec))
				*savepos = toupper(*savepos);
			break;
		case '#':
		        /* We should never reach this point unless we're
			 * in a loop or switch, thanks to the table lookup.
			 */

		        at_space = 0;
			(*dstr)++;
			if (!token_chartab[(unsigned char) **dstr]) {
			    (*dstr)--;
			    safe_chr(**dstr, buff, bufc);
			} else {
			    if ((**dstr == '#') && mudstate.in_loop) {
			      safe_str(mudstate.loop_token[mudstate.in_loop-1],
				       buff, bufc);
			    } else if ((**dstr == '@') && mudstate.in_loop) {
			      safe_ltos(buff, bufc,
				     mudstate.loop_number[mudstate.in_loop-1]);
			    } else if ((**dstr == '$') && mudstate.in_switch) {
				safe_str(mudstate.switch_token, buff, bufc);
			    } else if (**dstr == '!') {
				/* Nesting level of loop takes precedence
				 * over switch nesting level.
				 */
				safe_ltos(buff, bufc,
					  ((mudstate.in_loop) ?
					   (mudstate.in_loop - 1):
					   mudstate.in_switch));
			    } else {
				(*dstr)--;
				safe_chr(**dstr, buff, bufc);
			    }
			}
			break;
		case ESC_CHAR:
			safe_copy_esccode(*dstr, buff, bufc);
			(*dstr)--;
			break;
	    }
	    (*dstr)++;
	}

	/* If we're eating spaces, and the last thing was a space, eat it
	 * up. Complicated by the fact that at_space is initially
	 * true. So check to see if we actually put something in the
	 * buffer, too. 
	 */

	if (mudconf.space_compress && at_space && !(eval & EV_NO_COMPRESS)
	    && (start != *bufc))
		(*bufc)--;

	/* The ansi() function knows how to take care of itself. However, 
	 * if the player used a %x sub in the string, and hasn't yet
	 * terminated the color with a %xn yet, we'll have to do it for 
	 * them. 
	 */

	if (ansi)
		safe_ansi_normal(buff, bufc);

	**bufc = '\0';

	if (realbuff) {
		*bufc = realbp;
		safe_str(buff, realbuff, bufc);
		**bufc = '\0';
		XFREE(buff, "exec.buff_extend");
		buff = realbuff;
	}
	
	/* Report trace information */

	if (is_trace) {
		tcache_add(savestr, start);
		save_count = tcache_count - mudconf.trace_limit;;
		if (is_top || !mudconf.trace_topdown)
			tcache_finish(player);
		if (is_top && (save_count > 0)) {
			tbuf = alloc_mbuf("exec.trace_diag");
			sprintf(tbuf,
				"%d lines of trace output discarded.",
				save_count);
			notify(player, tbuf);
			free_mbuf(tbuf);
		}
	}
}

INLINE void safe_copy_thing(src, buff, bufp, size)
char *src;
int size;
char *buff, **bufp;
{
    int n;
    char *tp, *maxtp, *srcp;

    tp = *bufp;

    srcp = (char *)src;
    
    maxtp = buff + LBUF_SIZE - 1;

    if (tp + size < maxtp)
	maxtp = tp + size;
    while (tp < maxtp)
	*(tp)++ = *srcp++;

    *bufp = tp;
}

void mod_compile_compile(buff, bufc, dstr)
char *buff, **bufc;
char **dstr;
{
	char *real_fargs[NFARGS + 1], *preserve[MAX_GLOBAL_REGS];
	char **fargs = real_fargs + 1;
	char *tstr, *tbuf, *savepos, *atr_gotten, *start, *oldp;
	char savec, ch, *savestr, *str, *xptr, *mundane, *p;
	char *realbuff = NULL, *realbp = NULL;
	char xtbuf[SBUF_SIZE], *xtp;
	dbref aowner;
	int at_space, nfargs, gender, i, j, alldone, aflags, alen, feval;
	int is_trace, is_top, save_count, preserve_len[MAX_GLOBAL_REGS];
	int ansi, nchar, navail;
	int len;
	FUN *fp;
	UFUN *ufp;
	VARENT *xvar;
	ATTR *ap;

	fp = NULL;
	ufp = NULL;

	if (*dstr == NULL)
		return;

	alldone = 0;

	/* Extend the buffer if we need to. */
	
	if (((*bufc) - buff) > (LBUF_SIZE - SBUF_SIZE)) {
		realbuff = buff;
		realbp = *bufc;
		buff = (char *)XMALLOC(LBUF_SIZE, "exec.buff_extend");
		*bufc = buff;
	}

	oldp = start = *bufc;
	
	while (**dstr && !alldone) {

	    /* We adjust the special table every time we go around this
	     * loop, in order to avoid always treating '#' like a special
	     * character, as it gets used a whole heck of a lot.
	     */
	    mod_compile_compile_special_chartab[(unsigned char) '#'] =
		(mudstate.in_loop || mudstate.in_switch) ? 1 : 0;

	    if (!mod_compile_compile_special_chartab[(unsigned char) **dstr]) {
		/* Mundane characters are the most common. There are usually
		 * a bunch in a row. We should just copy them.
		 */
		mundane = *dstr;
		nchar = 0;
		do {
		    nchar++;
		} while (!mod_compile_compile_special_chartab[(unsigned char) *(++mundane)]);
		p = *bufc;
		navail = LBUF_SIZE - 1 - (p - buff);
		nchar = (nchar > navail) ? navail : nchar;
		memcpy(p, *dstr, nchar);
		*bufc = p + nchar;
		*dstr = mundane;
	    }

	    /* We must have a special character at this point. */

	    if (**dstr == '\0')
		break;

	    switch (**dstr) {
		case '[':
			/* Function start.  Evaluate the contents of the 
			 * square brackets as a function.  If no closing
			 * bracket, insert the [ and continue. 
			 */

			tstr = (*dstr)++;
			tbuf = parse_to(dstr, ']', 0);
			if (*dstr == NULL) {
				safe_chr('[', buff, bufc);
				*dstr = tstr;
			} else {
				str = tbuf;
				mod_compile_compile(buff, bufc, &str);
				(*dstr)--;
			}
			break;
		case '{':
			/* Literal start.  Insert everything up to the
			 * terminating } without parsing.  If no closing
			 * brace, insert the { and continue. 
			 */

			tstr = (*dstr)++;
			tbuf = parse_to(dstr, '}', 0);
			if (*dstr == NULL) {
				safe_chr('{', buff, bufc);
				*dstr = tstr;
			} else {
				/* insert string here */
				safe_chr(MOD_COMPILE_BYTECODE, buff, bufc);
				safe_chr(MOD_COMPILE_BRACES, buff, bufc);
				len = *dstr - tstr - 2;
				safe_copy_thing(&len, buff, bufc, sizeof(int));
				safe_str(tbuf, buff, bufc);
				safe_chr(MOD_COMPILE_END, buff, bufc);
				(*dstr)--;
			}
			break;
		case '(':
			/* Arglist start.  See if what precedes is a
			 * function. If so, execute it if we should. 
			 */

			/* Load an sbuf with an uppercase version of the func
			 * name, and see if the func exists.  Trim 
			 * trailing spaces from the name if configured. 
			 */

			**bufc = '\0';
			xtp = xtbuf;
			safe_sb_str(oldp, xtbuf, &xtp);
			*xtp = '\0';
			if (mudconf.space_compress) {
				while ((--xtp >= xtbuf) && isspace(*xtp)) ;
				xtp++;
				*xtp = '\0';
			}
			for (xtp = xtbuf; *xtp; xtp++)
				*xtp = toupper(*xtp);
			fp = (FUN *) hashfind(xtbuf, &mudstate.func_htab);

			/* If not a builtin func, check for global func */

			ufp = NULL;
			if (fp == NULL) {
				ufp = (UFUN *) hashfind(xtbuf,
							&mudstate.ufunc_htab);
			}
			/* Do the right thing if it doesn't exist */

			if (!fp && !ufp) {
				*bufc = oldp;
				safe_tprintf_str(buff, bufc, "#-1 FUNCTION (%s) NOT FOUND", xtbuf);
				alldone = 1;
				break;
			}

			/* If no closing delim, just insert the '(' and
			 * continue normally 
			 */

			tstr = (*dstr)++;
			tbuf = parse_to(dstr, ')', 0);
			if (*dstr == NULL) {
				safe_chr('(', buff, bufc);
				*dstr = tstr;
				break;
			}

			/* We've got function(args) now, so back up over
			 * function name in output buffer
			 */

			*bufc = oldp;
	
			safe_chr(MOD_COMPILE_BYTECODE, buff, bufc);
			
			if (fp) {
				safe_chr(MOD_COMPILE_FUN, buff, bufc);
				safe_copy_thing(&fp, buff, bufc, sizeof(FUN *));
			} else {
				safe_chr(MOD_COMPILE_UFUN, buff, bufc);
				safe_copy_thing(&ufp, buff, bufc, sizeof(UFUN *));
			}

                        if (ufp)
                                nfargs = NFARGS;
                        else if (fp->nargs < 0)
                                nfargs = -fp->nargs;
                        else
                                nfargs = NFARGS;

			mod_compile_compile_arglist(buff, bufc, tbuf, nfargs);
			safe_chr(MOD_COMPILE_END, buff, bufc);
			(*dstr)--;
	    }
	    (*dstr)++;
	}

	**bufc = '\0';

	if (realbuff) {
		*bufc = realbp;
		safe_str(buff, realbuff, bufc);
		**bufc = '\0';
		XFREE(buff, "exec.buff_extend");
		buff = realbuff;
	}
}


/* --------------------------------------------------------------------------
 * Functions.
 */

FUNCTION(mod_compile_do_ufun)
{
	dbref aowner, thing;
	int is_local, aflags, alen, anum, preserve_len[MAX_GLOBAL_REGS], len;
	ATTR *ap;
	char *atext, *preserve[MAX_GLOBAL_REGS], *str, *tbuf, *tbufc;
	DBData key, data;
	Aname okey;
	time_t start_time = 0;
	
	is_local = ((FUN *)fargs[-1])->flags & U_LOCAL;

	/* We need at least one argument */

	if (nfargs < 1) {
		safe_known_str("#-1 TOO FEW ARGUMENTS", 21, buff, bufc);
		return;
	}
	/* Two possibilities for the first arg: <obj>/<attr> and <attr>. */

	Parse_Uattr(player, fargs[0], thing, anum, ap);

	/* If we're evaluating locally, preserve the global registers. */

	if (is_local) {
		save_global_regs("fun_ulocal_save", preserve, preserve_len);
	}
	
	/* Try to find a compiled version of the attribute in cache */
	
	okey.object = thing;
	okey.attrnum = anum;
	key.dptr = &okey;
	key.dsize = sizeof(Aname);
	
	data = cache_get(key, dbtype);

	/* Check to see if the game has restarted since we compiled
	 * this attribute */

	if (data.dptr) {
		memcpy(data.dptr, &start_time, sizeof(time_t));
	}
	
	if (start_time && (start_time == mudstate.start_time)) {
		str = (char *)data.dptr + sizeof(time_t);
		mod_compile_exec(buff, bufc, thing, player, cause, EV_FCHECK | EV_EVAL,
		     &str, &(fargs[1]), nfargs - 1);
	} else {
		Get_Uattr(player, thing, ap, atext, aowner, aflags, alen);
		str = atext;
		tbufc = tbuf = alloc_lbuf("mod_compile_do_ufun");
		mod_compile_compile(tbuf, &tbufc, &str);
		data.dptr = (void *)XMALLOC(tbufc - tbuf + 1 + sizeof(time_t), "mod_compile_do_ufun");
		memcpy(data.dptr, &mudstate.start_time, sizeof(time_t)); 
		memcpy(data.dptr + sizeof(time_t), tbuf, tbufc - tbuf + 1);
		data.dsize = tbufc - tbuf + 1 + sizeof(time_t);
		cache_put(key, data, dbtype);
		str = tbuf;
		mod_compile_exec(buff, bufc, thing, player, cause, EV_FCHECK | EV_EVAL,
		     &str, &(fargs[1]), nfargs - 1);
		free_lbuf(atext);
		free_lbuf(tbuf);
	}
	
	/* If we're evaluating locally, restore the preserved registers. */

	if (is_local) {
		restore_global_regs("fun_ulocal_restore", preserve,
				    preserve_len);
	}
}

FUN mod_compile_functable[] = {
{"FASTU",           mod_compile_do_ufun,        0,  FN_VARARGS, CA_PUBLIC,      NULL},
{"FASTULOCAL",      mod_compile_do_ufun,        0,  FN_VARARGS|U_LOCAL,
                                                CA_PUBLIC,      NULL},
{NULL,		NULL,			0,	0,	0,		NULL}};

/* --------------------------------------------------------------------------
 * Initialization.
 */
	
void mod_compile_init()
{
    register_functions(mod_compile_functable);
}

void mod_compile_cleanup_startup()
{
    dbtype = register_dbtype("compile");
}

void mod_compile_cache_put_notify(key, type)
DBData key;
unsigned int type;
{
	if (type == DBTYPE_ATTRIBUTE) {
		cache_del(key, dbtype);
	}
}

void mod_compile_cache_del_notify(key, type)
DBData key;
unsigned int type;
{
	if (type == DBTYPE_ATTRIBUTE) {
		cache_del(key, dbtype);
	}
}
