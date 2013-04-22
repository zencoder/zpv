/*
Copyright 2012 Brightcove, Inc.

    Author:
    mszatmary@brightcove.com

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

#define VERSION "0.3"

////////////////////////////////////////////////////////////////////////////////
// #define EV_USE_EVENTFD 1
// #define EV_USE_SELECT  1
// #define EV_USE_POLL    1
// #define EV_USE_EPOLL   1 // Linux Only
// #define EV_USE_KQUEUE  1 // BSD/OSX Only
#define EV_NO_THREADS 1

#define EV_STANDALONE  1
#include "ev.c"
////////////////////////////////////////////////////////////////////////////////
#define BUFFER_SIZE PIPE_BUF
struct ev_loop *loop;
char data[BUFFER_SIZE+1];
int data_size;

#define READING 0
#define WRITING 1
int mode;

typedef struct
{
    ev_io watcher;
    ev_tstamp timer_start;
    ev_tstamp time_waiting;
} pipe_t;

pipe_t stdin_pipe;
pipe_t stdout_pipe;
ev_tstamp start_time;
ev_timer timer;
ev_signal exitsig;
int64_t bytes_out;

ev_tstamp posix_time;
char formatted_time[32];

static void print_version()
{
    fprintf(stderr, "{ \"zpv_version\": \"%s\" }\n", VERSION);
}

static void update_times()
{
    posix_time = ev_time();
    time_t truncated_time = posix_time;
    strftime(formatted_time, 32, "%Y-%m-%d %H:%M:%S", gmtime(&truncated_time));

    // Add microseconds
    truncated_time = (posix_time - truncated_time) * 100000;
    sprintf(formatted_time+19, ".%06d", (int)truncated_time);
}

static void print_timer()
{
    ev_tstamp total_time = posix_time - start_time;
    if ( 0 >= total_time ) return;
    fprintf(stderr, "{ \"utc_time\": \"%s\", \"stdin_wait_ms\": %d, \"stdout_wait_ms\": %d, \"total_time_ms\": %d, \"bytes_out\": %lld }\n", formatted_time,
        (int)(1000 * ( stdin_pipe.time_waiting  + ( mode != READING || 0 > stdin_pipe.timer_start  ? 0 : posix_time - stdin_pipe.timer_start ) ) ),
        (int)(1000 * ( stdout_pipe.time_waiting + ( mode != WRITING || 0 > stdout_pipe.timer_start ? 0 : posix_time - stdout_pipe.timer_start) ) ),
        (int)(1000 * total_time), bytes_out );
}

static void stdout_callback (EV_P_ ev_io *w, int revents);
static void stdin_callback (EV_P_ ev_io *w, int revents)
{
    if ( 0 == data_size )
    {
        if ( 0 >= ( data_size = read( STDIN_FILENO, &data[ 0 ], BUFFER_SIZE ) ) )
        {
            update_times();
            print_timer();
            if ( 0 == data_size )
                fprintf(stderr, "{ \"utc_time\": \"%s\", \"exit_status\": \"Success\", \"msg\": \"End of file reached\" }\n", formatted_time);
            else
                fprintf(stderr, "{ \"utc_time\": \"%s\", \"exit_status\": \"Error\",  \"msg\": \"Error reading from stdin\", \"errno\": %d }\n", formatted_time, errno);

            exit(data_size);
        }
    }

    if( 0 < data_size )
    {
        // switch to write mode
        ev_tstamp now = ev_now( loop );

        mode = WRITING;
        stdout_pipe.timer_start = now;

        if( 0 < stdin_pipe.timer_start )
            stdin_pipe.time_waiting += now - stdin_pipe.timer_start;

        ev_io_init (&stdout_pipe.watcher, stdout_callback, STDOUT_FILENO, EV_WRITE);
        ev_io_start( loop, &stdout_pipe.watcher ); // start stdout
        ev_io_stop( loop, &stdin_pipe.watcher ); // stop stdin
    }
}

static void stdout_callback (EV_P_ ev_io *w, int revents)
{
    if ( 0 < data_size )
    {
        if ( data_size != write( STDOUT_FILENO, &data[ 0 ], data_size ) )
        {
            update_times();
            print_timer();
            fprintf(stderr, "{ \"utc_time\": \"%s\", \"exit_status\": \"Error\",  \"msg\": \"Error writing to stdout\" }\n", formatted_time);
            exit(data_size);
        }

        bytes_out += data_size;
        data_size = 0;
    }

    if ( 0 == data_size )
    {
        // Switch to read mode

        ev_tstamp now = ev_now( loop );
        mode = READING;

        stdin_pipe.timer_start = now;

        if ( 0 < stdout_pipe.timer_start )
            stdout_pipe.time_waiting += now - stdout_pipe.timer_start;

        ev_io_init (&stdin_pipe.watcher, stdin_callback, STDIN_FILENO, EV_READ);
        ev_io_start( loop, &stdin_pipe.watcher );
        ev_io_stop( loop, &stdout_pipe.watcher );
    }
}

static void timer_callback(struct ev_loop *loop, ev_timer *w, int revents)
{
    static int bytes = 0;
    update_times();
    if ( bytes > 0 && bytes >= bytes_out )
    {
        if( mode == READING )
            fprintf(stderr, "{ \"utc_time\": \"%s\", \"msg\": \"Stalled reading from stdin\" }\n", formatted_time);
        else
            fprintf(stderr, "{ \"utc_time\": \"%s\", \"msg\": \"Stalled writing to stdout\" }\n", formatted_time);
    }

    bytes = bytes_out;
    print_timer();
}

static void sigint_callback (struct ev_loop *loop, ev_signal *w, int revents)
{
    update_times();
    print_timer();
    fprintf(stderr, "{ \"utc_time\": \"%s\", \"exit_status\": \"Success\", \"msg\": \"Received SIGINT\" }\n", formatted_time );
    exit(0);
}

int main(int argc, char **argv)
{
    int flags;
    mode = READING;
    data_size = 0;
    bytes_out = 0;
    loop = ev_loop_new( EVBACKEND_SELECT );
    start_time = ev_time();
    stdout_pipe.time_waiting = 0;
    stdout_pipe.timer_start = -1;
    stdin_pipe.time_waiting = 0;
    stdin_pipe.timer_start = -1;

/*
    flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if ( fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK | flags ) == -1 )
        fprintf(stderr, "{ \"msg\": \"Could not set O_NONBLOCK on stdin\" }\n", data_size, errno);

    flags = fcntl(STDOUT_FILENO, F_GETFL, 0);
    if ( fcntl(STDOUT_FILENO, F_SETFL, O_NONBLOCK | flags ) == -1 )
        fprintf(stderr, "{ \"msg\": \"Could not set O_NONBLOCK on stdout\" }\n", data_size, errno);
*/

    ev_io_init (&stdout_pipe.watcher, stdout_callback, STDOUT_FILENO, EV_WRITE);
    ev_io_init (&stdin_pipe.watcher, stdin_callback, STDIN_FILENO, EV_READ);
    ev_io_start( loop, &stdin_pipe.watcher );

    ev_timer_init (&timer, timer_callback, 2.0, 2.0);
    ev_timer_start (loop, &timer);

    ev_signal_init (&exitsig, sigint_callback, SIGINT);
    ev_signal_start (loop, &exitsig);

    print_version();
    update_times();
    print_timer();

    ev_run (loop, 0);
}
