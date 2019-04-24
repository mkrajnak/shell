#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>


void help(){
  printf("Usage:\n./program <N> <M>\n");
  printf("<N> - integer number of threads\n" );
  printf("<M> - integer number of allowed passes of the critical section\n");
  exit(EXIT_FAILURE);
}


void err(int code, char* err){
  if (code) {
    fprintf(stderr, "Err: %s, code:%d\n", err, code );
    exit(EXIT_FAILURE);
  }
}


int main(int argc, char** argv) {
  printf("%d\n", argc);
  return 0;
}
