////////////////////////////////////////////////////////////////////////////////
// #define EV_USE_EVENTFD 1
// #define EV_USE_SELECT  1
// #define EV_USE_POLL    1
// #define EV_USE_EPOLL   1 // Linux Only
// #define EV_USE_KQUEUE  1 // BSD/OSX Only

#define EV_STANDALONE  1
#include "ev.c"
////////////////////////////////////////////////////////////////////////////////
#define BUFFER_SIZE (2*1024*1024)
struct ev_loop *loop;
char data[BUFFER_SIZE];
int data_size;

typedef struct
{
    ev_io watcher;
    double timer_start;
    double time_waiting;
} pipe_t;

pipe_t stdin_pipe;
pipe_t stdout_pipe;
ev_tstamp start_time;
int64_t bytes_out;

static void print_timer()
{
    ev_tstamp now = ev_now( loop );
    ev_tstamp total_time = now - start_time;
    if ( 0 == total_time ) return;
    fprintf(stderr, "{ stdin_wait_ms: %d, stdout_wait_ms: %d, total_time_ms %d, bytes_out: %lld }\n",
        (int)(1000*stdin_pipe.time_waiting), (int)(1000*stdout_pipe.time_waiting), (int)(1000*total_time), bytes_out );
}

static void stdin_callback (ev_io *watcher, int revents)
{
    if ( 0 == data_size )
    {
        if ( 0 >= ( data_size = read( STDIN_FILENO, &data[ 0 ], BUFFER_SIZE ) ) )
        {
            print_timer();
            if ( 0 == data_size )
                fprintf(stderr, "{ status: \"Success\", msg: \"End of file reached\", read_retuned: %d, errno: %d }\n", data_size, errno);
            else
                fprintf(stderr, "{ status: \"Error\",  msg: \"Error reading from stdin\", read_retuned: %d, errno: %d }\n", data_size, errno);

            exit(data_size);
        }
    } else return;

    // switch to write mode
    register double now = ev_now( loop );
    stdout_pipe.timer_start = now;
    stdin_pipe.time_waiting += now - stdin_pipe.timer_start;
    ev_io_stop( loop, (ev_io*)&stdin_pipe ); // stop stdin
    ev_io_start( loop, (ev_io*)&stdout_pipe ); // start stdout
}

static void stdout_callback (ev_io *watcher, int revents)
{
    if ( 0 < data_size )
    {
        if ( data_size != write( STDOUT_FILENO, &data[ 0 ], data_size ) )
        {
            print_timer();
            fprintf(stderr, "{ status: \"Error\",  msg: \"Error writing to stdout\", write_retuned: %d, errno: %d }\n", data_size, errno);
            exit(data_size);
        }

        bytes_out += data_size;
        data_size = 0;
    } else return;

    // Switch to read mode
    register double now = ev_now( loop );
    stdin_pipe.timer_start = now;
    stdout_pipe.time_waiting += now - stdout_pipe.timer_start;
    ev_io_stop( loop, (ev_io*)&stdout_pipe );
    ev_io_start( loop, (ev_io*)&stdin_pipe );
}

static void timer_callback(struct ev_loop *loop, ev_timer *w, int revents)
{
    print_timer();
}

static void sigint_callback (ev_async *w, int revents)
{
    print_timer();
    fprintf(stderr, "{ status: \"Success\", msg: \"Received SIGINT\" }\n", data_size, errno);
    exit(0);
}

int main(int argc, char **argv)
{
    data_size = 0;
    bytes_out = 0;
    loop = ev_default_loop (0);
    start_time = ev_time();
    ev_timer timer;
    ev_signal exitsig;
    stdout_pipe.time_waiting = 0;
    stdin_pipe.time_waiting = 0;
    stdin_pipe.timer_start = start_time;

    ev_io_init ((ev_io*)&stdin_pipe , stdin_callback , STDIN_FILENO , EV_READ);
    ev_io_init ((ev_io*)&stdout_pipe, stdout_callback, STDOUT_FILENO, EV_WRITE);
    ev_timer_init (&timer, timer_callback, 2.0, 2.0);
    ev_signal_init (&exitsig, sigint_callback, SIGINT);

    ev_io_start( loop, (ev_io*)&stdin_pipe );
    ev_timer_start (loop, &timer);
    ev_signal_start (loop, &exitsig);

    ev_run (loop, 0);
}
