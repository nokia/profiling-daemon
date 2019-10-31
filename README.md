`poor-perf` is a mix of linux `perf` and a watchdog. It comes with two modes: watchdog and oneshot.


# One shot profile

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

