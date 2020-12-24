## PROGRAM STRUCTURE

There are 2 groups of child threads:
- Musicians (who play an instrument)
- Singers

Mutexes, condition variables and semaphores have been used for thread synchronization.

## SYNCHRONIZATION LOGIC

- Musicians who can play only on a single stage type wait for that stage type to become available. 
  Musicians who can play on both acoustic and electric stages wait for any stage type. If both types of stages
  are available, one is chosen at random.
  ```
  while(total_acoustic_stages <= 0 && res == 0)
      res = pthreadCondTimedWait(&acousticic_stage_available, &wait_mutex, &ts); // wait for acoustic stage
  
  while(total_electric_stages <= 0 && res == 0)
        res = pthreadCondTimedWait(&electric_stage_available, &wait_mutex, &ts); // wait for electric stage
  
  while(total_stages <= 0 && res == 0)
        res = pthreadCondTimedWait(&stage_available, &wait_mutex, &ts); // wait for any stage
  ```
  
- Singers wait until at least one stage is not occupied by a singer (who may or may not be performing with a musician).
  This guarantees that either a stage will be free, or a musician will be free to perform with, and the singer chooses 
  one of these possibilities randomly.
  ```
  while(singers_not_performing <= 0 && res == 0)
      res = pthreadCondTimedWait(&singer_done_performing, &wait_mutex, &ts);
  ```
  
- A performing musician my or may not be joined by a singer. This is implemented using a timed wait on a semaphore 
  indicating that a singer is ready to join the musicians performance. 
  
  - An ```ETIMEDOUT``` error indicates that no singer joined the performance, and the musician is done performing. 
    ```
    res = semTimedWait(&singer_joined, &ts); // wait for singer to join performance
    ```
  
  - If a singer joins the performance, it is extended by 2 seconds and the thread sleeps until the performance ends.
    ```
    clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &ts, NULL); // wait for performance to get over
    ```
    
- Synchronization of t-shirt collection is done using a semaphore whose value at any given time is the number of 
  coordinators available. 
  ```
  semWait(&coordinator_available); // wait for coordinator
                                   // collect t-shirt
  semPost(&coordinator_available); // signal that coordinator is available
  ```
  
- Both singers and musicians collect t-shirts after their performance. A singer who joined a musician waits for a 
  signal from the musician thread that the performance is done, following which the singer collects a t-shirt. 
  The signal is sent using ```pthreadCondBroadcast``` which is handled in the required singer thread and ignored in 
  others. 
  ```
  while(all_performers[(pi->performer_num)-1]->status != -1)
      pthreadCondWait(&singer_left, &wait_mutex);
  ```

- Stage numbers on which each musician or singer is performing, is kept track of.
  The status of a performer (```all_performers[(pi->performer_num)-1]->status```) is ```-1``` if not performing, 
  ```<stage number>``` if musician/singer is performing solo, and ```0``` for a singer who has joined a musician.

- The simulation ends once all musicians and singers have either collected t-shirts, or left due to impatience.
