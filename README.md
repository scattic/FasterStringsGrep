# FasterStringsGrep

## Overview

This is a faster implementation and an alternative to running `strings` followed by one or more `grep` commands. The optimization is achieved from using the Linux mmap function, thus mapping the file into memory and leaving all read-ahead optimizations for the kernel to perform. The execution is at least 2x faster than classic strings + grep.

## Compiling 

* `make all` will build both a debug version called `fsg_dgb` and an optimized build in `fsg`
* `make test` which (!) requires the presence of a large _test.iso_ file (use a Windows ISO for example), will run and compare _fsg_ with _strings_ + _grep_   

## Syntax

```
FasterStringsGrep - faster alternative to running strings | grep

  ./fsg [-o output] [-n length] [-j offset] [-q] [-h] [-f "filter"] [-r "regex"] [-t template] input

where:
input:		name of input file
-o output:	optional, name of output file or `stdout` if none specified
-n length:	optional, minimum number of chars for a string, default is 4
-j offset:	optional, start searching from this offset onwards. Useful to resume a failed search in a large file.
-f filter:	optional, filter argument is a special string containing list of words which must exist or not in the output.
		Prefix words which must be included with a +, and excluded with a -.
		All inclusions are OR'ed, then result is AND'ed with the OR'ed exclusions.
		Words are case sensitive, must be separated by spaces, and sub-strings with spaces are not supported.
		Example: "+foo +goo -bar -tar" will output strings which contain 'foo' OR 'goo' AND do NOT contain 'bar' OR 'tar'.
-q:		run quietly, without showing progress. Progress is shown only when writing results to a file.
-h:		this message
```

## Sample output

```
$ ./fsg test.iso -f '+Microsoft -Corporation' -o fsg_filter.out
> strings with 'Microsoft' will be included
> strings with 'Corporation' will be excluded

Starting...
Progress: 9%	[77.00 MBps]	[404895744 total bytes read]	[AVG: 64.00 MBps]	[ETA: 0.91 minutes]
Progress: 21%	[90.00 MBps]	[878927872 total bytes read]	[AVG: 69.00 MBps]	[ETA: 0.73 minutes]
Progress: 33%	[89.00 MBps]	[1347748864 total bytes read]	[AVG: 71.00 MBps]	[ETA: 0.61 minutes]
Progress: 44%	[88.00 MBps]	[1814136832 total bytes read]	[AVG: 72.00 MBps]	[ETA: 0.49 minutes]
Progress: 56%	[88.00 MBps]	[2277926912 total bytes read]	[AVG: 72.00 MBps]	[ETA: 0.39 minutes]
Progress: 67%	[88.00 MBps]	[2744466432 total bytes read]	[AVG: 72.00 MBps]	[ETA: 0.29 minutes]
Progress: 79%	[88.00 MBps]	[3210672128 total bytes read]	[AVG: 72.00 MBps]	[ETA: 0.19 minutes]
Progress: 90%	[86.00 MBps]	[3661746176 total bytes read]	[AVG: 72.00 MBps]	[ETA: 0.09 minutes]
Done.
```

## Performance

| Test | Description | Time(s)
| ----------- | ----------- |----------- |
| `strings test.iso > strings.out` | Normal execution of strings | 133.39
| `fsg test.iso -q -o fsg.out` | Quiet execution of fsg | 59.01
| `strings test.iso \| grep Microsoft \| grep -v Corporation > strings_filter.out` | Normal execution of strings with two pipes | 132.46
| `fsg test.iso -q -f '+Microsoft -Corporation' -o fsg_filter.out` | Equivalent to above | 55.90

**NOTE**: execution is faster when filtering by keywords due to reduced disk writes. 

## Testing full output

This is a sample output from `make test`:

```
- Removing older result files
======= Running simple strings dump test =======
- Clearing cache
- Running common strings
time -p sh -c "(strings test.iso > strings.out)"
real 133.39
user 127.86
sys 4.81
- Clearing cache again
- Running fsg
time -p sh -c "( ./fsg test.iso -q -o fsg.out )"
real 59.01
user 55.49
sys 3.10
- Comparing results of simple string dump test
fa577d6e1b059a54ea844a5cbda8a3c51192972fe2f2f122e91cbd9eb2d3e0c1  strings.out
fa577d6e1b059a54ea844a5cbda8a3c51192972fe2f2f122e91cbd9eb2d3e0c1  fsg.out
======= Running simple filtering test =======
- Clearing cache
- Running common strings and two grep pipes
time -p sh -c "(strings test.iso | grep Microsoft | grep -v Corporation > strings_filter.out)"
real 132.46
user 128.88
sys 5.44
- Clearing cache again
- Running fsg
time -p sh -c "( ./fsg test.iso -q -f '+Microsoft -Corporation' -o fsg_filter.out )"
real 55.90
user 53.49
sys 2.37
- Comparing results of simple filtering test
a5594d59e643103cafb9aa15dc9b9f1751618de419fbe28b64d97eda7fb91c20  strings_filter.out
a5594d59e643103cafb9aa15dc9b9f1751618de419fbe28b64d97eda7fb91c20  fsg_filter.out
DONE
```
