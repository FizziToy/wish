#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_ARGS 100
void print_error(void)
{
    char error_message[30] = "An error has occurred\n";
    write(STDERR_FILENO, error_message, strlen(error_message));
}
int parse_command(char *line, char **args, int max_args)
{
    int arg_count = 0;
    char *token;

    while ((token = strsep(&line, " \t\n")) != NULL)
    {
        if (*token == '\0')
        {
            continue;
        }

        if (arg_count >= max_args - 1)
        {
            return -1;
        }

        args[arg_count] = token;
        arg_count++;
    }

    args[arg_count] = NULL;

    return arg_count;
}
void execute_command(char **args)
{
    pid_t pid = fork();

    if (pid < 0)
    {
        print_error();
        return;
    }

    if (pid == 0)
    {
        execv("/bin/ls", args);

        print_error();
        exit(1);
    }

    waitpid(pid, NULL, 0);
}

int main(int argc, char *argv[])
{
    if (argc > 2)
    {
        print_error();
        return 1;
    }

    (void)argv;

    char *line = NULL;
    size_t line_capacity = 0;
    char *args[MAX_ARGS];

    while (1)
    {
        printf("wish> ");
        fflush(stdout);

        if (getline(&line, &line_capacity, stdin) == -1)
        {
            free(line);
            exit(0);
        }
	int arg_count = parse_command(line, args, MAX_ARGS);

	if (arg_count < 0)
	{
    	    print_error();
    	    continue;
	}

	if (arg_count == 0)
	{
    	    continue;
	}

	execute_command(args);
    }
}
