#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/wait.h>


/*function prototypes*/
static char *readline(void);
static int parseline(char *cmd, char **args);
static int checkmem(int index, int size, int *times, char **buf);
static int check_exit(char **args);
static void free_mem(char **args);
static int handle_cd(char **args, char **stack, int index);
static int handle_pushd(char **args, char **stack, int *index);
static int handle_popd(char **args, char **stack, int *index);
static int handle_dirs(char **args, char **stack);
static void print_stack(char **stack);
static int handle_path(char **args, char **path);
static int addpath(char **path, char *arg);
static int rmpath(char **path, char *arg);
static void printpath(char **path);
static int find_comd(char **cmd, char **path);
static int concatpath(char **cmd, char *path);
static void switch_stack(char **stack, int top);
static void init_block(char **block, int size);
static int strcp(char *dest, char *sour);

#define MAXARGS 50

int main(int argc, char **argv)
{
	/*argument array, maximum arguments is 50*/
	char *args[MAXARGS];
	char *cmd;
	char *command;
	int sig;
	pid_t pid;
	char *stack[1024];
	char *path[1024];
	size_t length;
	int index = 0;
	init_block(stack, 1024);
	init_block(path, 1024);
	while (1) {
		init_block(args, MAXARGS);
		if (index == 0) {
			free(stack[0]);
			stack[0] = getcwd(NULL, 0);
		}
		printf("[%s]: ", stack[index]);
		cmd = readline();
		if (cmd == NULL) {
			free_mem(stack);
			free_mem(path);
			return EXIT_FAILURE;
		}
		sig = parseline(cmd, args);
		free(cmd);
		if (sig == 1) {
			free_mem(args);
			continue;
		}
		if (sig == 2) {
			free_mem(args);
			free_mem(stack);
			free_mem(path);
			return EXIT_FAILURE;
		}
		if (args[0] == NULL)
			continue;
		sig = check_exit(args);
		if (sig == 1) {
			printf("[Process completed]\n");
			break;
		} else if (sig == -1) {
			free_mem(args);
			continue;
		}

		if (handle_cd(args, stack, index) == 1) {
			free_mem(args);
			continue;
		}
		if (handle_pushd(args, stack, &index) == 1) {
			free_mem(args);
			continue;
		}
		if (handle_popd(args, stack, &index) == 1) {
			free_mem(args);
			continue;
		}
		if (handle_dirs(args, stack)) {
			free_mem(args);
			continue;
		}
		if (handle_path(args, path)) {
			free_mem(args);
			continue;
		}
		pid = fork();
		if (pid == 0) {
			length = strlen(args[0]) + 1;
			command = (char *) malloc(sizeof(char) * length);
			strcp(command, args[0]);
			find_comd(&command, path);
			execv(command, args);
			free(command);
			free_mem(args);
			free_mem(stack);
			free_mem(path);
			return EXIT_FAILURE;
		} else if (pid == -1) {
			fprintf(stderr, "ERROR: can't fork a child\n");
			free_mem(args);
			free_mem(stack);
			free_mem(path);
			return EXIT_FAILURE;
		}
		while (pid != wait(0))
			;
	}
	free_mem(stack);
	free_mem(path);
	free_mem(args);
	return EXIT_SUCCESS;
}

/*read the inputed command*/
char *readline()
{
	char c;
	int index = 0;
	int times = 1;
	int size = 100;
	char *cmdline = (char *) malloc(sizeof(char) * size);
	if (cmdline == NULL) {
		fprintf(stderr, "ERROR: can't allocate memory space\n");
		return NULL;
	}
	while (((c = getchar()) != '\n')) {
		if (checkmem(index, size, &times, &cmdline) == 1)
			return NULL;
		cmdline[index++] = c;
	}
	cmdline[index] = '\0';
	return cmdline;
}

