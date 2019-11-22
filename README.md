`profd` is a mix of linux `perf` tool and a watchdog which can be used to profile the system on extraoridinary high loads or almost total CPU starvation. Watchdog part fires up the `watchdog` thread (`SCHED_OTHER`) which tries to switch global `bool` variable. Main thread (`SCHED_FIFO`) checks that variable periodicaly and if it remains unchanged, profiler is started for a few seconds.


# Common options

`--mode` - either _watchdog_ or _oneshot_

`--cpu` - which cpu the watchdog should run and profile be taken from; one single cpu is valid at the moment

`--duration` - specify in seconds for how long system should be profiled

`--output` - filename to store the report; use `-` if you want it to be printed on standard output.


# _watchdog_ vs _oneshot_

_watchdog_ is a default, but `profd` can be also started with _oneshot_ mode. It fires the profiler immidiately and exits when it is done.


# Output format

Output is a text file. Most of the lines represent a perf event ([https://easyperf.net/blog/2018/08/26/Basics-of-profiling-with-perf](this) is one of the best places where you can read what that means) but there are also special ones. When the line starts with `#`, it is a message. '$' is used for setting up the columns format.

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

Samples come from two places, kernel and user space:

```
10210792566967;0;2844;pulseaudio;<kernelmain>;0xffffffff8aae40f4;add_wait_queue
10211516492065;0;2623;Xorg;[i915];0xffffffffc0ffdc85;intel_prepare_plane_fb
10210792696146;0;2844;pulseaudio;/usr/lib/pulse-12.2/modules/libprotocol-native.so;0x9bb0;-
```

If you think that first one is from the kernel because it contains something like `kernelmain` then you are right. It means that the code is located in the main kernel (and not the module).

This column is called `pathname` is suppose to contain a filename to the image having the instruction that was being executed when perf event fired (some places calls this a `dso`, regardless if it is a `.so` library or executable). On user space, this information can be obtained from `/proc/$PID/maps`.

In kernel, the filename will usualy have a `<kernelmain>` or the module like `[i915]` which is a driver for my _Intel 915_ graphic card which is unsurprisingly used by the _X server_. This information comes from `/proc/kallsyms` which should also have a symbol name, this is why you see it even without the postprocessing (user mode has a placeholder: `-`).


# `report.py`

It is a python script that can be used to postprocess the output.

```
$ cmake --build build && sudo ./build/poor-perf --mode oneshot --output - | ./report.py top
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
