#define _POSIX_SOURCE
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <pthread.h>


#define BUFF_LEN 513
#define CMD_MAX 128 // Not expecting more than 128 tokens per command

char buffer[BUFF_LEN];

pthread_mutex_t mutex;
pthread_cond_t await;

int executing = 0;
int running = 1;
int background = 0;
char *in_file_name;
char *out_file_name;


void err(int code, char* err){
  if (code) {
    fprintf(stderr, "Err: %s, code:%d\n", err, code);
    exit(EXIT_FAILURE);
  }
}


void SIGINT_handler(){
  printf("\n");
}


// Handler inspired by "man 2 waitpid"
void SIGCHLD_handler(){
  int status;
  pid_t pid;
  while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
    printf("\n$ Process %d ", pid);
    if (WIFEXITED(status)) {
      printf("exited, status=%d", WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
      printf("killed by signal %d", WTERMSIG(status));
    } else if (WIFSTOPPED(status)) {
      printf("stopped by signal %d", WSTOPSIG(status));
    }
    printf("\n$ ");
    fflush(stdout);
  }
}


int jump_to_next(int i){
  while (!isspace(buffer[i]) && buffer[i]!='\0' && i < BUFF_LEN &&
    buffer[i] != '>' && buffer[i] != '<') {
      ++i;
  }
  return i;
}


void tokenize(char **cmd) {
  int tokens = 0;
  for (size_t i = 0; i < BUFF_LEN; i++) {
    if (buffer[i] == '\0') {
      break;
    } else if (isspace(buffer[i])) {
      continue;
    } else {
      if (strncmp(&buffer[i], "&", 1) == 0) { // &
        background = 1;
        buffer[i] = '\0';
        ++i;
      } else if (strncmp(&buffer[i], ">", 1) == 0) {  // >
        if (out_file_name) {  // filename already set
          fprintf(stderr, "Multiple redirects are not supported\n");
          return;
        }
        buffer[i] = '\0';
        ++i;
        while(isspace(buffer[i]))
          ++i;
        if (buffer[i] == '\0') { // Last argument is missing
          fprintf(stderr, "Missing filename\n");
          return;
        }
        out_file_name = &buffer[i];
        i = jump_to_next(i);
        buffer[i] = '\0';
      } else if (strncmp(&buffer[i], "<", 1) == 0) {  // <
        if (in_file_name) { // filename already set
          fprintf(stderr, "Multiple redirects are not supported\n");
          return;
        }
        buffer[i] = '\0';
        ++i;
        while(isspace(buffer[i]))
          ++i;
        if (buffer[i] == '\0') { // Last argument is missing
          fprintf(stderr, "Missing filename\n");
          return;
        }
        in_file_name = &buffer[i];
        i = jump_to_next(i);
        buffer[i] = '\0';
      } else {              // command
        cmd[tokens] = &buffer[i];
        ++tokens;
        i = jump_to_next(i);
        if (buffer[i] == '<' || buffer[i] == '>') { // >< without spaces
          printf("lel\n");
          --i;
        } else {
          buffer[i] = '\0';
        }
      }
    }
  }
  // printf("IN:%s\n", in_file_name);
  // printf("OUT:%s\n", out_file_name);
  // for (int i = 0; i < tokens; i++) {
  //   printf("%d: %s\n",i,cmd[i]);
  // }
  cmd[tokens] = NULL;  // ! Required by execvp function
}


void execute(char **cmd) {
  pid_t pid;
  if ((pid = fork()) < 0) {
    err(pid, "fork");
  }

  if (pid > 0 && !background) {  //parent
    waitpid(pid, NULL, 0);
  } else if (pid == 0) {          //child
    int f;
    int d;
    if(out_file_name){
      if ((f = open(out_file_name, O_WRONLY | O_CREAT | O_TRUNC, 0666)) < 0) {
        err(f, "open");
      }
      if ((d = dup2(f, fileno(stdout))) < 0) {
        err(d, "dup2");
      }
      close(f);
		}
    if(in_file_name){
      if ((f = open(in_file_name, O_RDONLY)) < 0) {
        err(f, "open");
      }
      if ((d = dup2(f, fileno(stdin))) < 0) {
        err(d, "dup2");
      }
      close(f);
		}
    int ex = execvp(cmd[0], cmd);
    err(ex, "execvp");
  }
}


