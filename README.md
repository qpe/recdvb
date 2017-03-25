# recdvb

Based on recdvb published on http://cgi1.plala.or.jp/~sat/


## Description

recdvb - command to record ISDB-T/S signals for linux DVB devices.
Maintain for pt3 with mirakurun.

## altnertive

- original - [http://cgi1.plala.or.jp/~sat/](http://cgi1.plala.or.jp/~sat/)
- ver.dogeel - [https://github.com/dogeel/recdvb](https://github.com/dogeel/recdvb)
- ver.k_pi - [https://github.com/k-pi/recdvb](https://github.com/k-pi/recdvb)

## Usage

- please see `--help`.

```
 $ recdvb --help
 $ recdvbchksig --help
 $ recdvbctl --help
```

## Requirement

- dvb drivers
- gcc

## how to install

```bash
 $ ./autogen.sh
 $ ./configure [--enable-b25]
 $ make
 $ sudo make install
```


## tested environment

- Device: PT3(with kernel 4.9.6 earth_pt3 driver)
- Software: Mirakurun

## license

- GPLv3

## contributor

- [Sat](http://cgi1.plala.or.jp/~sat/) -> Base
- [2GMon](https://github.com/2GMon) -> [this patch](https://gist.github.com/2GMon/3621dd5054ab20c2a8c565fc236de093)

