// Including libraries
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <termios.h>
#include <sys/wait.h>
#include <sys/types.h>

// Defining constants
#define MAXLINE 150
#define MAXARGS 100

// Variables
pid_t pid = -1;
int background;
int counter = 0;
int prompt_count;
int job_list[100];

// Function prototypes
void jobs();
void help_description();
void shell_initialization();
void int_signal_handler(int args);
void child_signal_handler(int args);
int parsing(char *cmdline, char **argv);
static void handling_signals(int signum);
void IO_redirection(int argc, char **argv);

// Main function
int main(void)
{
    struct sigaction parent_handler;
    parent_handler.sa_handler = SIG_IGN;
    sigemptyset(&parent_handler.sa_mask);
    parent_handler.sa_flags = 0;
    sigaction(SIGINT, &parent_handler, NULL);

    struct sigaction child_handler;
    child_handler.sa_handler = SIG_DFL;
    sigemptyset(&child_handler.sa_mask);
    child_handler.sa_flags = 0;

    char cmdline[MAXLINE];
    char *argv[MAXARGS];
    int argc;
    int status;
    static char *curr_directory;
    shell_initialization();
    curr_directory = (char *)calloc(1024, sizeof(char));

    printf("CONGRATULATIONS!\n");
    printf("THE SHELL HAS BEEN STARTED. Enjoy :D\n\n");

    while (1)
    {
        printf("%s %s> ", "ZaidiShell:", getcwd(curr_directory, 1024));
        fgets(cmdline, MAXLINE, stdin);

        // Restart loop on carriage return
        argc = parsing(cmdline, argv);
        if (argc == 0)
        {
            continue;
        }

        // Handle exit command
        if (argc == 1 && !strcmp(argv[0], "exit"))
        {
            if (counter == 0)
            {
                exit(0);
            }
            else
            {
                printf("Error - Exiting shell with background jobs running.\n");
            }
        }

        // Handle clear command
        if (argc == 1 && !strcmp(argv[0], "clear"))
        {
            system("clear");
        }

        // Handle help command
        if (argc == 1 && !strcmp(argv[0], "help"))
        {
            help_description();
        }

        // Handle pwd command
        if (argc == 1 && !strcmp(argv[0], "pwd"))
        {
            char cwd[1024];
            getcwd(cwd, sizeof(cwd));
            printf("%s\n", cwd);
            continue;
        }

        // Handle cd command
        if (!strcmp(argv[0], "cd"))
        {
            if (argc > 2)
            {
                printf("ERROR - Cannot function multiple arguements\n");
            }
            else if (!argv[1])
            {
                chdir(getenv("HOME"));
            }
            else
            {
                char dir[1024] = {0};
                if (argv[1][0] != '/')
                {
                    getcwd(dir, sizeof(dir));
                    strcat(dir, "/");
                }
                strcat(dir, argv[1]);
                chdir(dir);
            }
            char dir[1024];
            getcwd(dir, sizeof(dir));
            setenv("PWD", dir, 1);
            continue;
        }

        // Background process
        if (strcmp(argv[0], "&") == 0)
        {
            background = 1;
        }

        // For jobs command
        if (strcmp(argv[0], "jobs") == 0)
        {
            jobs();
        }

        // For kill command
        // Reference from: https://www.guru99.com/c-file-input-output.html#:~:text=To%20create%20a%20file%20in,used%20to%20open%20a%20file.
        if (strcmp(argv[0], "kill") == 0)
        {
            if (argc > 2)
            {
                printf("ERROR - Cannot function multiple arguements\n");
            }
            else if (!argv[1])
            {
                printf("ERROR - No arguement provided\n");
            }
            else
            {
                int kill_pid = atoi(argv[1]);
                int kill_status = kill(kill_pid, SIGINT);
                if (kill_status == 0)
                {
                    printf("Process with pid: %d is killed\n", kill_pid);
                }
                else
                {
                    printf("ERROR - Process with pid: %d is not killed\n", kill_pid);
                }
            }
            continue;
        }

        // for history command
        if (strcmp(argv[0], "history") == 0)
        {
            FILE *file;
            char chr;
            file = fopen("history.txt", "r");
            if (file == NULL)
            {
                printf("ERROR - File not found\n");
            }
            else
            {
                do
                {
                    printf("%c", chr);
                } while ((chr = fgetc(file)) != EOF);
                fclose(file);
            }
        }

        // Fork process
        pid = fork();
        if (pid == -1)
        {
            printf("Error - Forking failed.\n");
            exit(EXIT_FAILURE);
        }
        // child process
        else if (pid == 0)
        {
            sigaction(SIGINT, &child_handler, NULL);
            IO_redirection(argc, argv);
        }
        // parent process
        else
        {
            if (wait(&status) == -1)
            {
                printf("ERROR - Error in returning child procedure.\n");
            }
            else if (WIFSIGNALED(status))
            {
                printf("ERROR - child procedure is exited by signal %d\n", WTERMSIG(status));
            }
            else if (WIFSTOPPED(status))
            {
                printf("ERROR -  %d signal stopped child procedure\n", WIFSTOPPED(status));
            }
        }
        // Background process
        if (background == 0)
        {
            waitpid(pid, NULL, 0);
        }
        else
        {
            printf("A Process is created with pid: %d\n", pid);
            job_list[counter] = pid;
            counter++;
        }
    }
}

