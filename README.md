# 2. Fork 與 Pthread_create 之間的使用差異
* Linux 中 process 和 thread, 一般要產生 thread 則呼叫 clone，產生 process 則呼叫 fork。對 Linux kernel 而言兩者都是 task ，並使用 task_struct 來描述 process 和 thread。
* 因此從 Linux kernel 來看根本上是 clone 一個已存在的 task（process/ thread）來實現。二者的差別就在於 clone 時 Flags 的傳遞。
    * Linux 上如果要設置 CLONE_VM 時（共享地址空間），則創建 thread ; 相反未設置時，創建 process。 因為當 pthread_create 傳遞 CLONE_* Flags，會告訴 kernel 不需要拷貝 the virtual memory image， the open files， the signal handlers 等，相比 Fork 可以節省時間。
    
> For processes, there's a bit of copying to be done when fork is invoked, which costs time. The biggest chunk of time probably goes to copying the memory image due to the lack of CLONE_VM. Note, however, that it's not just copying the whole memory; Linux has an important optimization by using COW (Copy On Write) pages. The child's memory pages are initially mapped to the same pages shared by the parent, and only when we modify them the copy happens. This is very important because processes will often use a lot of shared read-only memory (think of the global structures used by the standard library, for example).    
*  資料來源: [Launching Linux threads and processes with clone](https://eli.thegreenplace.net/2018/launching-linux-threads-and-processes-with-clone/)
    * ![](https://i.imgur.com/JfFcCdC.png)
    * 小結: 
        1. 其差異點在於共享資源的多寡，尤其在記憶體上是否共用；相同點則在最终皆是使用 System call 中的 do_fork 方法來完成 Copy process。
        2. 在Linux中，共享記憶體的 task 可以被看做是同一 process 下不同的 thread。從 Kernel 的角度來講，每一個 task 都具有一個 PID（Process IDentifier）。
            * 但我們可以發現同一 process 下的 thread 擁有不同的 PID。 此外，每一個 task 還具有 TGID（Task Group ID），同一 process 下的 thread TGID 是相同的，這個 TGID 即是我們意義下的 PID。
            * ![](https://i.imgur.com/Bt9PhOF.png)

## Fork
* Fork 創出的 Child process 複製了 Parent process 的 task_struct，包含系統的 mm_struct and page table ， 這意味著若使用者沒有執行內容修改下，Child process 和 Parent process 皆指向同一塊記憶體。 而當 Child process 改變了內容的時候（對變數進行寫入操作），會通過 copy_on_write 的手段，另將頁面建立一個新的副本。
>[fork 原始碼(linux 4.15.1)](https://elixir.bootlin.com/linux/v4.5.1/source/kernel/fork.c#L1786)
### Copy_on_write
* 寫入時複製（Copy-on-write） 是一個被使用在程式設計領域的最佳化策略。 
* 其基礎的觀念是，如果有多個 callers 同時要求相同資源，他們會共同取得相同的指標指向相同的資源，直到某個 caller 嘗試修改資源時，系統才會真正複製一個副本給該caller，以避免被修改的資源被直接察覺到，這過程對其他的呼叫都只是 transparently。 此作法主要的優點是如果呼叫者並沒有修改該資源，就不會有副本被建立，節省時間和空間。

## Clone
* linux 創建的 threads，底層其實是系統調用 clone 產生的 child processes。
* Userspace 的 Clone（) 函數定義在 glibc 當中，且會需要使用某些參數，也就是說它創建一個新的 process 或 thread 會帶參數。它們之間的區別在於哪些數據結構（memory space, processor state, stack, PID, open files）是共用的。
* 我們從 Linux Programmer's Manual 發現我們在 userspace 呼叫 clone 時，需要以下參數: 
    * ![](https://i.imgur.com/ig4hmc9.png)
        1. fn 函數指標，clone 需要一個函數指標來運行
        2. stack 用來設置創建的 task 的 stack 大小
        3. flags
            * SIGCHLD 子 process 終止時，應該發送 SIGCHLD 信號給 Parent process
            * CLONE_VM Parent 和 Child process
        4. arg 傳遞給 Child process 的參數
* 而 Userspace 階段的 Clone 會中斷在 sys_clone 的 Syscall 函數，將傳入參數帶入 do_fork 來創建 threads / process。
>[sys_clone 原始碼(linux 4.15.1)](https://elixir.bootlin.com/linux/v4.5.1/source/kernel/fork.c#L1805)

## 透過 strace 找出兩者於 Clone 點上的差異

* [Link to Source Code](https://github.com/tsuchidaken/sys_clone_fork/blob/main/main.c)
* 我們輸入 strace -f ./main 可以從 output 擷取 clone 的動作
```
clone(child_stack=0, flags=CLONE_CHILD_CLEARTID|CLONE_CHILD_SETTID|SIGCHLD, child_tidptr=0x7fa1944d69d0) = 4471
wait4(4471, strace: Process 4471 attached
 <unfinished ...>
[pid  4471] set_robust_list(0x7fa1944d69e0, 24) = 0
[pid  4471] fstat(1, {st_mode=S_IFCHR|0620, st_rdev=makedev(136, 19), ...}) = 0
[pid  4471] brk(NULL)                   = 0xb8c000
[pid  4471] brk(0xbad000)               = 0xbad000
[pid  4471] write(1, "I am the child 4471\n", 20I am the child 4471
) = 20
[pid  4471] mmap(NULL, 8392704, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_STACK, -1, 0) = 0x7fa1934e3000
[pid  4471] mprotect(0x7fa1934e3000, 4096, PROT_NONE) = 0
[pid  4471] clone(strace: Process 4472 attached
 <unfinished ...>
[pid  4472] set_robust_list(0x7fa193ce39e0, 24 <unfinished ...>
[pid  4471] <... clone resumed> child_stack=0x7fa193ce2ff0, flags=CLONE_VM|CLONE_FS|CLONE_FILES|CLONE_SIGHAND|CLONE_THREAD|CLONE_SYSVSEM|CLONE_SETTLS|CLONE_PARENT_SETTID|CLONE_CHILD_CLEARTID, parent_tidptr=0x7fa193ce39d0, tls=0x7fa193ce3700, child_tidptr=0x7fa193ce39d0) = 4472
```
* 由此我們可得知
    1. 純粹的fork呼叫會轉化為最陽春的_do_fork，僅有一flags參數傳入SIGCHLD。
    2. 藉由 fork 呼叫，根據上面的strace訊息得到clone的會是**child_stack= 0, flags=CLONE_CHILD_CLEARTID|CLONE_CHILD_SETTID|SIGCHLD, child_tidptr=0x7fa1944d69d0**
    3. 藉由 pthread_create 呼叫，會得到 **child_stack=0x7fa193ce2ff0, flags=CLONE_VM|CLONE_FS|CLONE_FILES|CLONE_SIGHAND|CLONE_THREAD|CLONE_SYSVSEM|CLONE_SETTLS|CLONE_PARENT_SETTID|CLONE_CHILD_CLEARTID, parent_tidptr=0x7fa193ce39d0, tls=0x7fa193ce3700, child_tidptr=0x7fa193ce39d0**
* 結論: 
    * Thread 的創建會傳入給予child所使用的stack空間，而fork程序則不需要；
    * CLONE_THREAD的特性主要是宣告執行緒而非產生新的進程，儘管在核心的角度看起來是兩著很相似。(tgid = parent id)
       >CLONE_THREAD: the child is placed in the same thread group as the calling process