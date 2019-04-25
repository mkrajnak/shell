#define _POSIX_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

pthread_mutex_t lock;
pthread_cond_t await;




void err(int code, char* err, int fail){
  if (code) {
    fprintf(stderr, "Err: %s, code:%d\n", err, code );
    if (fail) {
      exit(EXIT_FAILURE);
    }
  }
}


void SIGINT_handler(){
  printf("Hanling SIGINT\n");
  exit(EXIT_SUCCESS);
}


void SIGCHLD_handler(){
  printf("Hanling SIGCHLD\n");
}


void *exec_thread(){
  printf("Running exec thread\n");
  return NULL;
}


void *input_thread(){
  printf("Running input thread\n");
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
  err(res, "sigaction SIGCHLD", 1);
  res = sigaction(SIGINT, &sa_int, NULL);
  err(res, "sigaction SIGINT", 1);

  // Create two threads, one to read input
  res = pthread_create(&input, NULL, &input_thread, NULL);
  err(res, "pthread_create input", 1);
  // Thread to execute commands
  res = pthread_create(&execute, NULL, &exec_thread, NULL);
  err(res, "pthread_create executre", 1);

  // Wait till the end
  res = pthread_join(input, NULL);
  err(res, "pthread_join input", 1);
  res = pthread_join(execute, NULL);
  err(res, "pthread_join execute", 1);

  return 0;
}
