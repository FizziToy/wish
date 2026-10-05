#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void print_error(void)
{
    char error_message[30] = "An error has occurred\n";
    write(STDERR_FILENO, error_message, strlen(error_message));
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

    while (1)
    {
        printf("wish> ");
        fflush(stdout);

        if (getline(&line, &line_capacity, stdin) == -1)
        {
            free(line);
            exit(0);
        }
    }
}
