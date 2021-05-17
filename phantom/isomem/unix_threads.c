
#define DEBUG_MSG_PREFIX "vm.unixhal"
#include <debug_ext.h>
#define debug_level_flow 10
#define debug_level_error 10
#define debug_level_info 10

#include <stdarg.h>
#include <threads.h>


phantom_thread_t * get_current_thread() { return 0; }


void *get_thread_owner( phantom_thread_t *t ) { return 0; }


errno_t t_current_set_name( const char *name )
{
    (void) name;
    return EINVAL;
}
errno_t t_current_set_priority(int p)
{
    (void) p;
    return EINVAL;
}
errno_t         t_set_owner( tid_t tid, void *owner )
{
    return EINVAL;
}
//int get_current_tid() { return -1; }
errno_t t_get_owner( int tid, void ** owner ) { return ENOENT; }

errno_t t_get_ctty( tid_t tid, struct wtty **ct ) { return ENOENT; }

errno_t t_kill_thread(tid_t tid)
{
#if CONF_USE_E4C
    const e4c_exception *e = e4c_get_exception();
    e4c_print_exception(e);
#endif // CONF_USE_E4C
    panic("t_kill_thread(%d) called", tid);
}

errno_t t_current_set_death_handler( void (*handler)(phantom_thread_t *tp) )
{
#warning ignored?
    return 0;
}




void phantom_thread_wait_4_snap()
{
    // Just return
}

void phantom_snapper_wait_4_threads()
{
    // Do nothing in non-kernel version
    // Must be implemented if no-kernel multithread will be done
}


void phantom_snapper_reenable_threads()
{
    //
}


//void phantom_activate_thread()
//{
    // Threads do not work in this mode
//}