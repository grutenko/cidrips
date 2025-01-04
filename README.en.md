

## cidrips

[![en](https://img.shields.io/badge/lang-en-red.svg)](ttps://github.com/grutenko/cidrips/blob/master/README.md)
[![ru](https://img.shields.io/badge/lang-ru-green.svg)](https://github.com/grutenko/cidrips/blob/master/README.ru.md)

Utlity for group list of ips by subnets

### Usage:

```bat
./build/cidrips -iips.txt -osubnets.txt -mcount --count=1  --overwrite --prefix="add route " --postfix="\n"
```
```
#  ips.txt
66.22.216.10, 66.22.216.11, 66.22.216.12, 66.22.216.122, 66.22.216.124, 66.22.216.125, 66.22.216.126, 66.22.216.128, 66.22.216.129, 66.22.216.13, 66.22.216.130, 66.22.216.131, 66.22.216.132, 66.22.216.139, 66.22.216.14, 66.22.216.141, 66.22.216.142, 66.22.216.143, 66.22.216.144
```
```
# subnets.txt
add route 66.22.216.0/24
```

### Arguments:

```
cidrips v0.2.0.rev9065823
Utlity for compress any IP list groupping by CIDR mask.

Usage:
        cidrips -i[FILE]
        cidrips -i[FILE] -o[FILE]
        cidrips -mlevel [-l[level]] -i[FILE] -o[FILE]
        cidrips -mcount [-c[count]] -i[FILE] -o[FILE]
        cidrips -i[FILE] -p[PREFIX] -P[POSTFIX]

Arguments:
        -i,--input    [FILE]           Path to file with input ips. (Use 
                                       --input - for stdin)
        -o,--out      [FILE]           Path to file with output subnets. (Use 
                                       --input - for stdin)
        -m,--mode     [level|count]    Use this method for generate 
                                       subnets: level >=0, count - maximum count
                                       of result subnets.
                                       [Default: level]
        -l,--level    [LEVEL]          Comression level.  [Default: 0]
        -c,--count    [COUNT]          Maxmimum count of result subnets.
                                       [Default: not specifie]
        -p,--prefix   [prefix]         Prefix for generated subnet in output.
                                       [Default: ""]
        -P,--postfix  [postfix]        Postfix for generated subnet in output.
                                       [Default: "\n"
        -O, --overwrite                Overwrite output file if not empty
        -A, --append                   Append into output file.
        -C, --cancel                   Cancel if output file is not empty.
            --no-stats                 No print any statistics
```

### Build
windows

```bat
clang cidrips.c -ocidrips.exe
```
