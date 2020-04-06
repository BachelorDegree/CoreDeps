#pragma once
#include <functional>
#include <vector>
#include <colib/co_routine.h>
#include <colib/coctx.h>
#include <sys/eventfd.h>
#include <assert.h>

class CoBatch
{
public:
  CoBatch(const CoBatch &) = delete;
  void operator=(const CoBatch &) = delete;
  CoBatch()
  {
    m_bRan = false;
  }
  template <class T>
  int AddTask(T func)
  {
    assert(m_bRan == false);
    TaskContext oTaskContext;
    size_t iTaskIndex = m_vecTaskContext.size();
    oTaskContext.bFinish = false;
    int iRet = co_create_capture(&oTaskContext.co, 0, [&, func, iTaskIndex]() {
      co_enable_hook_sys();
      func();
      this->OnTaskFinish(iTaskIndex);
    });
    if (iRet != 0)
    {
      return iRet;
    }
    m_vecTaskContext.push_back(oTaskContext);
    return 0;
  }
  int Run()
  {
    assert(m_bRan == false);
    m_bRan = true;
    m_iFinishTaskCnt = 0;
    m_iEventFd = eventfd(EFD_NONBLOCK, 0);
    if (m_iEventFd < 0)
    {
      return m_iEventFd;
    }
    for (size_t i = 0; i < m_vecTaskContext.size(); i++)
    {
      co_resume(m_vecTaskContext[i].co);
    }
    pollfd oPollFd;
    oPollFd.fd = m_iEventFd;
    oPollFd.events = POLLIN;
    while (m_iFinishTaskCnt < m_vecTaskContext.size())
    {
      co_poll(co_get_epoll_ct(), &oPollFd, 1, 0);
      int a;
      read(m_iEventFd, &a, sizeof(a));
    }
    close(m_iEventFd);
    m_iEventFd = -1;
    return 0;
  }
  ~CoBatch()
  {
    for (auto &oTask : m_vecTaskContext)
    {
      co_free(oTask.co);
    }
  }

private:
  struct TaskContext
  {
    stCoRoutine_t *co;
    bool bFinish;
  };
  template <class T>
  static int co_create_capture(stCoRoutine_t **co, const stCoRoutineAttr_t *attr, T func)
  {
    T *_func = new T(func);
    return co_create(co, attr, [](void *arg) -> void * {
      T &_func = *reinterpret_cast<T *>(arg);
      _func();
      delete &_func;
      return 0;
    },
                     (void *)_func);
  }
  void OnTaskFinish(size_t iTaskIndex)
  {
    auto &oTaskContext = m_vecTaskContext[iTaskIndex];
    oTaskContext.bFinish = true;
    m_iFinishTaskCnt++;
    int a = 1;
    write(m_iEventFd, &a, sizeof(a));
  }
  int m_iEventFd;
  size_t m_iFinishTaskCnt;
  bool m_bRan;
  std::vector<TaskContext> m_vecTaskContext;
};