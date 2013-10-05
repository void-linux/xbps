/*	$NetBSD: readpassphrase.c,v 1.1 2009/06/07 22:38:47 christos Exp $	*/
/*
 * Copyright (c) 2000 Todd C. Miller <Todd.Miller@courtesan.com>
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <pwd.h>
#include <signal.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "defs.h"

#define RPP_ECHO_OFF    0x00            /* Turn off echo (default). */
#define RPP_ECHO_ON     0x01            /* Leave echo on. */
#define RPP_REQUIRE_TTY 0x02            /* Fail if there is no tty. */
#define RPP_FORCELOWER  0x04            /* Force input to lower case. */
#define RPP_FORCEUPPER  0x08            /* Force input to upper case. */
#define RPP_SEVENBIT    0x10            /* Strip the high bit from input. */

char *
readpassphrase(const char *prompt, char *buf, size_t bufsiz, int flags)
{
	struct termios term, oterm;
	char ch, *p, *end;
	int input, output;
	sigset_t oset, nset;

	/* I suppose we could alloc on demand in this case (XXX). */
	if (bufsiz == 0) {
		errno = EINVAL;
		return(NULL);
	}

	/*
	 * Read and write to /dev/tty if available.  If not, read from
	 * stdin and write to stderr unless a tty is required.
	 */
	if ((input = output = open(_PATH_TTY, O_RDWR)) == -1) {
		if (flags & RPP_REQUIRE_TTY) {
			errno = ENOTTY;
			return(NULL);
		}
		input = STDIN_FILENO;
		output = STDERR_FILENO;
	}

	/*
	 * We block SIGINT and SIGTSTP so the terminal is not left
	 * in an inconsistent state (ie: no echo).  It would probably
	 * be better to simply catch these though.
	 */
	sigemptyset(&nset);
	sigaddset(&nset, SIGINT);
	sigaddset(&nset, SIGTSTP);
	(void)sigprocmask(SIG_BLOCK, &nset, &oset);

	/* Turn off echo if possible. */
	if (tcgetattr(input, &oterm) == 0) {
		memcpy(&term, &oterm, sizeof(term));
		if (!(flags & RPP_ECHO_ON) && (term.c_lflag & ECHO))
			term.c_lflag &= ~ECHO;
		(void)tcsetattr(input, TCSAFLUSH, &term);
	} else {
		memset(&term, 0, sizeof(term));
		memset(&oterm, 0, sizeof(oterm));
	}

	(void)write(output, prompt, strlen(prompt));
	end = buf + bufsiz - 1;
	for (p = buf; read(input, &ch, 1) == 1 && ch != '\n' && ch != '\r';) {
		if (p < end) {
			if ((flags & RPP_SEVENBIT))
				ch &= 0x7f;
			if (isalpha((unsigned char)ch)) {
				if ((flags & RPP_FORCELOWER))
					ch = tolower((unsigned char)ch);
				if ((flags & RPP_FORCEUPPER))
					ch = toupper((unsigned char)ch);
			}
			*p++ = ch;
		}
	}
	*p = '\0';
	if (!(term.c_lflag & ECHO))
		(void)write(output, "\n", 1);

	/* Restore old terminal settings and signal mask. */
	if (memcmp(&term, &oterm, sizeof(term)) != 0)
		(void)tcsetattr(input, TCSAFLUSH, &oterm);
	(void)sigprocmask(SIG_SETMASK, &oset, NULL);
	if (input != STDIN_FILENO)
		(void)close(input);

	return(buf);
}
