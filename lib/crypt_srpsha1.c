/*
 *      Copyright (C) 2001 Nikos Mavroyanopoulos
 *
 * This file is part of GNUTLS.
 *
 * GNUTLS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * GNUTLS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#include "defines.h"
#include "gnutls_int.h"
#include "gnutls_random.h"
#include "gnutls_hash_int.h"
#include "cert_b64.h"
#include "gnutls_srp.h"
#include <gnutls_errors.h>

/*	x = SHA(<salt> | SHA(<username> | ":" | <raw password>)) */

static const char magic[] = "$0$";

char *
 crypt_srpsha1(const char* username, const char *passwd, const char *salt)
{
	unsigned char *sp, *tsp, *r1;
	int salt_size = strlen(salt);
	unsigned char *local_salt, *v;
	int passwd_len;
	GNUTLS_MAC_HANDLE h1;
	int vsize, hash_len = gnutls_hash_get_algo_len(GNUTLS_MAC_SHA);
	char *tmp, *g, *n;
	uint8 *rtext, * csalt;
			
	passwd_len = strlen(passwd);	/* we do not want the null */

	h1 = gnutls_hash_init(GNUTLS_MAC_SHA);
	gnutls_hash(h1, (char*)username, strlen(username));
	gnutls_hash(h1, ":", 1);
	gnutls_hash(h1, (char*)passwd, passwd_len);
	r1 = gnutls_hash_deinit(h1);

	local_salt = malloc(salt_size + 1);
	strcpy((char *) local_salt, salt);
	sp = local_salt;

	/* If it starts with the magic string, then skip that */
	if (!strncmp((char *) sp, magic, strlen(magic)))
		sp += strlen(magic);

	tsp = sp;
	while((*tsp)!='$') tsp++;
	*tsp = '\0';		/* put a null after the end of salt */

	_gnutls_base64_decode( sp, strlen(sp), &csalt);
	
	h1 = gnutls_hash_init(GNUTLS_MAC_SHA);
	gnutls_hash(h1, csalt, 16);
	gnutls_free(csalt);
	
	gnutls_hash(h1, r1, hash_len);
	
	gnutls_free(r1);
	r1 = gnutls_hash_deinit(h1);
	
	/* v = g^x mod n */
	vsize = _gnutls_srp_gx(r1, hash_len, &v, &g, &n);
	gnutls_free(r1);
	if (vsize==-1 || v==NULL) {
		gnutls_assert();
		return NULL;
	}

	_gnutls_base64_encode(v, vsize, &rtext);
	gnutls_free(v);

	tmp = gnutls_malloc( strlen(sp)+strlen(rtext)+strlen(magic)+strlen(g)+2+strlen(n)+1+1 );
	
	sprintf( tmp, "%s%s$%s$%s$%s", magic, sp, rtext,g ,n);
	gnutls_free(g);
	gnutls_free(n);

	gnutls_free(rtext);
	gnutls_free(local_salt);

	return tmp;
}
/* salt here is the salt size */
char *crypt_srpsha1_wrapper(const char* username, const char *pass_new, int salt)
{
       unsigned char *result;
       char *tcp;
       unsigned char* rand;
       char *e = NULL;
	   int result_size;
	   
	   if (salt > 50 || salt <= 0) return NULL; /* wow that's pretty long salt */
	   
       rand = _gnutls_get_random( salt, GNUTLS_WEAK_RANDOM);

       result_size = _gnutls_base64_encode( rand, salt, &result);
	   if (result_size < 0) {
	   	gnutls_assert();
	   	return NULL;
	   }
	   
	   tcp = gnutls_calloc( 1, result_size + strlen(magic)+1 +1 );
	   sprintf(tcp, "%s%s$", magic, result);

	   gnutls_free(result);
	   _gnutls_free_rand(rand);
       /* no longer need cleartext */

       e = crypt_srpsha1(username, pass_new, (const char *) tcp);
	   gnutls_free(tcp);
	   
       return e;
}
