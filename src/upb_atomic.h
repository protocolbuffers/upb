/*
 * upb - a minimalist implementation of protocol buffers.
 *
 * Copyright (c) 2009 Joshua Haberman.  See LICENSE for details.
 *
 * Only a very small part of upb is thread-safe.  Notably, individual
 * messages, arrays, and strings are *not* thread safe for mutating.
 * However, we do make message *metadata* such as upb_msgdef and
 * upb_context thread-safe, and their ownership is tracked via atomic
 * refcounting.  This header implements the small number of atomic
 * primitives required to support this.  The primitives we implement
 * are:
 *
 * - a reader/writer lock (wrappers around platform-provided mutexes).
 * - an atomic refcount.
 */

#ifndef UPB_ATOMIC_H_
#define UPB_ATOMIC_H_

#include <stdbool.h>
#include "upb_misc.h"

#ifdef __cplusplus
extern "C" {
#endif

/* inline if possible, emit standalone code if required. */
#ifndef INLINE
#define INLINE static inline
#endif

#define UPB_THREAD_UNSAFE
#ifdef UPB_THREAD_UNSAFE

/* Non-thread-safe implementations. ******************************************/

typedef struct {
  int val;
} upb_atomic_refcount_t;

INLINE void upb_atomic_refcount_init(upb_atomic_refcount_t *a, int val) {
  a->val = val;
}

INLINE bool upb_atomic_ref(upb_atomic_refcount_t *a) {
  return a->val++ == 0;
}

INLINE bool upb_atomic_unref(upb_atomic_refcount_t *a) {
  return --a->val == 0;
}

typedef struct {
} upb_rwlock_t;

INLINE void upb_rwlock_init(upb_rwlock_t *l) { (void)l; }
INLINE void upb_rwlock_destroy(upb_rwlock_t *l) { (void)l; }
INLINE void upb_rwlock_rdlock(upb_rwlock_t *l) { (void)l; }
INLINE void upb_rwlock_wrlock(upb_rwlock_t *l) { (void)l; }
INLINE void upb_rwlock_unlock(upb_rwlock_t *l) { (void)l; }

#endif

/* Atomic refcount ************************************************************/

#ifdef UPB_THREAD_UNSAFE

/* Already defined above. */

#elif (__GNUC__ == 4 && __GNUC_MINOR__ >= 1) || __GNUC__ > 4

/* GCC includes atomic primitives. */

typedef struct {
  volatile int val;
} upb_atomic_refcount_t;

INLINE void upb_atomic_refcount_init(upb_atomic_refcount_t *a, int val) {
  a->val = val;
  __sync_synchronize();   /* Ensure the initialized value is visible. */
}

INLINE bool upb_atomic_ref(upb_atomic_refcount_t *a) {
  return __sync_fetch_and_add(&a->val, 1) == 0;
}

INLINE bool upb_atomic_unref(upb_atomic_refcount_t *a) {
  return __sync_sub_and_fetch(&a->val, 1) == 0;
}

#elif defined(WIN32)

/* Windows defines atomic increment/decrement. */
#include <Windows.h>

typedef struct {
  volatile LONG val;
} upb_atomic_refcount_t;

INLINE void upb_atomic_refcount_init(upb_atomic_refcount_t *a, int val) {
  InterlockedExchange(&a->val, val);
}

INLINE bool upb_atomic_ref(upb_atomic_refcount_t *a) {
  return InterlockedIncrement(&a->val) == 1;
}

INLINE bool upb_atomic_unref(upb_atomic_refcount_t *a) {
  return InterlockedDecrement(&a->val) == 0;
}

#else
#error Atomic primitives not defined for your platform/CPU.  \
       Implement them or compile with UPB_THREAD_UNSAFE.
#endif

/* Reader/Writer lock. ********************************************************/

#ifdef UPB_THREAD_UNSAFE

/* Already defined. */

#elif defined(UPB_USE_PTHREADS)

#include <pthread.h>

typedef struct {
  pthread_rwlock_t lock;
} upb_rwlock_t;

INLINE void upb_rwlock_init(upb_rwlock_t *l) {
  /* TODO: check return value. */
  pthread_rwlock_init(&l->lock, NULL);
}

INLINE void upb_rwlock_destroy(upb_rwlock_t *l) {
  /* TODO: check return value. */
  pthread_rwlock_destroy(&l->lock);
}

INLINE void upb_rwlock_rdlock(upb_rwlock_t *l) {
  /* TODO: check return value. */
  pthread_rwlock_rdlock(&l->lock);
}

INLINE void upb_rwlock_wrlock(upb_rwlock_t *l) {
  /* TODO: check return value. */
  pthread_rwlock_wrlock(&l->lock);
}

INLINE void upb_rwlock_unlock(upb_rwlock_t *l) {
  /* TODO: check return value. */
  pthread_rwlock_unlock(&l->lock);
}

#else
#error Reader/writer lock is not defined for your platform/CPU.  \
       Implement it or compile with UPB_THREAD_UNSAFE.
#endif

#ifdef __cplusplus
}  /* extern "C" */
#endif

namespace upb {

class RefCounted {
 public:
  RefCounted() { upb_atomic_refcount_init(&refcount_, 1); }
  void Ref() { upb_atomic_ref(&refcount_); }
  void Unref() { if (upb_atomic_unref(&refcount_)) delete this; }

 protected:
  virtual ~RefCounted() {}

 private:
  upb_atomic_refcount_t refcount_;
  DISALLOW_COPY_AND_ASSIGN(RefCounted);
};

template<class C> class ScopedRef {
 public:
  static const bool kNew = false;
  // Construct from a brand new object with:
  //   ScopedRef<Foo> foo(new Foo, kNew);
  // This will make us own the only reference.
  explicit ScopedRef(C* p = NULL, bool ref = true)
      : ptr_(p) {
    if (ptr_ && ref) ptr_->Ref();
  }
  ~ScopedRef() { ptr_->Unref(); }
  void reset(C* p = NULL) {
    if (p != ptr_) {
      if (ptr_) ptr_->Unref();
      if (p) p->Ref();
    }
    ptr_ = p;
  }
  C& operator*() const {
    assert(ptr_ != NULL);
    return *ptr_;
  }
  C* operator->() const {
    assert(ptr_ != NULL);
    return ptr_;
  }
  C* get() const { return ptr_; }
  C* release() {
    C* ret = ptr_;
    ptr_ = NULL;
    return ret;
  }

 private:
  C* ptr_;
  DISALLOW_COPY_AND_ASSIGN(ScopedRef);
};

class ReaderWriterLock {
 public:
  ReaderWriterLock() { upb_rwlock_init(&lock_); }
  ~ReaderWriterLock() { upb_rwlock_destroy(&lock_); }
  void ReaderLock() { upb_rwlock_rdlock(&lock_); }
  void WriterLock() { upb_rwlock_wrlock(&lock_); }
  void Unlock() { upb_rwlock_unlock(&lock_); }

 private:
  upb_rwlock_t lock_;
};

class ReaderMutexLock {
 public:
  ReaderMutexLock(ReaderWriterLock* l) : lock_(l) { lock_->ReaderLock(); }
  ~ReaderMutexLock() { lock_->Unlock(); }

 private:
  ReaderWriterLock* lock_;
};

class WriterMutexLock {
 public:
  WriterMutexLock(ReaderWriterLock* l) : lock_(l) { lock_->WriterLock(); }
  ~WriterMutexLock() { lock_->Unlock(); }

 private:
  ReaderWriterLock* lock_;
};

}  // namespace upb

#endif  /* UPB_ATOMIC_H_ */
