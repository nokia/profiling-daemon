`poor-perf` is a mix of linux `perf` and a watchdog. It comes with two modes: _watchdog_ and _oneshot_.

# Common options

`--mode` - either _watchdog_ or _oneshot_

`--cpu` - which cpu the watchdog should run and profile be taken from; one single cpu is valid at the moment

`--duration` - specify in seconds for how long system should be profiled

`--output` - filename to store the report, you can use `-` if you it to be printed on _stdout_.

# _watchdog_ mode

# _oneshot_ mode

```
podusows@podusows-ThinkPad-T480:~/Development/poor-perf$ sudo ./build/poor-perf --mode oneshot --output -
read 167161 kernel symbols
took map snapshot of 298 running processes
# 2019-10-31 10:40:12: oneshot profiling
# 2019-10-31 10:40:12: profiling cpu: 0
12016190261582;0;2271;Xorg;/usr/lib/x86_64-linux-gnu/libglapi.so.0.0.0;0x104e0;-
12016191933767;0;10491;chrome;/opt/google/chrome/chrome;0x43bfa61;-
12016193986998;0;0;<swapper>;-;0xffffffff98c3b62b;-
12016198312276;0;0;<swapper>;-;0xffffffff98c3b62b;-
12016203340631;0;0;<swapper>;-;0xffffffff98322ece;-
12016203418567;0;10491;chrome;/opt/google/chrome/chrome;0x435e661;-
12016204371818;0;2271;Xorg;<kernelmain>;0xffffffff98c3bc5b;_raw_spin_lock_irq
12016204441737;0;2271;Xorg;<kernelmain>;0xffffffff982d396b;update_blocked_averages
12016204575503;0;2271;Xorg;<kernelmain>;0xffffffff982c972c;check_preempt_curr
12016204642200;0;0;<swapper>;-;0xffffffff98c3b62b;-
12016204702507;0;2271;Xorg;/usr/lib/x86_64-linux-gnu/libEGL_mesa.so.0.0.0;0x17d50;-
12016204762974;0;2271;Xorg;[i915];0xffffffffc146ce05;fw_domains_get_with_fallback
```

# `report.py`

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