/*parse the command line cmd and put arguments in the arguments array args*/
int parseline(char *cmd, char **args)
{
	int i = 0;
	int index = 0;
	int size = 32;
	while (cmd[i] == ' ')
		i++;
	if (cmd[i] == '\0')
		return 0;
	while (cmd[i] != '\0') {
		int count = 0;
		int times = 1;
		if (index > MAXARGS - 1) {
			fprintf(stderr, "ERROR: too many arguments\n");
			return 1;
		}
		args[index] = (char *) malloc(sizeof(char) * size);
		if (args[index] == NULL) {
			fprintf(stderr, "ERROR: can't allocate memory space\n");
			return 2;
		}
		while (cmd[i] != '\0' && cmd[i] != ' ') {
			if (checkmem(count, size, &times, &args[index]) == 1)
				return 2;
			args[index][count++] = cmd[i++];
		}
		args[index][count] = '\0';
		index++;
		while (cmd[i] == ' ')
			i++;
	}
	return 0;
}

/*check if the allocated space buf is filled
 if yes, reallocated a memory*/
int checkmem(int index, int size, int *times, char **buf)
{
	char *temp;
	if (index > size * (*times) - 2) {
		*times = *times + 1;
		temp = (char *) realloc(*buf, sizeof(char) * (size * (*times)));
		if (temp == NULL) {
			fprintf(stderr, "ERROR: can't allocate memory space\n");
			free(*buf);
			return 1;
		}
		*buf = temp;
	}
	return 0;
}

/*check if the command is exit*/
int check_exit(char **args)
{
	int i;
	if (strcmp(args[0], "exit") != 0)
		return 0;
	if (args[2] != NULL) {
		fprintf(stderr, "ERROR: too many arguments\n");
		return -1;
	}
	if (args[1] == NULL)
		return 1;
	for (i = 0; i < (strlen(args[1])); i++) {
		if (isdigit(args[1][i]) == 0) {
			fprintf(stderr, "WARNING: numeric argument required\n");
			break;
		}
	}
	return 1;
}

/*free memory allocated for the array of arguments*/
void free_mem(char **args)
{
	int i;
	for (i = 0; args[i] != NULL; i++) {
		free(args[i]);
		args[i] = NULL;
	}
	return;
}

/*check and handle cd command*/
int handle_cd(char **args, char **stack, int index)
{
	if (strcmp(args[0], "cd") != 0)
		return 0;
	if (args[1] == NULL)
		return 1;
	if ((chdir(args[1])) == -1) {
		fprintf(stderr, "ERROR: can't change directory\n");
	} else {
		free(stack[index]);
		stack[index] = getcwd(NULL, 0);
	}
	return 1;
}


int handle_pushd(char **args, char **stack, int *index)
{
	if (strcmp(args[0], "pushd") != 0)
		return 0;
	if (args[1] == NULL) {
		switch_stack(stack, *index);
		return 1;
	}
	if (*index < 1023) {
		(*index)++;
	} else {
		fprintf(stderr, "ERROR: directory stack is full\n");
		return 1;
	}
	if ((chdir(args[1])) == -1) {
		fprintf(stderr, "ERROR: can't change directory\n");
		(*index)--;
	} else {
		stack[*index] = getcwd(NULL, 0);
		print_stack(stack);
	}
	return 1;
}

int handle_popd(char **args, char **stack, int *index)
{
	if (strcmp(args[0], "popd") != 0)
		return 0;
	if (args[1] != NULL) {
		print_stack(stack);
		return 1;
	}
	if (*index != 0) {
		if ((chdir(stack[*index - 1])) == -1) {
			fprintf(stderr, "ERROR: can't change directory\n");
			return 1;
		}
	} else {
		if ((chdir(stack[*index])) == -1) {
			fprintf(stderr, "ERROR: can't change directory\n");
			return 1;
		}
	}
	free(stack[*index]);
	stack[*index] = NULL;
	print_stack(stack);
	if (*index != 0)
		(*index)--;
	return 1;
}

int handle_dirs(char **args, char **stack)
{
	if (strcmp(args[0], "dirs") != 0)
		return 0;
	if (args[1] != NULL)
		fprintf(stderr, "ERROR: options are not supported\n");
	else
		print_stack(stack);
	return 1;
}

/*print the directory stack from the top of the stack*/
void print_stack(char **stack)
{
	int i = 0;
	while (stack[i] != NULL)
		i++;
	if (i == 0) {
		printf("directory stack empty\n");
		return;
	}
	for (i = i - 1; i >= 0; i--)
		printf("%s ", stack[i]);
	printf("\n");
}

