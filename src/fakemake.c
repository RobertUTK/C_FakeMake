/*This program simulates make to create an exe from the info of a given file.
 *Author: Robert Schaffer
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "fields.h"
#include "dllist.h"
#include "jval.h"

static void addToList(Dllist, char **, const int);
static void listCleanUp(Dllist *);
static char* changeFileToO(const char *);
static time_t getMaxModTime(Dllist *);
static time_t checkAndMakeO(Dllist *, time_t);
static char* makeOString(const char *, Dllist *);
static char* makeExeString(Dllist*, const char *);

int 
main(int argc, char **argv)
{
	IS fin;
	int isEOF, exeNameRead, i, exeExists, cmdFailed, count;
	char *exeName,*exeString;
	Dllist lists[4];//cList, hList, fList, lList;
	time_t maxHTime, maxOTime;
	struct stat exeInfo;


	if(argc > 2){
		fprintf(stderr, "Usage: %s [description-file]\n", argv[0]);
		exit(1);
	}

	if(argc == 2){ 
		fin = new_inputstruct(argv[1]);
		if(fin == NULL){ fprintf(stderr,"fmakefile: ");  perror(argv[1]); exit(1);}
	}
	else {
		fin = new_inputstruct("fmakefile");
		if(fin == NULL){ perror("fmakefile"); exit(1);}
	}

	for(i = 0; i < 4; i++) lists[i] = new_dllist();
	
	exeName = NULL;
	exeNameRead = 0;
	
	count = 0;
	isEOF = get_line(fin);
	while(isEOF != -1){
		count++;
		if(fin->NF == 0) /*Skip*/;
		else if(fin->fields[0][0] == 'C') addToList(lists[0], fin->fields, fin->NF);
		else if(fin->fields[0][0] == 'H') addToList(lists[1], fin->fields, fin->NF);
		else if(fin->fields[0][0] == 'F') addToList(lists[2], fin->fields, fin->NF);
		else if(fin->fields[0][0] == 'L') addToList(lists[3], fin->fields, fin->NF);
		else if(fin->fields[0][0] == 'E') {
			exeName = (char*) realloc(exeName, sizeof(char)*(strlen(fin->fields[1])+1));
			memcpy(exeName, fin->fields[1], (strlen(fin->fields[1]) + 1));
			exeNameRead++;
			if(exeNameRead > 1){
				fprintf(stderr, "fmakefile (%d) cannot have more than one E line\n", count);	
				free(exeName);
				listCleanUp(lists);
				exit(1);
			}
		}
		else { 
			fprintf(stderr, "Read error\n"); 
			listCleanUp(lists);
			if(exeNameRead) free(exeName);
			jettison_inputstruct(fin);
			exit(1); 
		}
		isEOF = get_line(fin);
	}
	jettison_inputstruct(fin);

	
	if(!exeNameRead){
		fprintf(stderr, "No executable specified\n");	
		listCleanUp(lists);
		exit(1);
	}

	maxHTime = getMaxModTime(lists);
	maxOTime = checkAndMakeO(lists, maxHTime);
	exeExists = stat(exeName, &exeInfo);
	if(exeExists == -1 || maxOTime == 1 || maxOTime > exeInfo.st_mtime){
		exeString = makeExeString(lists, exeName);
		printf("%s\n",exeString);
		cmdFailed = system(exeString);
		free(exeString);
		if (cmdFailed != 0){
			fprintf(stderr, "Command failed.  Fakemake exiting\n");
			listCleanUp(lists);
			free(exeName);
			exit(1);
		}
	}
	else printf("%s up to date\n", exeName);

	listCleanUp(lists);
	free(exeName);
	return 0;
}

/*addToList: Allocates memory for some number of strings and adds them to a Dllist
 *Params:list, the list to add the string
 *		 fields, a pointer to the list of strings
 *		 NF, number of strings
 *Post-Con: Memory will be allocated for the strings and added to list
 */
static void
addToList(Dllist list, char **fields, const int NF)
{
	int i;
	char *tmp;
	
	for(i = 1; i < NF; i++){
		tmp = (char*) malloc(sizeof(char)*(strlen(fields[i]) + 1));
		memcpy(tmp, fields[i], (strlen(fields[i]) + 1));
		dll_append(list, new_jval_s(tmp));
	}
}

/*listCleanUp: Free the memory allocated in addToList and the lists
 * Params: lists, an array of lists
 * Post-Con: Memory will be freed for all lists
 */
static void
listCleanUp(Dllist *lists)
{
	Dllist tmp;
	int i;

	for(i = 0; i < 4; i++){
		dll_traverse(tmp, lists[i]){
			free(tmp->val.s);
		}
		free_dllist(lists[i]);
	}
}

/*changeFileToO: Creates a new string with a .o
 *Params: fileName, the string for modifying
 *returns: name, the string ending in .o
 */