//---------------------------------------FUNCTIONS---------------------------------------------

// Initializing the shell
// Referenced from: http://www.gnu.org/software/libc/manual/html_node/Initializing-the-Shell.html
void shell_initialization()
{
    static pid_t shell_pid;
    static pid_t shell_pgid;
    static char *curr_directory;
    static int shell_is_interactive;
    static struct termios shell_TMODES;
    struct sigaction act_int;
    struct sigaction act_child;
    shell_pid = getpid();

    if (isatty(STDIN_FILENO) == 0)
    {
        printf("Zaidi shell cannot be made interactive.\n");
        exit(EXIT_FAILURE);
    }
    else
    {
        while (tcgetpgrp(STDIN_FILENO) != (shell_pgid = getpgrp()))
        {
            kill(shell_pid, SIGTTIN);
        }

        // Setting the signal handlers for sigchild and sigint
        act_child.sa_handler = child_signal_handler;
        act_int.sa_handler = int_signal_handler;
        sigaction(SIGCHLD, &act_child, 0);
        sigaction(SIGINT, &act_int, 0);

        // Putting shell in the shell's process group
        setpgid(shell_pid, shell_pid);
        shell_pgid = getpgrp();
        if (shell_pid != shell_pgid)
        {
            printf("The process group leader is not Zaidi Shell.\n");
            exit(EXIT_FAILURE);
        }
        // Grab control of the terminal
        tcsetpgrp(STDIN_FILENO, shell_pgid);
        tcgetattr(STDIN_FILENO, &shell_TMODES);
        curr_directory = (char *)calloc(1024, sizeof(char));
    }
}

// Signal handler for SIGINT
void int_signal_handler(int args)
{
    if (kill(pid, SIGTERM) != 0)
    {
        printf("\n");
    }
    else
    {
        printf("process got a sigint signal with pid: %d\n", pid);
        prompt_count = 1;
    }
}

// Signal handler for SIGCHLD
void child_signal_handler(int args)
{
    while (waitpid(-1, NULL, WNOHANG) > 0)
    {
    }
    printf("\n");
}

// signal handler
static void handling_signals(int signum)
{
    if (pid == 0)
    {
        exit(0);
    }
}

// Parsing the command line
// Referenced from: https://www.geeksforgeeks.org/tokenizing-a-string-cpp/
int parsing(char *line, char **argo)
{
    int i = 0;
    char *tokens = " \n\t";
    argo[i] = strtok(line, tokens);
    while ((i + 1 < MAXARGS) && (argo[i] != NULL))
    {
        argo[++i] = strtok((char *)0, tokens);
    }
    return i;
}

