#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#define BUFSIZE 1024
#define MAXARGNUM 32
#define STACKSIZE 15
#define HISTSIZE 32
#define ALIASSIZE 15
#define DELIMITER " \t\r\n\a"

int my_cd(char **args);
int my_dirs(char **args, char **dirstack);
int my_mkdir(char **args);
int my_pipe(char *line);

char *stdcommands[] = {"cd", "exit", "dirs", "mkdir"}; // list of command the are avalible
int counter = 0;
int promptchange = 0;

char *parse(void) // function to allocate dynamic memory.
{
    int bufsize = BUFSIZE;
    int position = 0;
    char *buffer = malloc(sizeof(char) * bufsize);
    int c;
    if (!buffer)
    {
        fprintf(stderr, "unable to allocate dynamic memory\n");
        exit(EXIT_FAILURE);
    }
    while (1)
    {
        c = getchar();
        if (c == EOF)
        {
            exit(EXIT_SUCCESS);
        }
        else if (c == '\n')
        {
            buffer[position] = '\0';
            return buffer;
        }
        else
        {
            buffer[position] = c;
        }
        position++;

        if (position >= bufsize)
        {
            bufsize += BUFSIZE;
            buffer = realloc(buffer, bufsize);
            if (!buffer)
            {
                fprintf(stderr, "unable to allocate dynamic memory\n");
                exit(EXIT_FAILURE);
            }
        }
    }
}

