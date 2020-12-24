## PROGRAM STRUCTURE

There are 2 groups of child threads:
- Musicians (who play an instrument)
- Singers

There can be a maximum of 10,000 total performers.  
Mutexes, semaphores and conditional variables have been used for thread synchronization.

## SYNCHRONIZATION LOGIC

- Musicians who can play only on a single stage type wait for that stage type to become available. 
  Musicians who can play on both acoustic and electric stages wait for any stage type. 
  If both types of stages are available, one is chosen at random.
  ```
  res = semTimedWait(&acoustic_stage, &ts, &wait_mutex); // wait for acoustic stage
  res = semTimedWait(&electric_stage, &ts, &wait_mutex); // wait for electric stage
  res = semTimedWait(&stage, &ts, &wait_mutex);          // wait for any stage
  ```

  - A stage of either type is chosen by a combination of ```semTryWait``` and ```semWait```, one for each type of stage.
    If a stage is available, it is guaranteed that either an acoustic or electric stage is available and hence either
    ```semTryWait``` will succeed (indicating that the first type of stage is available), else it will fail 
    and ```semWait``` will return immediately (as the second type of stage is definitely available).
    
    ```
    res = semTryWait(&acoustic_stage, &wait_mutex); // wait for acoustic before electric
    if(res == EAGAIN)                               // acoustic stage not available 
        semWait(&electric_stage, &wait_mutex);      // electric stage definitely available
    ```

- Singers wait until at least one stage is not occupied by a singer (who may or may not be performing with a musician).
  This guarantees that either a stage will be free, or a musician will be free to perform with, and the singer chooses 
  one of these possibilities randomly.
  ```
  res = semTimedWait(&singer_not_performing, &ts, &wait_mutex); 
  ```
  
  - In a similar manner to choosing any type of stage, one of the two options of performing solo or joining a 
    performing musician is chosen using a combination of ```semTryWait``` and ```semWait```, one for each option.
    One of these options is guaranteed to be possible.
    
    ```
    res = semTryWait(&stage, &wait_mutex);          // perform solo if stage is available
    if(res == EAGAIN)                               // stage is not available 
        semWait(&musician_performing, &wait_mutex); // join performing musician
    ```
  
- A performing musician may or may not be joined by a singer. This is implemented using a timed wait on a semaphore 
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
  
- Stage numbers on which each musician or singer is performing is kept track of.
  The status of a performer (```all_performers[(pi->performer_num)-1]->status```) is ```-1``` if not performing, 
  ```<stage number>``` if a musician/singer is performing solo, and ```0``` for a singer who has joined a musician.  
  
- Both singers and musicians collect t-shirts after their performance. A singer who joined a musician waits for a 
  signal from the musician thread that the performance is done, following which the singer collects a t-shirt. 
  The signal is sent using ```pthreadCondBroadcast``` which is handled in the required singer thread 
  (in which ```status = -1```) and ignored in others (in which ```status = 0```).
  ```
  while(all_performers[(pi->performer_num)-1]->status != -1)
      pthreadCondWait(&singer_left, &wait_mutex);
  ```
  
- The simulation ends once all musicians and singers have either collected t-shirts, or left due to impatience.
