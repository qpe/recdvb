# recdvb

Based on recdvb published on http://cgi1.plala.or.jp/~sat/


## Description

recdvb - command to record ISDB-T/S signals for linux DVB devices.
Maintain for pt3 with mirakurun.


## Alternative

- original - [http://cgi1.plala.or.jp/~sat/](http://cgi1.plala.or.jp/~sat/)
- ver.dogeel - [https://github.com/dogeel/recdvb](https://github.com/dogeel/recdvb)
- ver.k_pi - [https://github.com/k-pi/recdvb](https://github.com/k-pi/recdvb)

## Usage

- please see `--help`.

```
 $ recdvb --help
```

- sample `mirakurun config tuners`
```
- name: PT3-S1
  types:
    - BS
    - CS
  command: recdvb --dev 0 --b25 --lnb 15 <channel> - -
  isDisabled: false

- name: PT3-T1
  types:
    - GR
    command: recdvb --dev 1 --b25 <channel> - -
    isDisabled: false
```

- sample `mirakurun config channels`
```
- name: NHK G
  type: GR
  channel: '27'

- name: NHK BS1
  type: BS
  channel: BS15_0
  serviceId: 101
```


## Requirement

- dvb drivers
- gcc / clang


## Install

```bash
 $ ./autogen.sh
 $ ./configure [--enable-b25]
 $ make
 $ sudo make install
```


## Tested environment

- Device: PT3 (with kernel 4.9.6 earth_pt3 driver)
- Software: Mirakurun


## License

- GPLv3


## Contributor

- [Sat](http://cgi1.plala.or.jp/~sat/) -> Base
- [2GMon](https://github.com/2GMon) -> [this patch](https://gist.github.com/2GMon/3621dd5054ab20c2a8c565fc236de093)