int handle_path(char **args, char **path)
{
	if (strcmp(args[0], "path") != 0)
		return 0;
	if (args[1] != NULL && args[2] == NULL) {
		fprintf(stderr, "ERROR: path not specified\n");
		return 1;
	}
	if (args[1] == NULL) {
		printpath(path);
	} else if (strcmp(args[1], "+") == 0) {
		addpath(path, args[2]);
	} else if (strcmp(args[1], "-") == 0) {
		if (rmpath(path, args[2]) == 0)
			printf("WARNING: the path doesn't exist\n");
	} else {
		fprintf(stderr, "ERROR: invalid argument\n");
	}
	return 1;
}

int addpath(char **path, char *arg)
{
	int i = 0;
	size_t length;
	length = strlen(arg);
	if ((length != 1) && (arg[length - 1] == '/'))
		arg[length - 1] = '\0';
	while (path[i] != NULL) {
		if (strcmp(path[i], arg) == 0) {
			printf("INFO: the path already exists\n");
			printpath(path);
			return 1;
		}
		if (i < 1023) {
			i++;
		} else {
			fprintf(stderr, "ERROR: path list is full\n");
			return 1;
		}
	}
	path[i] = (char *) malloc(sizeof(char) * (strlen(arg) + 1));
	if (path[i] == NULL) {
		fprintf(stderr, "ERROR: can't allocate memory space\n");
		return 1;
	}
	strcp(path[i], arg);
	printpath(path);
	return 0;
}

int rmpath(char **path, char *arg)
{
	size_t length;
	int i = 0;
	length = strlen(arg);
	if ((length != 1) && (arg[length - 1] == '/'))
		arg[length - 1] = '\0';
	while (path[i] != NULL) {
		if (strcmp(path[i], arg) != 0) {
			i++;
			continue;
		}
		while (path[i + 1] != NULL) {
			free(path[i]);
			path[i]	= path[i + 1];
			i++;
		}
		path[i] = NULL;
		printpath(path);
		return 1;
	}
	return 0;
}

void printpath(char **path)
{
	int i = 0;
	while (path[i] != NULL) {
		if (i == 0)
			printf("%s", path[i]);
		else
			printf(":%s", path[i]);
		i++;
	}
	if (i == 0)
		printf("INFO: path empty");
	printf("\n");
}

int find_comd(char **cmd, char **path)
{
	int fp;
	int i = 0;
	fp = open(*cmd, O_RDONLY);
	if (fp >= 0) {
		close(fp);
		return 0;
	}
	while (path[i] != NULL) {
		if (concatpath(cmd, path[i]) != 0)
			return -1;
		fp = open(*cmd, O_RDONLY);
		if (fp >= 0) {
			close(fp);
			return 0;
		}
		i++;
	}
	fprintf(stderr, "ERROR: command not found\n");
	return 1;
}

int concatpath(char **cmd, char *path)
{
	size_t length;
	char *temp;
	length = strlen(path) + strlen(*cmd) + 2;
	temp = *cmd;
	*cmd = (char *) malloc(sizeof(char) * length);
	if (*cmd == NULL) {
		*cmd = temp;
		fprintf(stderr, "ERROR: can't allocate memory space\n");
		return 1;
	}
	strcp(*cmd, path);
	strcat(*cmd, "/");
	strcat(*cmd, temp);
	free(temp);
	return 0;
}

void switch_stack(char **stack, int top)
{
	char *temp;
	if (top == 0) {
		fprintf(stderr, "WARNING: no other directory\n");
		return;
	}
	temp = stack[top - 1];
	stack[top - 1] = stack[top];
	stack[top] = temp;
	if ((chdir(stack[top])) == -1) {
		fprintf(stderr, "ERROR: can't change directory\n");
		return;
	}
	print_stack(stack);
	return;
}

void init_block(char **block, int size)
{
	int i;
	for (i = 0; i < size; i++)
		block[i] = NULL;
}

/*copy string sour to dest, return the length of string copied*/
int strcp(char *dest, char *sour)
{
	int i = 0;
	while (sour[i] != '\0') {
		dest[i] = sour[i];
		i++;
	}
	dest[i] = '\0';
	return i;
}

