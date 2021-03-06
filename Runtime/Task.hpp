#ifndef __TASK_HPP
#define __TASK_HPP

#include <ucontext.h>
#include <sys/types.h>
#include <memory>
#include <atomic>
#include <functional>
#include "Queue.hpp"
#include <thread>
#include <assert.h>
#include "Exception.hpp"
#include <chrono>
#include <vector>

namespace Runtime{

    typedef void (*ContextFunc)(void *);

    class Scheduler;
    class Task;
    //typedef Queue<Task *>::NonBlockQ TaskQ;
    
    class TaskQ:public Queue<Task *>::BlockQ {
    public:
        TaskQ(Scheduler &s);
        ~TaskQ();
        void OnEmpty() override;
    private:
        Scheduler &sched_;
    };


    /*
     * CPU和操作系统线程一一对应。
     */
    class CPU{
    public:
        CPU(Scheduler &sched);
        ~CPU();
        void Run();

        void PutPrivateTask(Task *t) noexcept;
        size_t PrivateTaskSize() const noexcept;

    public:
        static void Loop(CPU *core);
    private:
        std::thread thread_;
        size_t tick_;

        Scheduler &sched_;

        TaskQ private_queue_;

    public:
        Task *running_task;
        ucontext_t sched_context;

    public:
        static __thread CPU *current_core;
    };



    enum class TaskState{
        NEW = 0,
        READY,           // runnable.
        SCHED,           // simply abandon CPU.
        RUNNING,         // running.
        WAITING,         // abandon CPU because of waiting for some condition.
        SUSPEND,
        DEAD,
    };

    const size_t TASK_STACK_SIZE = 1024*64;
    struct TaskAttr{
        void *ss_sp;
        size_t ss_size;
    };

    typedef std::function<void (void *)> TaskFunctor;
    typedef void (*TaskFunc)(void *);
    typedef size_t TaskID;

    class TaskError:public Exception{
    public:
        TaskError(const char *func,int line)
            :Exception(func,line,"task state is not WAITING")
        {
        }
    };


    class Task{
    public:
        Task(TaskFunc f, void *arg, size_t ss = 0);
        Task(TaskFunctor f, void *arg, size_t ss = 0);
        ~Task() noexcept;

        void SetReady();
        void Resume() noexcept;
        void Yield() noexcept;

        void Run(void *arg){
            func_(arg);
        }

        TaskID Id() const{
            return task_id_;
        }

        inline TaskState State() const{
            return state_;
        }
    private:
        void init_context(CPU *);

    public:
        //64-bit intel
        static void MainFunc(int p1, int p2) noexcept;

    public:
        CPU *BindCPU;
        size_t Calls;

    private:
        static std::atomic<TaskID> task_id ;

    private:
        TaskState state_;
        std::mutex task_lock_;

        ucontext_t uctx_;
        TaskID task_id_;
        char * stack_ptr_;
        size_t stack_size;
        //TaskFunc func_;
        TaskFunctor func_;
        void *arg_;
    };


    typedef Queue<CPU *>::NonBlockQ CPUQ;
    class Scheduler{
    public:
        Scheduler(size_t count = 8);
        ~Scheduler();

        TaskID Spawn(TaskFunctor f, void *arg=NULL);

        inline void PutTask(Task *t);

        inline CPU *NextCPU(){
            std::lock_guard<std::mutex> _(public_lock_);
            ++core_id_;

            if(core_id_ == cpus_.size()){
                core_id_ = 0;
            }
            return cpus_[core_id_];
        }

        void PushIdleCPU(CPU *cpu){
            idle_queue_.Push(cpu);
        }

    private:
        std::mutex public_lock_;
        size_t core_id_;

        CPUQ idle_queue_;

        std::vector<CPU *> cpus_;
    };

    // Can only run in Task
    inline void Yield(){
        CPU::current_core->running_task->Yield();
    }
}
#endif// __TASK_HPP

