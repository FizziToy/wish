#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_ARGS 100
#define MAX_PATHS 100

void print_error(void)
{
    char error_message[30] = "An error has occurred\n";
    write(STDERR_FILENO, error_message, strlen(error_message));
}

char *normalize_operators(const char *line)
{
    size_t length = strlen(line);
    char *result = malloc(length * 3 + 1);

    if (result == NULL)
    {
        return NULL;
    }

    size_t j = 0;

    for (size_t i = 0; i < length; i++)
    {
        if (line[i] == '>' || line[i] == '&')
        {
            result[j++] = ' ';
            result[j++] = line[i];
            result[j++] = ' ';
        }
        else
        {
            result[j++] = line[i];
        }
    }

    result[j] = '\0';
    return result;
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

int setup_redirection(char *output_file)
{
    int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);

    if (fd < 0)
    {
        return -1;
    }

    if (dup2(fd, STDOUT_FILENO) < 0)
    {
        close(fd);
        return -1;
    }

    if (dup2(fd, STDERR_FILENO) < 0)
    {
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

pid_t execute_command(char **args, char **paths, int path_count, char *output_file)
{
    char *executable = find_executable(args[0], paths, path_count);

    if (executable == NULL)
    {
        print_error();
        return -1;
    }

    pid_t pid = fork();

    if (pid < 0)
    {
        free(executable);
        print_error();
        return -1;
    }

    if (pid == 0)
    {
        if (output_file != NULL)
        {
            if (setup_redirection(output_file) != 0)
            {
                print_error();
                free(executable);
                exit(1);
            }
        }

        execv(executable, args);

        print_error();
        free(executable);
        exit(1);
    }

    free(executable);
    return pid;
}

int parse_redirection(char **args, int *arg_count, char **output_file)
{
    int redirect_index = -1;

    *output_file = NULL;

    for (int i = 0; i < *arg_count; i++)
    {
        if (strcmp(args[i], ">") == 0)
        {
            if (redirect_index != -1)
            {
                return -1;
            }

            redirect_index = i;
        }
    }

    if (redirect_index == -1)
    {
        return 0;
    }

    if (redirect_index == 0)
    {
        return -1;
    }

    if (redirect_index != *arg_count - 2)
    {
        return -1;
    }

    *output_file = args[redirect_index + 1];
    args[redirect_index] = NULL;
    *arg_count = redirect_index;

    return 0;
}

pid_t process_command(char **args, int arg_count, char **paths, int *path_count)
{
    if (arg_count == 0)
    {
        return -1;
    }

    char *output_file = NULL;

    if (parse_redirection(args, &arg_count, &output_file) != 0)
    {
        print_error();
        return -1;
    }

    if (execute_builtin(args, arg_count, paths, path_count))
    {
        return -1;
    }

    return execute_command(args, paths, *path_count, output_file);
}

int execute_line(char **args, int arg_count, char **paths, int *path_count)
{
    pid_t pids[MAX_ARGS];
    int pid_count = 0;
    int command_start = 0;

    for (int i = 0; i <= arg_count; i++)
    {
        if (i == arg_count || strcmp(args[i], "&") == 0)
        {
            int command_arg_count = i - command_start;

            if (command_arg_count > 0)
            {
                args[i] = NULL;

                pid_t pid = process_command(
                    &args[command_start],
                    command_arg_count,
                    paths,
                    path_count
                );

                if (pid > 0)
                {
                    pids[pid_count] = pid;
                    pid_count++;
                }
            }

            command_start = i + 1;
        }
    }

    for (int i = 0; i < pid_count; i++)
    {
        waitpid(pids[i], NULL, 0);
    }

    return 0;
}

void run_shell(FILE *input, int interactive, char **paths, int *path_count)
{
    char *line = NULL;
    size_t line_capacity = 0;
    char *args[MAX_ARGS];

    while (1)
    {
        if (interactive)
        {
            printf("wish> ");
            fflush(stdout);
        }

        if (getline(&line, &line_capacity, input) == -1)
        {
            free(line);
            return;
        }
	char *normalized_line = normalize_operators(line);

	if (normalized_line == NULL)
	{
    	    print_error();
    	    continue;
	}

        int arg_count = parse_command(normalized_line, args, MAX_ARGS);

        if (arg_count < 0)
        {
            print_error();
	    free(normalized_line);
            continue;
        }

        if (arg_count == 0)
        {
	    free(normalized_line);
            continue;
        }

        execute_line(args, arg_count, paths, path_count);
	free(normalized_line);
    }
}

int main(int argc, char *argv[])
{
    if (argc > 2)
    {
        print_error();
        return 1;
    }

    (void)argv;

    char *paths[MAX_PATHS];
    int path_count = 0;

    paths[0] = strdup("/bin");

    if (paths[0] == NULL)
    {
        print_error();
        return 1;
    }

    path_count = 1;

    if (argc == 1)
    {
        run_shell(stdin, 1, paths, &path_count);
    }
    else
    {
        FILE *batch_file = fopen(argv[1], "r");

        if (batch_file == NULL)
        {
            print_error();
            clear_paths(paths, &path_count);
            return 1;
        }

        run_shell(batch_file, 0, paths, &path_count);
        fclose(batch_file);
    }

    clear_paths(paths, &path_count);

    return 0;
}