// IO redirection & Pipelining
void IO_redirection(int c_arguement, char *v_arguement[])
{
    int point = c_arguement;
    int file_open = O_CREAT | O_WRONLY | O_TRUNC;
    int file_parameters = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
    char inp, out = 0;
    int file_desc_out = fileno(stdout);
    int file_desc_inp = fileno(stdin);

    do
    {
        point = point - 1;

        if (strcmp(v_arguement[point], "<") == 0 || strcmp(v_arguement[point], ">") == 0)
        {
            if ((point + 1 > c_arguement) || (point + 1 == c_arguement))
            {
                printf("Error - The specification of redirection file has not been given\n");
                exit(EXIT_FAILURE);
            }
            if (point == 0)
            {
                exit(EXIT_FAILURE);
            }
        }

        if (strcmp(v_arguement[point], ">") == 0)
        {
            if (out)
            {
                printf("ERROR - Two output cannot be redirected on the same line.\n");
                exit(EXIT_FAILURE);
            }

            int current_out = open(v_arguement[point + 1], file_open, file_parameters);
            if (current_out != -1)
            {
                file_desc_out = dup(current_out);
                close(current_out);
                v_arguement[point] = NULL;
                c_arguement = point;
            }
            else
            {
                exit(EXIT_FAILURE);
            }
            out = 1;
        }
        else if (strcmp(v_arguement[point], "<") == 0)
        {
            if (inp)
            {
                exit(EXIT_FAILURE);
            }

            int current_inp = open(v_arguement[point + 1], O_RDONLY);
            if (current_inp != -1)
            {
                file_desc_inp = dup(current_inp);
                close(current_inp);
                v_arguement[point] = NULL;
                c_arguement = point;
            }
            else
            {
                printf("Error - Input is redirected in non-exisiting file.\n");
                exit(EXIT_FAILURE);
            }
            inp = 1;
        }
    } while (point > 0);

    // Pipe implementation
    int pipe_counter, pipe_position = 0;
    char **options[MAXARGS];
    int pipe_file_desc[MAXARGS - 1];
    memset(&options[0], 0, sizeof(options));

    for (point = 0; point < c_arguement; point++)
    {
        if (strcmp(v_arguement[point], "|") == 0)
        {
            options[pipe_counter] = v_arguement + pipe_position;
            options[pipe_counter][point - pipe_position] = NULL;
            pipe_counter++;
            pipe_position = point + 1;
        }
    }
    options[pipe_counter] = v_arguement + pipe_position;

    int i = 0;
    int p[2];
    pid_t pid;
    do
    {
        pipe(p);
        if ((pid = fork()) == -1)
        {
            printf("ERROR - Forking failed for pipe.\n");
            exit(EXIT_FAILURE);
        }
        else if (pid == 0)
        {
            dup2(file_desc_inp, 0);
            if (options[i + 1])
            {
                dup2(p[1], 1);
            }
            else
            {
                dup2(file_desc_out, 1);
            }
            close(p[0]);
            execvp(options[i][0], options[i]);
            exit(EXIT_FAILURE);
        }
        else
        {
            wait(NULL);
            close(p[1]);
            file_desc_inp = p[0];
            i++;
        }
    } while (options[i]);
    exit(EXIT_SUCCESS);
}

// Printing Jobs
void jobs()
{
    for (int i = 0; i < counter; i++)
    {
        printf("job %d: %d\n", i + 1, job_list[i]);
    }
}

// Printing help description
void help_description()
{
    printf("\ncd -> sets the path name as working directory.\n");
    printf("jobs -> provides a list of all background processes and their local pid.\n");
    printf("clear -> clears the shell.\n");
    printf("history -> prints a list of previously executed commands.\n");
    printf("kill PID -> terminates the background process identified locally with PID in the jobs list.\n");
    printf("!CMD -> runs the command numbered CMD in the command history.\n");
    printf("exit -> terminates the shell only if there are no background jobs.\n");
    printf("help -> prints the list of builtin commands along with their description.\n\n");
}