void *exec_thread(){
  while (running) {
    pthread_mutex_lock(&mutex);
    while (!executing) {  // Wait for input thread to get a cmd
      pthread_cond_wait(&await, &mutex);
    }
    if (!running) { // exit cmd has been executed
      pthread_cond_broadcast(&await);
      pthread_mutex_unlock(&mutex);
      return NULL;
    }
    background = 0;
    in_file_name = out_file_name = NULL;
    char *cmd[CMD_MAX];
    tokenize(cmd);
    execute(cmd);
    executing = 0;
    pthread_cond_broadcast(&await);
    pthread_mutex_unlock(&mutex);
  }
  return NULL;
}


void *input_thread(){
  int cmd_len = 0;

  while (running) {
    memset(&buffer, 0, BUFF_LEN);
    printf("$ ");
    fflush(stdout);
    cmd_len = read(0, &buffer, BUFF_LEN);

    if (cmd_len == 513) {   // Invalid input
      fprintf(stderr, "Error ! Input longer than 512B\n");
      while(getchar() == '\n');
    } else if (strncmp(buffer, "exit\n", strlen("exit\n")) == 0) {
      pthread_mutex_lock(&mutex);
      executing = 1;                  // Unleash the exec thread
      running = 0;
      pthread_cond_broadcast(&await);
      pthread_mutex_unlock(&mutex);
    } else if (strncmp(buffer, "\n", strlen("\n")) == 0){ // Just pressed enter
      continue;
    } else {  // Valid cmd, proceed to parsing in another thread
      pthread_mutex_lock(&mutex);
      executing = 1;                  // Unleash the exec thread
      pthread_cond_broadcast(&await);
      pthread_mutex_unlock(&mutex);
      // Wait for cmd execution
      pthread_mutex_lock(&mutex);
      while(executing){
        pthread_cond_wait(&await, &mutex);
  		}
      pthread_cond_broadcast(&await);
      pthread_mutex_unlock(&mutex);
    }
  }
  return NULL;
}


int main() {
  int res = 0;
  pthread_t execute, input;
  struct sigaction sa_chld, sa_int;

  // Signal handlers Initialization
  sa_chld.sa_handler = SIGCHLD_handler;
  sa_int.sa_handler = SIGINT_handler;
  sigemptyset(&sa_chld.sa_mask);
  sigemptyset(&sa_int.sa_mask);
  sa_chld.sa_flags = sa_int.sa_flags = 0;

  res = sigaction(SIGCHLD, &sa_chld, NULL);
  err(res, "sigaction SIGCHLD");
  res = sigaction(SIGINT, &sa_int, NULL);
  err(res, "sigaction SIGINT");

  // mutex and cond setup
  res = pthread_mutex_init(&mutex, NULL);
  err(res, "pthread_mutex_init mutex");
  res = pthread_cond_init(&await, NULL);
  err(res, "pthread_cond_init await");

  // Create two threads, one to read input
  res = pthread_create(&input, NULL, &input_thread, NULL);
  err(res, "pthread_create input");
  // Thread to execute commands
  res = pthread_create(&execute, NULL, &exec_thread, NULL);
  err(res, "pthread_create execute");

  // Wait till the end
  res = pthread_join(input, NULL);
  err(res, "pthread_join input");
  res = pthread_join(execute, NULL);
  err(res, "pthread_join execute");

  // Cleanup
  res = pthread_cond_destroy(&await);
  err(res, "pthread_cond_destroy await");
  res = pthread_mutex_destroy(&mutex);
  err(res, "pthread_mutex_destroy mutex");

  return 0;
}
