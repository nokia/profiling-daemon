`poor-perf` is a mix of linux `perf` and a watchdog. It comes with two modes: _watchdog_ and _oneshot_.


# Common options

`--mode` - either _watchdog_ or _oneshot_

`--cpu` - which cpu the watchdog should run and profile be taken from; one single cpu is valid at the moment

`--duration` - specify in seconds for how long system should be profiled

`--output` - filename to store the report; you can use `-` if you it to be printed on _stdout_.


# _watchdog_ mode

In this mode, an additional thread is started with `SCHED_OTHER` scheduler class and its one and only function is to switch global atomic flag to `true`, sleep for 1 second and then start over again. The main thread (which runs on `SCHED_FIFO`) monitors that flag and runs profiling for a brief moment when the flag is not switched in time because of some sort of cpu core starvation. After profiling is done, the `watchdog` keeps running so multiple sessions can appear in the output.

This mode was the key reason why we did implement this tool.


# _oneshot_ mode

This executes one profiling session immidiately.


# Output format

```
# 2019-11-04 11:09:46: oneshot profiling
# 2019-11-04 11:09:46: profiling cpu: 0
$ time;cpu;pid;comm;pathname;addr;name
10210785447776;0;0;<swapper>;-;0xffffffff8aa7504a;-
10210788186300;0;3607;chrome;<kernelmain>;0xffffffff8b420a21;timerqueue_add
10210788201844;0;3607;chrome;<kernelmain>;0xffffffff8b420a21;timerqueue_add
10210792194558;0;0;<swapper>;-;0xffffffff8aad38d9;-
10210792203914;0;0;<swapper>;-;0xffffffff8aad395d;-
10210792212681;0;0;<swapper>;-;0xffffffff8aad395d;-
10210792509512;0;2844;pulseaudio;<kernelmain>;0xffffffff8acd5732;__fget
10210792566967;0;2844;pulseaudio;<kernelmain>;0xffffffff8aae40f4;add_wait_queue
10210792696146;0;2844;pulseaudio;/usr/lib/pulse-12.2/modules/libprotocol-native.so;0x9bb0;-
```


# `report.py`

It is a python script that can be used to postprocess the output.

```
podusows@podusows-ThinkPad-T480:~/Development/poor-perf$ cmake --build build && sudo ./build/poor-perf --mode oneshot --output - | ./report.py top
[100%] Built target poor-perf
read 167161 kernel symbols
took map snapshot of 293 running processes
2341 chrome /opt/google/chrome/chrome 0x49fd0c0 mallinfo
1559 <swapper> - 0xffffffff9827504a -
1524 chrome /opt/google/chrome/chrome 0x1c68f65 _start
186 chrome /opt/google/chrome/chrome 0x32e7130 ChromeMain
86 chrome <kernelmain> 0xffffffff98204269 prepare_exit_to_usermode
77 chrome <kernelmain> 0xffffffff98e01130 page_fault
71 chrome <kernelmain> 0xffffffff982d3826 update_blocked_averages
49 chrome <kernelmain> 0xffffffff98c279c7 clear_page_erms
40 chrome /usr/lib/x86_64-linux-gnu/libc-2.29.so 0x1890dc __nss_passwd_lookup
40 chrome /opt/google/chrome/chrome 0x43a5336 ChromeMain
39 chrome <kernelmain> 0xffffffff982f1ef0 native_queued_spin_lock_slowpath
38 chrome <kernelmain> 0xffffffff9822f680 sync_regs
```
