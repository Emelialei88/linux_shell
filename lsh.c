#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <setjmp.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#define LSH_RL_BUFSIZE 1024
#define LSH_TOK_BUFSIZE 64
#define LSH_TOK_DELIM " \t\r\n\a"

void lsh_loop(void);
char *lsh_read_line(void);
char **lsh_split_line(char *line);
int lsh_launch(char **args);
int lsh_execute(char **args);
void sigint_handler(int signo);
int input_redirect(char **args, int pos);
int output_redirect(char **args, int pos);
int pipeline(char **args, int pos);

static sigjmp_buf env;
static volatile sig_atomic_t jump_active = 0;

int main(int argc, char *argv[])
{
  // Load config files
  
  // Run command loop
  lsh_loop();
  
  // Perform any shutdown/cleanup

  return EXIT_SUCCESS;
}

void lsh_loop(void)
{
  char *line;
  char **args;
  int status;
  
  // Replace to increase protablility
  // Setup SIGINT
  struct sigaction s;
  s.sa_handler = sigint_handler;
  sigemptyset(&s.sa_mask);
  s.sa_flags = SA_RESTART;
  sigaction(SIGINT, &s, NULL);
  
  do {
    if(sigsetjmp(env, 1) == 77) {
    	// Restart loop
    	printf("\n");
    	continue;
    }   
    // Guarantee that a signal will only 
    // be delivered after the jump point has been set 
    jump_active = 1;
    
    printf(" > ");
    line = lsh_read_line();
    if(line == NULL) {
    	printf("\n");
    	exit(EXIT_SUCCESS);
    }
        
    args = lsh_split_line(line);
    status = lsh_execute(args);

    free(line);
    free(args);
  } while (status);
}

char *lsh_read_line(void) 
{    
    int bufsize = LSH_RL_BUFSIZE;
    int pos = 0;
    char *buf = malloc(sizeof(char) * bufsize);
    int c;

    if(!buf) {
        fprintf(stderr, "lsh: allocation error\n");
	exit(EXIT_FAILURE);
    }

    // Return NULL on entering Ctrl-D
    c = getchar();
    if(c == EOF) {
    	return NULL;
    }
    
    ungetc(c, stdin);
    while(1) {
    	c = getchar();
	if(c == '\n' || c == EOF) {
	    buf[pos] = '\0';
	    return buf;
	}
	else {
	    buf[pos] = c;
	}
	pos++;

	// If we exceeded the buffer, reallocate;
	if(pos >= bufsize) {
	    bufsize += LSH_RL_BUFSIZE;
	    buf = realloc(buf, bufsize);
	    if(!buf) {
	    	fprintf(stderr, "lsh: allocation error\n");
	        exit(EXIT_FAILURE);
	    }
	}
    }
}

