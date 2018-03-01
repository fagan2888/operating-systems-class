void increment_requesters();
void decrement_requesters();
int requesters_are_running();
void sleep_random();

void *requester_entry_point(void *void_ptr);
void *resolver_entry_point();