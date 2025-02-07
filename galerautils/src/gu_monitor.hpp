/*
 * Copyright (C) 2008-2017 Codership Oy <info@codership.com>
 *
 * $Id$
 */

/*!
 * @file gu_monitor.hpp
 *
 *
 */

#ifndef __GU_MONITOR_HPP__
#define __GU_MONITOR_HPP__

#include <gu_lock.hpp>

#include <cassert>

namespace gu
{
    class Monitor;
    class Critical;
}

class gu::Monitor
{
    int mutable refcnt;
    Mutex       mutex;
    Cond        cond;

#ifndef NDEBUG
    gu_thread_t mutable holder;
#endif

    // copy contstructor and operator= disabled by mutex and cond members.
    // but on Darwin, we got an error 'class gu::Monitor' has pointer data members
    // so make non-copyable explicitly
    Monitor(const Monitor&);
    void operator=(const Monitor&);

public:

#ifndef NDEBUG
    Monitor(const wsrep_mutex_key_t* mutex_key,
            const wsrep_cond_key_t* cond_key)
        : refcnt(0), mutex(mutex_key), cond(cond_key), holder() {}
#else
    Monitor(const wsrep_mutex_key_t* mutex_key,
            const wsrep_cond_key_t* cond_key)
        : refcnt(0), mutex(mutex_key), cond(cond_key) {}
#endif

    ~Monitor() {}

    void enter() const
    {
        Lock lock(mutex);

// Teemu, pthread_equal() check seems redundant, refcnt too (counted in cond)
//        while (refcnt > 0 && pthread_equal(holder, pthread_self()) == 0)
        while (refcnt)
        {
            lock.wait(cond);
        }
        refcnt++;
#ifndef NDEBUG
        holder = gu_thread_self();
#endif
    }

    void leave() const
    {
        Lock lock(mutex);

        assert(refcnt > 0);
        assert(gu_thread_equal(holder, gu_thread_self()) != 0);

        refcnt--;
        if (refcnt == 0)
        {
            cond.signal();
        }
    }
};

class gu::Critical
{
    const Monitor& mon;

    Critical (const Critical&);
    Critical& operator= (const Critical&);

public:

    Critical(const Monitor& m) : mon(m)
    {
        mon.enter();
    }

    ~Critical()
    {
        mon.leave();
    }
};


#endif /* __GU_MONITOR_HPP__ */
