// NOTE: this header is only for using the Thread Pool manually. When using on
// code transformed by Hydra, there is no need to use this header.

void spawn(const unsigned task, void (*f)(void));

void spawn(const unsigned task, void (*f)(void *), void *arg1);

void spawn(const unsigned task, void (*f)(void *, void *), void *arg1,
           void *arg2);

void spawn(const unsigned task, void (*f)(void *, void *, void *), void *arg1,
           void *arg2, void *arg3);
           
void spawn(const unsigned task, void (*f)(void *, void *, void *, void *),
           void *arg1, void *arg2, void *arg3, void *arg4);

void spawn(const unsigned task,
           void (*f)(void *, void *, void *, void *, void *), void *arg1,
           void *arg2, void *arg3, void *arg4, void *arg5);

void spawn(const unsigned task,
           void (*f)(void *, void *, void *, void *, void *, void *),
           void *arg1, void *arg2, void *arg3, void *arg4, void *arg5,
           void *arg6);

void spawn(const unsigned task,
           void (*f)(void *, void *, void *, void *, void *, void *, void *),
           void *arg1, void *arg2, void *arg3, void *arg4, void *arg5,
           void *arg6, void *arg7);

void spawn(const unsigned task, void (*f)(void *, void *, void *, void *,
                                          void *, void *, void *, void *),
           void *arg1, void *arg2, void *arg3, void *arg4, void *arg5,
           void *arg6, void *arg7, void *arg8);

void join(const unsigned task);
