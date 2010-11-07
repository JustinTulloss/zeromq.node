/*
 * Stress test client.
 * See stress-test-server.js for commentary.
 *
 * On Linux (maybe other OSes as well?), you can compile this with
 *  gcc stress-test-client.c -lzmq -lpthread -o stress-test-client
 */

#define _GNU_SOURCE

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zmq.h>

#define THREADS 10
#define INFINITY 1
#define SERVER "tcp://localhost:23456"

typedef struct function_args {
  void* context;
  int i;
} function_args_t;

void my_free(void* data, void* hint) {
  free(data);
}

void* worker_function(void* params) {
  function_args_t* args = (function_args_t*)params;
  void* socket = zmq_socket(args->context, ZMQ_REQ);
  char* message;
  zmq_msg_t request, reply;
  size_t request_size, reply_size;
  char* reply_buf;

  zmq_connect(socket, SERVER);

  request_size = asprintf(&message, "hello! %i", args->i);
  zmq_msg_init_data(&request, message, request_size, my_free, NULL);
  zmq_send(socket, &request, 0);
  zmq_msg_close(&request);

  zmq_msg_init(&reply);
  zmq_recv(socket, &reply, 0);

  /* make a copy of the reply, but NUL pad it */
  reply_size = zmq_msg_size(&reply);
  reply_buf = (char*)malloc(reply_size + 1);
  memcpy((void*)reply_buf, zmq_msg_data(&reply), reply_size);
  reply_buf[reply_size] = '\0';
  zmq_msg_close(&reply);

  printf("thread %i got reply %s\n", args->i, reply_buf);
  free(reply_buf);
  zmq_close(socket);
}

int main(int argc, char** argv) {
  void* context = zmq_init(1);
  pthread_t threads[THREADS];
  function_args_t args[THREADS];
  int i;

  for (i = 0; i < THREADS; ++i) {
    args[i].context = context;
    args[i].i = i + 1;
  }

  do {
    for (i = 0; i < THREADS; ++i) {
      pthread_create(&(threads[i]), NULL, worker_function, &(args[i]));
    }

    for (i = 0; i < THREADS; ++i) {
      pthread_join(threads[i], NULL);
    }
  } while (INFINITY);
}