char **splitline(char *line) // function to split input (in case of | or && or > or <)
{
    int bufsize = MAXARGNUM, position = 0;
    char **tokens = malloc(bufsize * sizeof(char *));
    char *token, **tokens_backup;

    if (!tokens)
    {
        fprintf(stderr, "unable to allocate dynamic memory\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(line, DELIMITER);
    while (token != NULL)
    {
        if (token[0] == '*' || token[strlen(token) - 1] == '*')
        {
            DIR *d;
            struct dirent *dir;
            d = opendir(".");
            if (d)
            {
                while ((dir = readdir(d)) != NULL)
                {
                    if (dir->d_name[0] != '.')
                    {
                        if (strlen(token) == 1 || (token[0] == '*' && (!strncmp(token + 1, (dir->d_name + strlen(dir->d_name) - strlen(token) + 1), strlen(token) - 1))) || (token[strlen(token) - 1] == '*' && (!strncmp(token, dir->d_name, strlen(token) - 1))))
                        {
                            tokens[position] = dir->d_name;
                            position++;
                        }
                    }
                }
                closedir(d);
            }
        }
        else
        {
            tokens[position] = token;
            position++;
        }

        if (position >= bufsize)
        {
            bufsize += MAXARGNUM;
            tokens_backup = tokens;
            tokens = realloc(tokens, bufsize * sizeof(char *));
            if (!tokens)
            {
                free(tokens_backup);
                fprintf(stderr, "unable to allocate dynamic memory\n");
                exit(EXIT_FAILURE);
            }
        }

        token = strtok(NULL, DELIMITER);
    }
    tokens[position] = NULL;
    return tokens;
}

int my_cd(char **args) // implemention of cd command
{
    if (args[1] == NULL)
    {
        if (chdir(getenv("HOME")) != 0)
        {
            perror("failed to change the directory");
            return 3;
        }
    }
    else
    {
        if (chdir(args[1]) != 0)
        {
            perror("failed to change the directory");
            return 3;
        }
    }
    return 1;
}

int my_dirs(char **args, char **dirstack) // implementation of dir command
{
    int i;
    char cwd[256];
    getcwd(cwd, sizeof(cwd));
    if (args[1] != NULL)
    {
        fprintf(stderr, "unrequired argument to \"dirs\"\n");
        return 3;
    }
    else
    {
        for (i = counter - 1; i >= 0; i--)
        {
            printf("%s%s ", cwd, dirstack[i]);
        }
        printf("%s\n", cwd);
    }
    return 1;
}

int my_mkdir(char **args) // implementation of mkdir command
{
    int c, argc = 0;
    mode_t mode = 0777;
    char path[128];
    getcwd(path, sizeof(path));
    while (args[argc] != NULL)
    {
        argc++;
    }
    while ((c = getopt(argc, args, "m:p:")) != -1)
    {
        switch (c)
        {
        case 'm':
            mode = (mode_t)strtol(optarg, NULL, 8);
            break;
        case 'p':
            strcpy(path, optarg);
            break;
        default:
            fprintf(stderr, "-m or -p options only for \"%s\"\n", args[0]);
            return 3;
        }
    }
    argc -= optind;
    args += optind;
    if (*args)
    {
        strcat(path, "/");
        strcat(path, *args);
        if (mkdir(path, mode) == -1)
        {
            perror("mkdir failed ");
            return 3;
        }
    }
    else
    {
        fprintf(stderr, "expected directory name argument for \"mkdir\"\n");
        return 3;
    }
    return 1;
}

int my_pipe(char *line) // function to exec the pipe input
{
    int i, commandc = 0, numpipes = 0, status;
    pid_t pid;
    char **args;
    for (i = 0; line[i] != '\0'; i++)
    {
        if (i > 0)
        {
            if (line[i] == '|' && line[i + 1] != '|' && line[i - 1] != '|')
            {
                numpipes++;
            }
        }
    }
    int *pipefds = (int *)malloc((2 * numpipes) * sizeof(int));
    char *token = (char *)malloc((128) * sizeof(char));
    token = strtok_r(line, "|", &line);
    for (i = 0; i < numpipes; i++)
    {
        if (pipe(pipefds + i * 2) < 0)
        {
            perror("failed to process pipe opertion");
            return 3;
        }
    }
    do
    {
        pid = fork();
        if (pid == 0)
        { // child process
            if (commandc != 0)
            {
                if (dup2(pipefds[(commandc - 1) * 2], 0) < 0)
                {
                    perror("no input for child");
                    exit(1);
                }
            }
            if (commandc != numpipes)
            {
                if (dup2(pipefds[commandc * 2 + 1], 1) < 0)
                {
                    perror("no output given by child");
                    exit(1);
                }
            }
            for (i = 0; i < 2 * numpipes; i++)
            {
                close(pipefds[i]);
            }
            args = splitline(token);
            execvp(args[0], args);
            perror("failed to exec");
            exit(1);
        }
        else if (pid < 0)
        {
            perror("failed to fork()");
            return 3;
        }           // fork error
        commandc++; // parent process
    } while (commandc < numpipes + 1 && (token = strtok_r(NULL, "|", &line)));
    for (i = 0; i < 2 * numpipes; i++)
    {
        close(pipefds[i]);
    }
    free(pipefds);
    return 1;
}

int my_launch(char **args)
{
    pid_t pid;
    int status;
    pid = fork();
    if (pid == 0)
    { // child process
        if (execvp(args[0], args) == -1)
        {
            perror("child process: error while processing child");
            return 3;
        }
        exit(EXIT_FAILURE);
    }
    else if (pid < 0)
    {
        perror("forking error");
        return 3;
    }
    else
    { // parent process
        do
        {
            waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }
    if (status == 0)
        return 1;
    return 3;
}

int execute(char **args, char **dirstack) // calls function when required.
{
    int i;
    if (args[0] == NULL)
    {
        return 1;
    }
    for (i = 0; i < (sizeof(stdcommands) / sizeof(char *)); i++)
    {
        if (strcmp(args[0], stdcommands[i]) == 0)
        {
            switch (i)
            {
            case 0:
                return my_cd(args);
                break;
            case 1:
                return 0;
                break;
            case 2:
                return my_dirs(args, dirstack);
                break;
            case 3:
                return my_mkdir(args);
                break;
            }
        }
    }
    return my_launch(args);
}

int main(int argc, char **argv)
{
    int i, j = 0, scriptfn = 0, status = 1;
    char *line, *oneline, *tmp, **args, *saveptr, prompt[256], *gotline = "s";
    char **dirstack = (char **)malloc((STACKSIZE + 1) * sizeof(char *));
    for (i = 0; i < STACKSIZE + 1; i++)
        dirstack[i] = (char *)malloc((30) * sizeof(char));
    FILE *fp;
    if (argc == 3 && strcmp(argv[1], "<") == 0)
    {
        scriptfn++;
        printf("\n\n%d \n ", scriptfn);
        fp = fopen(argv[2], "r");
        if (fp == NULL)
        {
            perror("could not open file");
            return EXIT_FAILURE;
        }
    }
    do
    {
        j = 0;
        if (promptchange == 0)
            getcwd(prompt, sizeof(prompt));
        if (scriptfn == 1)
        {
            printf("\n");
            gotline = fgets(line, sizeof(line), fp);
        }
        else
        {
            printf("\nSharishth@12041370 $ ");
            line = parse();
            if(line == "exit"){
                break;
            }
        }
        for (i = 0; line[i] != '\0'; i++)
        {
            if (line[i] == ';')
            {
                j++;
            }
            else if (i > 0 && line[i - 1] == '&' && line[i] == '&')
            {
                j++;
                line[i - 1] = ';';
            }
            else if (i > 0 && line[i - 1] == '|' && line[i] == '|')
            {
                j++;
                line[i - 1] = ';';
            }
            else if (i > 0 && line[i] == '|' && line[i - 1] != '|' && line[i + 1] != '|')
            {
                my_pipe(line);
                goto piped;
            }
        }
        tmp = (char *)malloc((strlen(line) + 1) * sizeof(char));
        strcpy(tmp, ";");
        strcat(tmp, line);
        oneline = strtok_r(tmp, ";", &tmp);
        do
        {
            if (oneline != NULL)
            {
                if (oneline[0] == '&' && status != 1)
                {
                    break;
                }
                else
                {
                    if (oneline[0] == '&')
                    {
                        oneline++;
                    }
                    args = splitline(oneline);
                    status = execute(args, dirstack);
                }
            }
            oneline = strtok_r(NULL, ";", &tmp);
            j--;
        } while (j > -1 && status);
        if (status == 2)
        {
            promptchange = 1;
            if (args[1] != NULL)
            {
                strcpy(prompt, args[1]);
            }
            else
            {
                strcpy(prompt, "Cmd: ");
            }
        }
        free(args);
    piped:
        free(line);
    } while (status && gotline != NULL);
    if (scriptfn == 1)
    {
        fclose(fp);
    }
    free(tmp);
    free(dirstack);
    return EXIT_SUCCESS;
}