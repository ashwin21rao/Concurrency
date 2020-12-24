## COMPARISON OF MERGE SORT IMPLEMENTATIONS

- Normal mergesort runs faster than both multi-process and multi-threaded mergesort without exception. 
  
- For small n (n <= 5) multi-process mergesort runs faster than multi-threaded mergesort.
  (This is because selection sort is performed for n <= 5).
  ```
  n = 5
  Normal mergesort ran [ 1.614458 ] times faster than concurrent mergesort
  Normal mergesort ran [ 316.379007 ] times faster than multi-threaded mergesort
  ```

- For all other n, multi-threaded mergesort runs faster than multi-process mergesort without exception.
  ```
  n = 10000
  Normal mergesort ran [ 219.501776 ] times faster than concurrent mergesort
  Normal mergesort ran [ 60.100712 ] times faster than multi-threaded mergesort
  ```
  
- For large n (n > 100,000 but may be less depending on the system), forking of child processes or creation of child threads will
  eventually fail due to memory limits. In this case, the sorting defaults to normal merge sort to 
  complete the sorting process and an error message is printed. 
  ```
  Failed to fork child process: defaulting to normal merge sort
  Failed to create child thread: defaulting to normal merge sort
  Normal mergesort ran [ 193.409657 ] times faster than concurrent mergesort
  Normal mergesort ran [ 87.536262 ] times faster than multi-threaded mergesort
  ```

- For very large n (n > 1,000,000), the system may hang due to too much memory usage (do not try this).
