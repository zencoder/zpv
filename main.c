/*
Copyright 2013 Brightcove, Inc.

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
#include <signal.h>
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

double initial_start_time;
double read_wait_time = 0.0;
double write_wait_time = 0.0;
long long unsigned int total_bytes_written = 0;
double realtime_clock_offset = 0.0;

double timespec_to_double(struct timespec *the_time) {
  double decimal_time = (double)(the_time->tv_sec);
  decimal_time += (double)(the_time->tv_nsec) / 1e9;
  return decimal_time;
}

double get_current_time() {
  struct timespec now;
  clock_gettime(XCLOCK_MONOTONIC, &now);
  return timespec_to_double(&now) + realtime_clock_offset;
}

void set_realtime_clock_offset() {
  struct timespec now;
  clock_gettime(CLOCK_REALTIME, &now);
  double realtime_decimal = timespec_to_double(&now);

  // Technically there's a tiny delay between the clock calls, but it'll be close
  // enough for anything we care about.
  double now_decimal = get_current_time();

  if (realtime_decimal > now_decimal) {
    realtime_clock_offset = realtime_decimal - now_decimal;
  }
}

// NOTE: Buffer must be at least 32 chars!
void format_time(const double the_time, char* buffer) {
  time_t truncated_time = (time_t)the_time;
  // First the general formatting
  strftime(buffer, 32, "%Y-%m-%d %H:%M:%S", gmtime(&truncated_time));
  // Then add microseconds
  truncated_time = (the_time - truncated_time) * 1000;
  sprintf(buffer+19, ".%03d", (int)truncated_time);
}

void print_version() {
  fprintf(stderr, "{ \"zpv_version\": \"%s\" }\n", VERSION);
}

void print_message(char* message) {
  char formatted_time[32];
  format_time(get_current_time(), formatted_time);
  fprintf(stderr, "{ \"utc_time\": \"%s\", \"msg\": \"%s\" }\n", formatted_time, message);
}

void quit_with_error(char* message) {
  char formatted_time[32];
  format_time(get_current_time(), formatted_time);
  fprintf(stderr, "{ \"utc_time\": \"%s\", \"exit_status\": \"Error\", \"msg\": \"%s\" }\n", formatted_time, message);
  exit(1);
}

void quit_with_success(char* message) {
  char formatted_time[32];
  format_time(get_current_time(), formatted_time);
  fprintf(stderr, "{ \"utc_time\": \"%s\", \"exit_status\": \"Success\", \"msg\": \"%s\" }\n", formatted_time, message);
  exit(0);
}

void print_stats() {
  char formatted_time[32];
  double cur_time = get_current_time();
  format_time(cur_time, formatted_time);
  long long unsigned int in_wait, out_wait, total_time;
  in_wait =    (long long unsigned int)(read_wait_time * 1000);
  out_wait =   (long long unsigned int)(write_wait_time * 1000);
  total_time = (long long unsigned int)((cur_time - initial_start_time) * 1000);
  fprintf(stderr, "{ \"utc_time\": \"%s\", \"stdin_wait_ms\": %llu, \"stdout_wait_ms\": %llu, \"total_time_ms\": %llu, \"bytes_out\": %llu }\n", formatted_time, in_wait, out_wait, total_time, total_bytes_written);
}

void wait_for_reading() {
  fd_set fds;
  struct timeval tv = { 1, 0 };
  double before, elapsed;

  FD_ZERO(&fds);
  FD_SET(0, &fds);

  before = get_current_time();
  while (select(1, &fds, NULL, NULL, &tv) <= 0) {
    print_message("Stalled reading from stdin");
    tv.tv_sec = 1;
    tv.tv_usec = 0;
  }
  elapsed = get_current_time() - before;
  read_wait_time += elapsed;
}

void wait_for_writing() {
  fd_set fds;
  struct timeval tv = { 1, 0 };
  double before, elapsed;

  FD_ZERO(&fds);
  FD_SET(1, &fds);

  before = get_current_time();
  while (select(2, NULL, &fds, NULL, &tv) <= 0) {
    print_message("Stalled writing to stdout");
    tv.tv_sec = 1;
    tv.tv_usec = 0;
  }
  elapsed = get_current_time() - before;
  write_wait_time += elapsed;
}

static void sigint_callback(int signal_number) {
  quit_with_success("Received SIGINT");
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

  set_realtime_clock_offset();
  initial_start_time = get_current_time();
  print_version();
  print_stats();

  // Start trapping interrupts!
  signal(SIGINT, sigint_callback);

  before = initial_start_time;
  while (1) {
    wait_for_reading();
    bytes_read = read(0, buf, BUFFER_SIZE);
    if (bytes_read <= 0) break; // When we can't read anymore, quit.

    wait_for_writing();
    result = write(1, buf, bytes_read);
    if (result != bytes_read) quit_with_error("Error writing to stdout");
    total_bytes_written += result;

    // Print stats every 1 seconds.
    after = get_current_time();
    elapsed = after - before;
    if (elapsed > 1.0) {
      print_stats();
      before = after;
    }
  }

  print_stats();
  quit_with_success("End of file reached");

  return 0;
}
