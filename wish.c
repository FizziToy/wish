#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_ARGS 100
#define MAX_PATHS 100

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

char *find_executable(char *command, char **paths, int path_count)
{
    for (int i = 0; i < path_count; i++)
    {
        size_t length = strlen(paths[i]) + strlen(command) + 2;

        char *full_path = malloc(length);

        if (full_path == NULL)
        {
            return NULL;
        }

        snprintf(full_path, length, "%s/%s", paths[i], command);

        if (access(full_path, X_OK) == 0)
        {
            return full_path;
        }

        free(full_path);
    }

    return NULL;
}

void clear_paths(char **paths, int *path_count)
{
    for (int i = 0; i < *path_count; i++)
    {
        free(paths[i]);
        paths[i] = NULL;
    }

    *path_count = 0;
}

int execute_builtin(char **args, int arg_count, char **paths, int *path_count)
{
    if (strcmp(args[0], "exit") == 0)
    {
        if (arg_count != 1)
        {
            print_error();
            return 1;
        }
	clear_paths(paths, path_count);
        exit(0);
    }

    if (strcmp(args[0], "cd") == 0)
    {
        if (arg_count != 2)
    	{
            print_error();
            return 1;
        }

    	if (chdir(args[1]) != 0)
    	{
            print_error();
    	}

    	return 1;
    }

    if (strcmp(args[0], "path") == 0)
    {
        clear_paths(paths, path_count);

        for (int i = 1; i < arg_count; i++)
        {
            if (*path_count >= MAX_PATHS)
            {
                print_error();
                return 1;
            }

            char *new_path = strdup(args[i]);

            if (new_path == NULL)
            {
                print_error();
                clear_paths(paths, path_count);
                return 1;
            }

            paths[*path_count] = new_path;
            (*path_count)++;
        }

        return 1;
    }

    return 0;
}

void execute_command(char **args, char **paths, int path_count)
{
    char *executable = find_executable(args[0], paths, path_count);

    if (executable == NULL)
    {
        print_error();
        return;
    }

    pid_t pid = fork();

    if (pid < 0)
    {
        free(executable);
        print_error();
        return;
    }

    if (pid == 0)
    {
        execv(executable, args);

        print_error();
        free(executable);
        exit(1);
    }

    waitpid(pid, NULL, 0);
    free(executable);
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
    char *paths[MAX_PATHS];
    int path_count = 0;

    paths[0] = strdup("/bin");

    if (paths[0] == NULL)
    {
        print_error();
        free(line);
        return 1;
    }

    path_count = 1;
    while (1)
    {
        printf("wish> ");
        fflush(stdout);

        if (getline(&line, &line_capacity, stdin) == -1)
        {
	    clear_paths(paths, &path_count);
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
	if (execute_builtin(args, arg_count, paths, &path_count))
	{
    	    continue;
	}
	execute_command(args, paths, path_count);
    }
}
