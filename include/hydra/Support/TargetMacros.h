// Macros to select the threading model - exactaly one should be 1
#define KERNEL_THREADS 0
#define LIGHT_THREADS 1

// check that these have been set properly
#if (KERNEL_THREADS && LIGHT_THREADS) || !(KERNEL_THREADS || LIGHT_THREADS)
  #error "Error: Invalid threading model selection"
#endif
