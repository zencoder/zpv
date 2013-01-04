# zpv: ZenPipeViewer

Yet another tool to monitor the progress of data through a pipe.

`zpv`, like [pv](http://www.ivarch.com/programs/pv.shtml) allows a user to see the progress of data through a pipeline. Unlike `pv`, `zpv` reports the amount of time spent waiting to read and write to stdin/stdout. This functionality gives more insight into the processes that generate and consume data to help performance profiling in long plipelines.

Output is human readable as well as valid json (One json object per line).

## Example Output

```json
{ "stdin_wait_ms": 26307, "stdout_wait_ms": 1691, "total_time_ms": 27999, "bytes_out": 410473472 }
```

### Definitions

1. **stdin_wait_ms**: The amount of time, in milliseconds, that `zpv` spent waiting for data on standard in.
2. **stdout_wait_ms**: The amount of time, in milliseconds, that `zpv` spent waiting for data to be consumed on standard out.
3. **total_time_ms**: The total amount of time, in milliseconds, that has elapsed.
4. **bytes_out**: The total number of bytes that have been written to standard out.

### Inferences

1. **throughput**: bytes_out / total_time_ms
2. **overhead created by the `zpv` tool itself**: total_time_ms - ( stdin_wait_ms + stdout_wait_ms )
3. **stalled input**: `stdin_wait_ms` continues to increment while bytes_out remains static
4. **stalled output**: stdout_wait_ms continues to increment while bytes_out remains static


## Example Usage

```bash
dd if=/dev/random | zpv >/dev/null
```

`zpv` has no options. It simple copies standard in to standard out and prints progress to standard error.
