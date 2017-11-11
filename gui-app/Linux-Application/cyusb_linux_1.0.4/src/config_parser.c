/***********************************************************************************************************************\
 * Program Name		:	config_parser.c										*
 * Author		:	V. Radhakrishnan ( rk@atr-labs.com )							*
 * License		:	GPL Ver 2.0										*
 * Copyright		:	Cypress Semiconductors Inc. / ATR-LABS							*
 * Date written		:	March 7, 2012										*
 * Modification Notes	:												*
 * 															*
 * This program reads the /etc/cyusb.conf file and dumps the contents on the screen.					*
 * This is mainly useful to test the syntax of the configuration file..	.						*
\***********************************************************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>

static char pidfile[256];
static char logfile[256];
static int logfd;
static int pidfd;
struct VPD {
	unsigned short	vid;
	unsigned short	pid;
	char		desc[30];
};

static struct VPD vpd[100];
static int maxdevices;

static int isempty(char *buf, int L)
{
	int flag = 1;
	int i;

	if ( L == 0 ) return 1;

	for (i = 0; i < L; ++i ) {
	    if ( (buf[i] == ' ') || ( buf[i] == '\t' ) )
		 continue;
	    else {
		 flag = 0;
		 break;
	    }
	}
	return flag;
}

int main(void)
{
	FILE *inp;
	char buf[100];
	char *cp1, *cp2, *cp3;
	int i;
	int fd;

	inp = fopen("/etc/cyusb.conf", "r");
	memset(buf,'\0',100);
	while ( fgets(buf,100,inp) ) {
		if ( buf[0] == '#' ) 			/* Any line starting with a # is a comment 	*/
		   continue;
		if ( buf[0] == '\n' )
		   continue;
		if ( isempty(buf,strlen(buf)) )		/* Any blank line is also ignored		*/
		   continue;

		cp1 = strtok(buf," =\t\n");
		if ( !strcmp(cp1,"LogFile") ) {
		   cp2 = strtok(NULL," \t\n");
		   strcpy(logfile,cp2);
		}
		else if ( !strcmp(cp1,"PIDFile") ) {
			cp2 = strtok(NULL," \t\n");
			strcpy(pidfile,cp2);
		}
		else if ( !strcmp(cp1,"<VPD>") ) {
			while ( fgets(buf,100,inp) ) {
				if ( buf[0] == '#' ) 		/* Any line starting with a # is a comment 	*/
				   continue;
				if ( buf[0] == '\n' )
				   continue;
				if ( isempty(buf,strlen(buf)) )	/* Any blank line is also ignored		*/
				   continue;
				cp1 = strtok(buf," \t\n");
				if ( !strcmp(cp1,"</VPD>") )
				   break;
				cp2 = strtok(NULL, " \t");
				cp3 = strtok(NULL, " \t\n");
				vpd[maxdevices].vid = strtol(cp1,NULL,16);
				vpd[maxdevices].pid = strtol(cp2,NULL,16);
				strcpy(vpd[maxdevices].desc,cp3);
				++maxdevices;
			}
		}
		else {
		     printf("Error in config file /etc/cyusb.conf. %s \n",buf);
		     return -1;
		}
	}
	printf("PIDFile=%s\n",pidfile);
	printf("LogFile=%s\n",logfile);
	for ( i = 0; i < maxdevices; ++i ) {
		printf("VID = %04x, PID = %04x, DESC = %-30s\n",vpd[i].vid, vpd[i].pid, vpd[i].desc);
	}
	fclose(inp);
}