char **lsh_split_line(char *line)
{
    int bufsize = LSH_TOK_BUFSIZE;
    int pos = 0;
    char **tokens = malloc(sizeof(char*) * bufsize);
    char *token;

    if(!tokens) {
        fprintf(stderr, "lsh: allocation error\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(line, LSH_TOK_DELIM);
    while(token != NULL) {
    	tokens[pos] = token;
	pos++;

	if(pos >= bufsize) {
	    bufsize += LSH_RL_BUFSIZE;
            tokens = realloc(tokens, bufsize);
	    if(!tokens) {
	        fprintf(stderr, "lsh: allocation error\n");
        	exit(EXIT_FAILURE);
	    }
	}

	token = strtok(NULL, LSH_TOK_DELIM);
    }
    tokens[pos] = NULL;
    return tokens;
}

/*Child process*/
int lsh_launch(char **args)
{
    pid_t pid;
    int status;
    pid = fork();
    
    if(pid == 0) {
    	// Child process
    	struct sigaction s_child;
	s_child.sa_handler = sigint_handler;
	sigemptyset(&s_child.sa_mask);
	s_child.sa_flags = SA_RESTART;
	sigaction(SIGINT, &s_child, NULL);
	
	// Execute Redirection and Piplining
	/* This part has to be put into lsh_launch func
	   If it is put in lsh_execute func, then 
	   lsh terminates after executing redirection
	   Reason: It is in the child process, so termination 
	   only results in returning to parent process
	*/
	for(int j=0; args[j] != NULL; j++) {
	    if(strcmp(args[j], "<") == 0) {
	    	// CMD > file
	        return input_redirect(args, j);
	    }
    	    if(strcmp(args[j], ">") == 0) {
    	    	// CMD < file
    	    	printf("Entering....");
    	        return output_redirect(args, j);
    	    }
    	    if(strcmp(args[j], "|") == 0) {
    	    	// CMD1 | CMD2
    	    	printf("Entering....");
    	        return pipeline(args, j); 
    	    }
	}
    	
    	// Execute normal commands
    	if(execvp(args[0], args) < 0) {
    	    perror("lsh");
    	    exit(EXIT_FAILURE);
    	} 
    	sleep(10);
    	
    } else if(pid < 0) {
    	// Error forking
    	perror("lsh");
    } else {
    	// Parent process	
    	do {
	    waitpid(pid, &status, WUNTRACED);    
	} while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }
    
    return 1;
}


/*Built-in process*/

/*
  Function Declarations for builtin shell commands:
 */
int lsh_cd(char **args);
int lsh_help(char **args);
int lsh_exit(char **args);

char *builtin_str[] = {
    "cd",
    "help",
    "exit"
};

int (*builtin_func[]) (char **) = {
    &lsh_cd,
    &lsh_help,
    &lsh_exit,
};

int lsh_num_builtins() {
    return sizeof(builtin_str) / sizeof(char *);
}

int lsh_cd(char **args) {
    if(args[1] == NULL) {
    	fprintf(stderr, "lsh: expected argument to \"cd\"\n");
    } else {
    	if(chdir(args[1]) != 0) {
    	    perror("lsh");
    	}
    }
    return 1;
}

int lsh_help(char **args) {
    int i;
    printf("Olivia's LSH\n");
    printf("Type program names and arguments, and hit enter.\n");
    printf("The following are built in:\n");
    
    for(i=0; i<lsh_num_builtins(); i++) {
    	printf("  %s\n", builtin_str[i]);
    }
    
    printf("Use the man command for information on other programs.\n");
    return 1;
}

int lsh_exit(char **args) {
    return EXIT_SUCCESS;
}

int lsh_execute(char **args) 
{
    int i;
    
    if(args[0] == NULL) {
    	// Entered an empty cmd
    	return 1;
    }
       
    // Execute built in commands
    for(i=0; i<lsh_num_builtins(); i++) {
    	if(strcmp(args[0], builtin_str[i]) == 0) {
    	    return (*builtin_func[i])(args);
    	}
    }
    
    // Execute command line commands
    return lsh_launch(args);
}

void sigint_handler(int signo) {
    if(!jump_active) {
    	return;
    } 
    siglongjmp(env, 77);
    //printf("SIGTTIN detected\n");
}

int input_redirect(char **args, int pos) {
    char *filename = args[pos+1];
    args[pos] = NULL;
    
    // Redirect input from file fd to stdin (fd=0)
    int fdin = open(filename, O_RDONLY);
    if(fdin == -1) {
    	perror("open");
    	exit(EXIT_FAILURE);
    } 
    if(dup2(fdin, STDIN_FILENO) == -1) {
    	perror("dup2");
    	exit(EXIT_FAILURE);
    }
    if(close(fdin) == -1) {
    	perror("close");
    	exit(EXIT_FAILURE);
    }
    
    // Execute commands
    if (execvp(args[0], args) < 0) {
    	perror("execvp");
    	exit(EXIT_FAILURE);
    }
    
    return 1;
}

int output_redirect(char **args, int pos) {
    char *filename = args[pos+1];
    args[pos] = NULL;
    
    // Redirect input from file fd to stdin (fd=0)
    int fdout = open(filename, O_WRONLY | O_CREAT | O_TRUNC,
                     S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if(fdout == -1) {
    	perror("out");
    	exit(EXIT_FAILURE);
    } 
    if(dup2(fdout, STDOUT_FILENO) == -1) {  // Redirect fdout to 1
    	perror("dup2");
    	exit(EXIT_FAILURE);
    }
    if(close(fdout) == -1) {	// Close the original fd
    	perror("close");
    	exit(EXIT_FAILURE);
    }
    
    // Execute commands
    if (execvp(args[0], args) < 0) {
    	perror("execvp");
    	exit(EXIT_FAILURE);
    }
    
    return 1;
}

int pipeline(char **args, int pos) {
    // Save commands
    char **args1 = &args[0];
    char **args2 = &args[pos+1];
    args[pos] = NULL;
    
    // Create pipe
    int pipefd[2]; // 0 - read; 1 - write
    int pid1, pid2;
    if(pipe(pipefd) == -1) {
    	perror("pipe");
    	exit(EXIT_FAILURE);
    }
    
    // Create child process, exe CMD1, write
    if((pid1 = fork()) == -1) {
    	perror("fork");
    	exit(EXIT_FAILURE);
    } 
    if(pid1 == 0) {
    	// Child process
    	// Close the read end of the pipe
    	if(close(pipefd[0]) == -1) {
    	    perror("close");
    	    exit(EXIT_FAILURE);
        }
        // Redirect stdout to the write end
    	if(pipefd[1] != STDOUT_FILENO) {
    	    if(dup2(pipefd[1], STDOUT_FILENO) == -1) { 
    	        perror("dup2");
    	        exit(EXIT_FAILURE);
    	    }
    	    // Close the orignal fd
    	    if(close(pipefd[1]) == -1) {
    	        perror("close");
    	        exit(EXIT_FAILURE);
            }
    	}
    	// Execute CMD1
    	execvp(args1[0], args1); 
    	perror("execvp");
    	exit(EXIT_FAILURE);
    }
    
    	// In parent process, exe CMD2, read
    	if(close(pipefd[1]) == -1) {
    	    perror("close");
    	    exit(EXIT_FAILURE);
        }
        // Redirect stdout to the write end
    	if(pipefd[0] != STDIN_FILENO) {
    	    if(dup2(pipefd[0], STDIN_FILENO) == -1) { 
    	        perror("dup2");
    	        exit(EXIT_FAILURE);
    	    }
    	    // Close the orignal fd
    	    if(close(pipefd[0]) == -1) {
    	        perror("close");
    	        exit(EXIT_FAILURE);
            }
    	}
    	// Execute CMD2
    	execvp(args2[0], args2); 
    	perror("execvp");
    	exit(EXIT_FAILURE);    
    
      
    // Wait for 2 child process
    if (wait(NULL) == -1) {
        perror("wait");
        exit(EXIT_FAILURE);
    }
    if (wait(NULL) == -1) {
        perror("wait");
        exit(EXIT_FAILURE);
    }
    
    return 1;
}
