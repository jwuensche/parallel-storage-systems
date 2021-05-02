# Performance analysis "Checkpoint"

The following pages describe different aspects of each tested version and compares them to another based on their specific implementation differences. All measurements were performed on an AMD EPYC 2nd Gen with 8 cores each with 2.5GHz.

## Runtime per Iteration

![Relative runtime per iteration for each thread and version](plots/relative_time.svg)

For a first overview of a versions performance we have chosen to view the relative runtime of it in relation to the amount of iterations.
We have discovered that as expected the `checkpoint` version performs best in comparison to the `fsync` and `optimized` version.
In Fig. 1 the time required for each iteration in seconds is shown.
As can be seen, the average time for all versions scales with the amount of threads used, whereas `checkpoint_fsync` does so more strongly than the others.
Additionally, `checkpoint_fsync` is in all case the least performing version, reaching it's minimal time required at 11 threads, still being slower than the longest average iteration speed of `checkpoint_optimized` with 1 thread.

All version scale, though `checkpoint` and `checkpoint` scale up to 8 threads, where after that mark performance degrades. The `checkpoint_fsync` version does not suffer from performance degradation, this is likely due to the generally low performance and long time idling in fsync calls as can be seen later on.

## Average Throughput

![Average throughput achieved](plots/bandwidth.svg)

--- 

Notice: We have recorded the throughput on the client side only, `checkpoint` writes into the cache but never clears it, therefore the throughput is not representative to the amount written to the storage medium.

---

Similarly to previous measurements we can see in Fig. 2 that all applications scale to a certain degree to the amount of threads used, reaching a maximum at about 8 threads.
Only `checkpoint_fsync` performs better after increasing to 9 threads and higher, this is probably due to the aforementioned imperformant implementation.
The optimized synchronization of `checkpoint_optimized` reaches around 250 MiB/s in contrast to the achieved 420 MiB/s in the `checkpoint` version this is a 170 MiB/s reduction in maximum throughput. Though a reduction was expected to occur, especially in maximum throughput situations.
Compared to the `checkpoint_fsync` version a substantial difference of 260 MiB/s is still seen in this case. Indicating that while the optimization does not offer speeds at the same level a noticeable improvement is done.

\newpage

# Average IO-Operations

![Average IO Operations](plots/iops.svg)

The achieved average IOPS, seen in Fig. 3, are identically distributed to the average throughput (Fig. 2). Much of the reasoning done there can be applied to this case.
This similar distribution is given due to the structure of the program, all io operations called are write operations with the same buffer size.

\newpage

## Duration of IO-Operations

![Duration of fsync compared to other operations](plots/iotime.svg)

Figure 4 shows the composition of total time spend in IO-Operations on average over all thread amounts. As expected `checkpoint` does not spend any time in synchronization calls. Version `checkpoint_fsync` spends most of the time accredited to IO in various fsync calls, this is due to the often and constant calling of fsync after every write call. In the optimized version this is reduced to represent only all a small part of the total IO time.
This also lead to a total reduction in IO time in this version.

\newpage

## Independent Composition of total IO time

![Duration spend in IO and fsync compared to the total runtime](plots/time_checkpoint.svg)

In Figure 5 we can see the total amount of time spend in various categories of the `checkpoint` version.
With an increased number of threads we can observe an increase in total time spend in IO operations other than fsync (which is in this case 0). This is likely due to more write calls for increasingly smaller data.
After reaching the 8 thread cap, IO time reduces drastically and remains relatively stable, the exact reason we are not sure at the moment, but could be due to other effects in our chosen parallelization.

\newpage

![Duration spend in IO and fsync compared to the total runtime](plots/time_checkpoint_fsync.svg)

Figure 6 shows a steady increase in total time spend in fsync calls over all threads. With this comes a reduction of the total runtime. The least time is spend in other io calls (e.g. write).

\newpage

![Duration spend in IO and fsync compared to the total runtime](plots/time_checkpoint_optimized.svg)

Figure 7 shows a relatively static effort in fsync calls, only varying slightly between different amount of threads. The reason for this can be found in the centralized fsync. The time for other IO increases relatively strongly in the end. The reason for this can be found in the synchronization of threads and the subsequent waiting for the main fsync to finish before continuing.