static char*
changeFileToO(const char *fileName)
{
	int i;
	char *name;
	
	for(i = 0; i < strlen(fileName); i++) if(fileName[i] == '.') break;

	name = (char*) malloc(sizeof(char)*(i+3));
	memcpy(name, fileName, i + 1);
	name[i+1] = 'o';
	name[i+2] = '\0';
	return name;
}

/*getMaxModTime: gets the maximum modified time of the header files
 *Param: lists, an array of lists
 *Returns: maxTime, a time_t struct with the most recent modified time
 */
static time_t
getMaxModTime(Dllist *lists)
{
	int test;
	Dllist tmp;
	struct stat statbuf;
	time_t maxTime = 0;

	dll_traverse(tmp, lists[1]){
		test = stat(tmp->val.s, &statbuf);
		if(test == -1){ perror(tmp->val.s); listCleanUp(lists); exit(1);}
		if(maxTime < statbuf.st_mtime) maxTime = statbuf.st_mtime;
	}
	return maxTime;
}

/*checkAndMakeO: checks for the .o files and makes them if they do not exist or the header or c 
 * file has been more recently modified that the .o
 * Params: lists, array of list with the file names
 *		   maxHTime, the time of the most recently modified header file
 *Returns: maxOTime. the time of the last modified .o file
 */
static time_t
checkAndMakeO(Dllist *lists, time_t maxHTime)
{
	Dllist tmp;
	char *tmpName, *oString;
	struct stat statbuf1, statbuf2;
	int test, cmdFailed;
	time_t maxOTime = 0;

	dll_traverse(tmp, lists[0]){
		test = stat(tmp->val.s, &statbuf1);
		if(test == -1){ fprintf(stderr, "fmakefile: "); perror(tmp->val.s); listCleanUp(lists); exit(1);}
		else{
			tmpName = changeFileToO(tmp->val.s);
			test = stat(tmpName, &statbuf2);
			if(maxOTime != 1 && maxOTime < statbuf2.st_mtime) maxOTime = statbuf2.st_mtime;
			if(test == -1 || statbuf1.st_mtime > statbuf2.st_mtime || maxHTime > statbuf2.st_mtime){
				oString = makeOString(tmp->val.s, lists);
				printf("%s\n", oString);
				cmdFailed = system(oString);
				free(oString);
				maxOTime = 1;
				if (cmdFailed != 0){
					fprintf(stderr, "Command failed.  Exiting\n");
					listCleanUp(lists);
					exit(1);
				}
			}
			free(tmpName);
		}
	}
	return maxOTime;
}

/*makeOString: creates the string for creating the .o files
 * Params: cName, name of the .c file
 *		   lists, an array of lists that has the .c/.h file names along with the flags and links
 *Returns: oString, the string to create the .o files
 */
static char*
makeOString(const char *cName, Dllist *lists)
{
	char *oString;
	int length;
	Dllist tmp;
	
	length = 6;
	length += (strlen(cName)+1);
	dll_traverse(tmp, lists[2]){
		length += (strlen(tmp->val.s)+1);
	}

	oString = (char*) malloc(sizeof(char)*(length+1));
	
	strcpy(oString, "gcc -c");
	length = 6;
	dll_traverse(tmp, lists[2]){
		oString[length] = ' ';
		strcpy(oString+length+1, tmp->val.s);
		length += (strlen(tmp->val.s)+1);
	}
	oString[length] = ' ';
	strcpy(oString+length+1, cName);
	length += (strlen(cName)+1);
	return oString;
}

/*makeOString: creates the string for creating the executable
 * Params: lists, an array of lists that has the .c/.h file names along with the flags and links
 *		   exeName, the name for the exe
 *Returns: exeString, the string to create the exe
 */

static char*
makeExeString(Dllist *list, const char *exeName)
{
	Dllist tmp;
	int length = 6;
	char *exeString, *tmpName;

	length += (strlen(exeName)+1);
	dll_traverse(tmp, list[0]){
		length += (strlen(tmp->val.s)+1);
	}
	dll_traverse(tmp, list[2]){
		length += (strlen(tmp->val.s)+1);
	}
	dll_traverse(tmp, list[3]){
		length += (strlen(tmp->val.s)+1);
	}

	exeString = (char*) malloc(sizeof(char)*(length+1));
	
	strcpy(exeString, "gcc -o");
	length = 6;
	exeString[length] = ' ';
	strcpy(exeString+length+1, exeName);
	length += (strlen(exeName)+1);
	
	dll_traverse(tmp, list[2]){
		exeString[length] = ' ';
		strcpy(exeString+length+1, tmp->val.s);
		length += (strlen(tmp->val.s)+1);
	}
	dll_traverse(tmp, list[0]){
		exeString[length] = ' ';
		tmpName = changeFileToO(tmp->val.s);
		strcpy(exeString+length+1, tmpName);
		length += (strlen(tmp->val.s)+1);
		free(tmpName);
	}
	dll_traverse(tmp, list[3]){
		exeString[length] = ' ';
		strcpy(exeString+length+1, tmp->val.s);
		length += (strlen(tmp->val.s)+1);
	}
	return exeString;
	
}
