/*
Copyright 2012 Brightcove, Inc.

    Author:
    jgreer@brightcove.com

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <time.h>
#include <errno.h>

#include "mac_support.h"

#define VERSION "0.5"
#define BUFFER_SIZE 0x10000 // 64k

// Not all systems have a coarse/fast clock, so allow fallback.
#if defined CLOCK_MONOTONIC_COARSE
#define XCLOCK_MONOTONIC CLOCK_MONOTONIC_COARSE
#else
#define XCLOCK_MONOTONIC CLOCK_MONOTONIC
#endif

char *current_state;
double initial_start_time;
double read_wait_time = 0.0;
double write_wait_time = 0.0;

double get_current_time() {
  struct timespec now;
  clock_gettime(XCLOCK_MONOTONIC, &now);
  double decimal_time = (double)(now.tv_sec);
  decimal_time += (double)(now.tv_nsec) / 1e9;
  return decimal_time;
}

// NOTE: Buffer must be at least 32 chars!
void format_time(const double the_time, char* buffer) {
  time_t truncated_time = (time_t)the_time;
  // First the general formatting
  strftime(buffer, 32, "%Y-%m-%d %H:%M:%S", gmtime(&truncated_time));
  // Then add microseconds
  truncated_time = (the_time - truncated_time) * 100000;
  sprintf(buffer+19, ".%06d", (int)truncated_time);
}

static void print_version() {
  fprintf(stderr, "{ \"zpv_version\": \"%s\" }\n", VERSION);
}

void print_stats() {
  char formatted_time[32];
  double cur_time = get_current_time();
  format_time(cur_time, formatted_time);
  fprintf(stderr, "%s - %s - Read wait: %0.6f, Write wait: %0.6f, Elapsed: %0.6f\n", formatted_time, current_state, read_wait_time, write_wait_time, cur_time - initial_start_time);
}

void wait_for_reading() {
  static fd_set fds;
  struct timeval tv = { 1, 0 };
  double before, elapsed;

  FD_ZERO(&fds);
  FD_SET(0, &fds);

  before = get_current_time();
  while (select(1, &fds, NULL, NULL, &tv) <= 0) print_stats();
  elapsed = get_current_time() - before;
  read_wait_time += elapsed;
}

void wait_for_writing() {
  static fd_set fds;
  struct timeval tv = { 1, 0 };
  double before, elapsed;

  FD_ZERO(&fds);
  FD_SET(1, &fds);

  before = get_current_time();
  while (select(2, NULL, &fds, NULL, &tv) <= 0) print_stats();
  elapsed = get_current_time() - before;
  write_wait_time += elapsed;
}

int main(int argc,char* argv[]){

  if (argc > 1 && strcmp(argv[1],"-h") == 0) {
    fprintf(stderr, "Usage: prog1 | zpv2 [-u unique_string] 2> zpv2.log | prog2\n");
    fprintf(stderr, "  The unique string is ignored.  Just for finding in 'ps' output.\n");
    fprintf(stderr, "  Timing info is written to stderr.\n");
    exit(1);
  }

  char buf[BUFFER_SIZE];
  int bytes_read;
  int result;
  double before, after, elapsed;

  initial_start_time = get_current_time();
  current_state = "start   ";
  print_stats();
  current_state = "running ";

  before = initial_start_time;
  while (1) {

    wait_for_reading();
    bytes_read = read(0, buf, BUFFER_SIZE);
    // fprintf(stderr, "Bytes read: %d\n", bytes_read);

    if (bytes_read <= 0) break; // When we can't read anymore, quit.

    wait_for_writing();
    result = write(1, buf, bytes_read);
    if (result < 1) break; // If the program we're writing to dies, then make sure we quit.

    // Print stats every 1 seconds.
    after = get_current_time();
    elapsed = after - before;
    if (elapsed > 1.0) {
      print_stats();
      before = after;
    }
  }

  current_state = "finished";
  print_stats();

  // fprintf(stderr, "SUCCESS!\n");
  return 0;
}
