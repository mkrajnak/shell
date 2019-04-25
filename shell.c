#define _POSIX_SOURCE
#include <ctype.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define BUFF_LEN 513
#define CMD_MAX 128 // Not expecting more than 128 tokens per command


char buffer[BUFF_LEN];
char *cmd[CMD_MAX];
pthread_mutex_t mutex;
pthread_cond_t await;
int executing = 0;
int running = 1;


void err(int code, char* err){
  if (code) {
    fprintf(stderr, "Err: %s, code:%d\n", err, code);
    exit(EXIT_FAILURE);
  }
}


void SIGINT_handler(){
  printf("Hanling SIGINT\n");
  exit(EXIT_SUCCESS);
}


void SIGCHLD_handler(){
  printf("Hanling SIGCHLD\n");
}


void cmd_init() {
  for (size_t i = 0; i < CMD_MAX; i++) {
    cmd[i] = (char *)malloc(CMD_MAX);
    if (&cmd[i] == NULL) {
      err(-1, "malloc");
    }
    memset(cmd[i], 0, CMD_MAX);
  }
}


void cmd_deinit() {
  for (size_t i = 0; i < CMD_MAX; i++) {
    free(cmd[i]);
  }
}


void tokenize() {
  int tokens = 0;
  for (size_t i = 0; i < BUFF_LEN; i++) {
    if (buffer[i] == '\0') {
      break;
    } else if (isspace(buffer[i])) {
      continue;
    } else {
      int start = i;
      while (!isspace(buffer[i]) && buffer[i]!='\0' && i < BUFF_LEN) {
        ++i;
      }
      strncpy(cmd[tokens], &buffer[start], i-start);
      printf("T %2d: %s\n", tokens, cmd[tokens]);
      printf("%ld\n", strlen(cmd[tokens]) );
      ++tokens;
    }
  }
  cmd[tokens] = NULL;  // ! Required by execvp function
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
    cmd_init();
    tokenize();
    pid_t pid;
    if ((pid = fork()) < 0) {
      err(pid, "fork");
    }

    if (pid > 0) {  //parent
      waitpid(pid, NULL, 0);
    }

    if (pid == 0) { //child
  		int ex = execvp(cmd[0], cmd);
  		err(ex, "execvp");
	   }

    executing = 0;
    pthread_cond_broadcast(&await);
    pthread_mutex_unlock(&mutex);
    cmd_deinit();
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
      fprintf(stderr, "Eror ! Input way too long\n");
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
