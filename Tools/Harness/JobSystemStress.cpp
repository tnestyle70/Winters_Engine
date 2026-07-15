#include "Core/JobCounter.h"
#include "Core/JobSystem.h"
#include "Core/JobSystem/WorkStealingDeque.h"

#include <atomic>
#include <barrier>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <vector>

namespace
{
    using Clock = std::chrono::steady_clock;

    const char* ModeName(eJobExecutionMode eMode)
    {
        switch (eMode)
        {
        case eJobExecutionMode::ThreadOnly:
            return "ThreadOnly";
        case eJobExecutionMode::FiberShell:
            return "FiberShell";
        case eJobExecutionMode::FiberFull:
            return "FiberFull";
        default:
            return "Unknown";
        }
    }

    void Require(bool bCondition, const char* pMessage)
    {
        if (!bCondition)
            throw std::runtime_error(pMessage);
    }

    void PassiveWait(CJobCounter& counter, std::chrono::seconds timeout)
    {
        const Clock::time_point deadline = Clock::now() + timeout;
        while (!counter.IsComplete())
        {
            if (Clock::now() >= deadline)
                throw std::runtime_error("passive counter wait timed out");
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    void PrintResult(
        const char* pCase,
        eJobExecutionMode eMode,
        std::uint32_t iWorkers,
        std::uint64_t uJobs,
        Clock::duration elapsed,
        const CJobSystemStats& stats)
    {
        const double fMilliseconds =
            std::chrono::duration<double, std::milli>(elapsed).count();
        const double fJobsPerSecond = (fMilliseconds > 0.0)
            ? (static_cast<double>(uJobs) * 1000.0 / fMilliseconds)
            : 0.0;
        std::cout
            << "case=" << pCase
            << " mode=" << ModeName(eMode)
            << " workers=" << iWorkers
            << " jobs=" << uJobs
            << " elapsed_ms=" << fMilliseconds
            << " jobs_per_sec=" << fJobsPerSecond
            << " submitted=" << stats.uSubmitted
            << " executed=" << stats.uExecuted
            << " failed=" << stats.uFailed
            << " local=" << stats.uLocalPushes
            << " global=" << stats.uGlobalPushes
            << " steals=" << stats.uSteals
            << " inline=" << stats.uInlineExecutions
            << " fiber_switches=" << stats.uFiberSwitches
            << " fiber_waits=" << stats.uFiberWaits
            << " fiber_resumes=" << stats.uFiberResumes
            << " fiber_pool_misses=" << stats.uFiberPoolMisses
            << '\n';
    }

    void RunFanOut(eJobExecutionMode eMode, std::uint32_t iWorkers, std::uint32_t iJobs)
    {
        CJobSystem jobs;
        jobs.SetExecutionMode(eMode);
        jobs.Initialize(iWorkers);

        CJobCounter counter;
        std::atomic<std::uint32_t> iCompleted{ 0 };
        const Clock::time_point begin = Clock::now();
        for (std::uint32_t i = 0; i < iJobs; ++i)
        {
            jobs.Submit([&iCompleted]() {
                iCompleted.fetch_add(1, std::memory_order_relaxed);
            }, &counter);
        }
        jobs.WaitForCounter(&counter);
        const Clock::duration elapsed = Clock::now() - begin;

        Require(iCompleted.load(std::memory_order_acquire) == iJobs,
            "fan-out lost or duplicated work");
        const CJobSystemStats stats = jobs.GetStats();
        Require(stats.uExecuted == iJobs, "fan-out executed count mismatch");
        PrintResult("fan_out_help_wait", eMode, iWorkers, iJobs, elapsed, stats);
        jobs.Shutdown();
    }

    void RunPureWorkerFanOut(
        eJobExecutionMode eMode,
        std::uint32_t iWorkers,
        std::uint32_t iJobs)
    {
        CJobSystem jobs;
        jobs.SetExecutionMode(eMode);
        jobs.Initialize(iWorkers);

        CJobCounter counter;
        std::atomic<std::uint32_t> iCompleted{ 0 };
        const Clock::time_point begin = Clock::now();
        for (std::uint32_t i = 0; i < iJobs; ++i)
        {
            jobs.Submit([&iCompleted]() {
                iCompleted.fetch_add(1, std::memory_order_relaxed);
            }, &counter);
        }
        PassiveWait(counter, std::chrono::seconds(30));
        const Clock::duration elapsed = Clock::now() - begin;

        Require(iCompleted.load(std::memory_order_acquire) == iJobs,
            "pure-worker fan-out lost or duplicated work");
        const CJobSystemStats stats = jobs.GetStats();
        Require(stats.uExecuted == iJobs,
            "pure-worker fan-out executed count mismatch");
        PrintResult("fan_out_pure_worker", eMode, iWorkers, iJobs, elapsed, stats);
        jobs.Shutdown();
    }

    void RunNestedWait(eJobExecutionMode eMode, std::uint32_t iWorkers)
    {
        constexpr std::uint32_t kParents = 16;
        constexpr std::uint32_t kChildrenPerParent = 256;

        CJobSystem jobs;
        jobs.SetExecutionMode(eMode);
        jobs.Initialize(iWorkers);

        CJobCounter parentCounter;
        std::atomic<std::uint32_t> iChildrenCompleted{ 0 };
        const Clock::time_point begin = Clock::now();
        for (std::uint32_t parent = 0; parent < kParents; ++parent)
        {
            jobs.Submit([&jobs, &iChildrenCompleted]() {
                CJobCounter childCounter;
                for (std::uint32_t child = 0; child < kChildrenPerParent; ++child)
                {
                    jobs.Submit([&iChildrenCompleted]() {
                        iChildrenCompleted.fetch_add(1, std::memory_order_relaxed);
                    }, &childCounter);
                }
                jobs.WaitForCounter(&childCounter);
            }, &parentCounter);
        }
        PassiveWait(parentCounter, std::chrono::seconds(30));
        const Clock::duration elapsed = Clock::now() - begin;

        constexpr std::uint32_t kExpectedChildren = kParents * kChildrenPerParent;
        Require(iChildrenCompleted.load(std::memory_order_acquire) == kExpectedChildren,
            "nested wait lost or duplicated child work");
        const CJobSystemStats stats = jobs.GetStats();
        Require(stats.uExecuted == kParents + kExpectedChildren,
            "nested wait executed count mismatch");
        if (eMode == eJobExecutionMode::FiberFull)
        {
            Require(stats.uFiberWaits > 0, "FiberFull did not park a waiting fiber");
            Require(stats.uFiberResumes == stats.uFiberWaits,
                "FiberFull wait/resume count mismatch");
        }
        PrintResult(
            "nested_wait",
            eMode,
            iWorkers,
            kParents + kExpectedChildren,
            elapsed,
            stats);
        jobs.Shutdown();
    }

    void RunOverflow(eJobExecutionMode eMode)
    {
        constexpr std::uint32_t kChildren = 20000;
        CJobSystem jobs;
        jobs.SetExecutionMode(eMode);
        jobs.Initialize(1);

        CJobCounter parentCounter;
        std::atomic<std::uint32_t> iCompleted{ 0 };
        const Clock::time_point begin = Clock::now();
        jobs.Submit([&jobs, &iCompleted]() {
            CJobCounter childCounter;
            for (std::uint32_t i = 0; i < kChildren; ++i)
            {
                jobs.Submit([&iCompleted]() {
                    iCompleted.fetch_add(1, std::memory_order_relaxed);
                }, &childCounter);
            }
            jobs.WaitForCounter(&childCounter);
        }, &parentCounter);
        PassiveWait(parentCounter, std::chrono::seconds(30));
        const Clock::duration elapsed = Clock::now() - begin;

        Require(iCompleted.load(std::memory_order_acquire) == kChildren,
            "overflow fallback lost or duplicated work");
        const CJobSystemStats stats = jobs.GetStats();
        Require(stats.uGlobalPushes > 1,
            "overflow test did not exercise the global fallback queue");
        PrintResult("deque_overflow", eMode, 1, kChildren + 1, elapsed, stats);
        jobs.Shutdown();
    }

    void RunExceptionCompletion(eJobExecutionMode eMode)
    {
        CJobSystem jobs;
        jobs.SetExecutionMode(eMode);
        jobs.Initialize(2);

        CJobCounter counter;
        jobs.Submit([]() {
            throw std::runtime_error("intentional stress exception");
        }, &counter);
        PassiveWait(counter, std::chrono::seconds(10));

        const CJobSystemStats stats = jobs.GetStats();
        Require(stats.uExecuted == 1, "throwing job did not complete exactly once");
        Require(stats.uFailed == 1, "throwing job failure was not counted");
        PrintResult("exception_completion", eMode, 2, 1, Clock::duration::zero(), stats);
        jobs.Shutdown();
    }

    void RunShutdownDrain(eJobExecutionMode eMode)
    {
        constexpr std::uint32_t kJobs = 20000;
        CJobSystem jobs;
        jobs.SetExecutionMode(eMode);
        jobs.Initialize(2);

        std::atomic<std::uint32_t> iCompleted{ 0 };
        for (std::uint32_t i = 0; i < kJobs; ++i)
        {
            jobs.Submit([&iCompleted]() {
                iCompleted.fetch_add(1, std::memory_order_relaxed);
            });
        }
        const Clock::time_point begin = Clock::now();
        jobs.Shutdown();
        const Clock::duration elapsed = Clock::now() - begin;

        Require(iCompleted.load(std::memory_order_acquire) == kJobs,
            "shutdown did not drain accepted work");
        const CJobSystemStats stats = jobs.GetStats();
        Require(stats.uExecuted == kJobs, "shutdown executed count mismatch");
        PrintResult("shutdown_drain", eMode, 2, kJobs, elapsed, stats);
    }

    void RunMultiInstanceOwnerIsolation()
    {
        constexpr std::uint32_t kJobs = 10000;
        CJobSystem systemA;
        CJobSystem systemB;
        systemA.Initialize(2);
        systemB.Initialize(2);

        CJobCounter parentCounter;
        std::atomic<std::uint32_t> iCompleted{ 0 };
        const Clock::time_point begin = Clock::now();
        systemA.Submit([&systemB, &iCompleted]() {
            CJobCounter counterB;
            for (std::uint32_t i = 0; i < kJobs; ++i)
            {
                systemB.Submit([&iCompleted]() {
                    iCompleted.fetch_add(1, std::memory_order_relaxed);
                }, &counterB);
            }
            systemB.WaitForCounter(&counterB);
        }, &parentCounter);
        PassiveWait(parentCounter, std::chrono::seconds(30));
        const Clock::duration elapsed = Clock::now() - begin;

        Require(iCompleted.load(std::memory_order_acquire) == kJobs,
            "cross-system submit violated owner isolation");
        const CJobSystemStats statsB = systemB.GetStats();
        Require(statsB.uLocalPushes == 0,
            "foreign worker incorrectly pushed another system's deque bottom");
        PrintResult(
            "multi_instance_owner",
            eJobExecutionMode::ThreadOnly,
            4,
            kJobs + 1,
            elapsed,
            statsB);
        systemA.Shutdown();
        systemB.Shutdown();
    }

    void RunSubmitShutdownRace()
    {
        constexpr std::uint32_t kProducerCount = 8;
        constexpr std::uint32_t kJobsPerProducer = 5000;
        constexpr std::uint32_t kExpected = kProducerCount * kJobsPerProducer;

        CJobSystem jobs;
        jobs.Initialize(4);

        CJobCounter counter;
        std::atomic<std::uint32_t> iCompleted{ 0 };
        std::atomic<std::uint32_t> iReady{ 0 };
        std::atomic<std::uint32_t> iPreBoundaryComplete{ 0 };
        std::atomic<bool> bStartBoundaryRace{ false };
        std::atomic<bool> bStartPostBoundary{ false };
        std::vector<std::thread> producers;
        producers.reserve(kProducerCount);
        for (std::uint32_t producer = 0; producer < kProducerCount; ++producer)
        {
            producers.emplace_back([&]() {
                iReady.fetch_add(1, std::memory_order_release);
                while (iReady.load(std::memory_order_acquire) < kProducerCount)
                    std::this_thread::yield();

                constexpr std::uint32_t kPreBoundary = 1000;
                constexpr std::uint32_t kBoundaryRace = 1000;
                for (std::uint32_t i = 0; i < kPreBoundary; ++i)
                {
                    jobs.Submit([&iCompleted]() {
                        iCompleted.fetch_add(1, std::memory_order_relaxed);
                    }, &counter);
                }
                iPreBoundaryComplete.fetch_add(1, std::memory_order_release);
                while (!bStartBoundaryRace.load(std::memory_order_acquire))
                    std::this_thread::yield();
                for (std::uint32_t i = 0; i < kBoundaryRace; ++i)
                {
                    jobs.Submit([&iCompleted]() {
                        iCompleted.fetch_add(1, std::memory_order_relaxed);
                    }, &counter);
                }
                while (!bStartPostBoundary.load(std::memory_order_acquire))
                    std::this_thread::yield();
                for (std::uint32_t i = kPreBoundary + kBoundaryRace;
                     i < kJobsPerProducer;
                     ++i)
                {
                    jobs.Submit([&iCompleted]() {
                        iCompleted.fetch_add(1, std::memory_order_relaxed);
                    }, &counter);
                }
            });
        }

        while (iReady.load(std::memory_order_acquire) < kProducerCount)
            std::this_thread::yield();
        while (iPreBoundaryComplete.load(std::memory_order_acquire) < kProducerCount)
            std::this_thread::yield();
        const Clock::time_point begin = Clock::now();
        bStartBoundaryRace.store(true, std::memory_order_release);
        jobs.Shutdown();
        bStartPostBoundary.store(true, std::memory_order_release);
        for (std::thread& producer : producers)
            producer.join();
        const Clock::duration elapsed = Clock::now() - begin;

        Require(counter.IsComplete(),
            "Submit/Shutdown race left the shared counter incomplete");
        Require(iCompleted.load(std::memory_order_acquire) == kExpected,
            "Submit/Shutdown race lost or duplicated work");
        const CJobSystemStats stats = jobs.GetStats();
        Require(stats.uSubmitted == kExpected,
            "Submit/Shutdown race submitted count mismatch");
        Require(stats.uExecuted == kExpected,
            "Submit/Shutdown race executed count mismatch");
        Require(stats.uGlobalPushes > 0,
            "Submit/Shutdown race did not publish pre-boundary work");
        Require(stats.uInlineExecutions >=
                static_cast<std::uint64_t>(kProducerCount) * 3000u,
            "Submit/Shutdown race did not execute post-boundary work inline");
        PrintResult(
            "submit_shutdown_race",
            eJobExecutionMode::ThreadOnly,
            4,
            kExpected,
            elapsed,
            stats);
        jobs.Shutdown();
    }

    void RunStoppedInlineRecursiveSubmit()
    {
        CJobSystem jobs;
        std::atomic<bool> bOuterEntered{ false };
        std::atomic<bool> bAllowNested{ false };
        std::atomic<std::uint32_t> iCompleted{ 0 };

        std::thread submitter([&]() {
            jobs.Submit([&]() {
                bOuterEntered.store(true, std::memory_order_release);
                while (!bAllowNested.load(std::memory_order_acquire))
                    std::this_thread::yield();
                jobs.Submit([&iCompleted]() {
                    iCompleted.fetch_add(1, std::memory_order_relaxed);
                });
                iCompleted.fetch_add(1, std::memory_order_relaxed);
            });
        });

        while (!bOuterEntered.load(std::memory_order_acquire))
            std::this_thread::yield();
        std::thread initializer([&]() { jobs.Initialize(2); });
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        bAllowNested.store(true, std::memory_order_release);

        submitter.join();
        initializer.join();
        Require(iCompleted.load(std::memory_order_acquire) == 2,
            "paused lifecycle deadlocked or lost recursive inline Submit");
        jobs.Shutdown();
        std::cout << "case=stopped_inline_recursive_submit completed=2\n";
    }

    void RunExternalWaitShutdownRace()
    {
        constexpr std::uint32_t kJobs = 256;
        CJobSystem jobs;
        jobs.Initialize(4);

        CJobCounter counter;
        std::atomic<bool> bReleaseJobs{ false };
        std::atomic<bool> bWaitStarted{ false };
        std::atomic<bool> bWaitReturned{ false };
        std::atomic<std::uint32_t> iCompleted{ 0 };
        for (std::uint32_t i = 0; i < kJobs; ++i)
        {
            jobs.Submit([&]() {
                while (!bReleaseJobs.load(std::memory_order_acquire))
                    std::this_thread::yield();
                iCompleted.fetch_add(1, std::memory_order_relaxed);
            }, &counter);
        }

        std::thread waiter([&]() {
            bWaitStarted.store(true, std::memory_order_release);
            jobs.WaitForCounter(&counter);
            bWaitReturned.store(true, std::memory_order_release);
        });
        while (!bWaitStarted.load(std::memory_order_acquire))
            std::this_thread::yield();

        std::thread shutdown([&]() { jobs.Shutdown(); });
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        bReleaseJobs.store(true, std::memory_order_release);
        waiter.join();
        shutdown.join();

        Require(bWaitReturned.load(std::memory_order_acquire),
            "external WaitForCounter did not cross Shutdown safely");
        Require(counter.IsComplete(),
            "external Wait/Shutdown race left counter incomplete");
        Require(iCompleted.load(std::memory_order_acquire) == kJobs,
            "external Wait/Shutdown race lost work");
        std::cout
            << "case=external_wait_shutdown_race"
            << " jobs=" << kJobs
            << " completed=" << iCompleted.load(std::memory_order_relaxed)
            << '\n';
    }

    void RunReinitialize()
    {
        constexpr std::uint32_t kJobsPerGeneration = 2000;
        CJobSystem jobs;
        std::atomic<std::uint32_t> iCompleted{ 0 };
        for (std::uint32_t generation = 0; generation < 3; ++generation)
        {
            jobs.Initialize(2 + generation);
            Require(jobs.GetWorkerCount() == 2 + generation,
                "reinitialize worker count mismatch");
            CJobCounter counter;
            for (std::uint32_t i = 0; i < kJobsPerGeneration; ++i)
            {
                jobs.Submit([&iCompleted]() {
                    iCompleted.fetch_add(1, std::memory_order_relaxed);
                }, &counter);
            }
            jobs.WaitForCounter(&counter);
            jobs.Shutdown();
        }

        Require(iCompleted.load(std::memory_order_acquire) ==
                3u * kJobsPerGeneration,
            "reinitialize lost or duplicated work");
        std::cout
            << "case=reinitialize"
            << " generations=3"
            << " completed=" << iCompleted.load(std::memory_order_relaxed)
            << '\n';
    }

    void RunWorkerLifecycleGuard()
    {
        CJobSystem jobs;
        jobs.Initialize(1);
        CJobCounter counter;
        std::atomic<bool> bReturned{ false };
        jobs.Submit([&]() {
            jobs.Shutdown();
            bReturned.store(true, std::memory_order_release);
        }, &counter);
        PassiveWait(counter, std::chrono::seconds(10));
        Require(bReturned.load(std::memory_order_acquire),
            "worker lifecycle guard did not return");
        Require(jobs.GetWorkerCount() == 1,
            "worker-origin Shutdown crossed the lifecycle guard");
        jobs.Shutdown();
        std::cout << "case=worker_lifecycle_guard result=rejected\n";
    }

    void RunFiberSubmissionContextInterleave()
    {
        CJobSystem jobs;
        jobs.SetExecutionMode(eJobExecutionMode::FiberFull);
        jobs.Initialize(1);

        std::atomic<bool> bStartQueue{ false };
        std::atomic<bool> bReleaseGoJob{ false };
        std::atomic<bool> bReleaseChildren{ false };
        std::atomic<std::uint32_t> iParentsEntered{ 0 };
        std::atomic<std::uint32_t> iChildrenEntered{ 0 };
        std::atomic<std::uint32_t> iChildrenCompleted{ 0 };
        std::atomic<std::uint32_t> iFollowupsCompleted{ 0 };
        CJobCounter parentCounter;
        CJobCounter goCounter;
        CJobCounter releaseCounter;

        // Hold the only worker until the complete FIFO scenario is published.
        jobs.Submit([&]() {
            while (!bStartQueue.load(std::memory_order_acquire))
                std::this_thread::yield();
        });
        for (std::uint32_t i = 0; i < 2; ++i)
        {
            jobs.Submit([&]() {
                iParentsEntered.fetch_add(1, std::memory_order_release);
                jobs.WaitForCounter(&goCounter);
                jobs.Submit([&]() {
                    iChildrenEntered.fetch_add(1, std::memory_order_release);
                    jobs.WaitForCounter(&releaseCounter);
                    iChildrenCompleted.fetch_add(1, std::memory_order_relaxed);
                });
                // This starts after the yielded Submit scope has restored its
                // fiber-local predecessor. A thread-local linked stack would
                // traverse a sibling fiber's destroyed stack node here.
                jobs.Submit([&iFollowupsCompleted]() {
                    iFollowupsCompleted.fetch_add(1, std::memory_order_relaxed);
                });
            }, &parentCounter);
        }
        jobs.Submit([&]() {
            while (!bReleaseGoJob.load(std::memory_order_acquire))
                std::this_thread::yield();
        }, &goCounter);
        jobs.Submit([&]() {
            while (!bReleaseChildren.load(std::memory_order_acquire))
                std::this_thread::yield();
        }, &releaseCounter);
        bStartQueue.store(true, std::memory_order_release);

        const Clock::time_point parentDeadline =
            Clock::now() + std::chrono::seconds(10);
        while (iParentsEntered.load(std::memory_order_acquire) < 2 &&
               Clock::now() < parentDeadline)
        {
            std::this_thread::yield();
        }
        const bool bBothParentsEntered =
            iParentsEntered.load(std::memory_order_acquire) == 2;
        if (!bBothParentsEntered)
        {
            bReleaseGoJob.store(true, std::memory_order_release);
            bReleaseChildren.store(true, std::memory_order_release);
            jobs.Shutdown();
            Require(false,
                "FiberFull interleave did not park both parent fibers");
        }

        std::thread shutdown;
        try
        {
            shutdown = std::thread([&]() { jobs.Shutdown(); });

            // The go job holds the only worker, so this callback can run before
            // Submit returns only after Shutdown crosses the stopped-inline boundary.
            bool bStoppedInlineObserved = false;
            const Clock::time_point boundaryDeadline =
                Clock::now() + std::chrono::seconds(10);
            while (!bStoppedInlineObserved && Clock::now() < boundaryDeadline)
            {
                auto pProbeRan = std::make_shared<std::atomic<bool>>(false);
                jobs.Submit([pProbeRan]() {
                    pProbeRan->store(true, std::memory_order_release);
                });
                bStoppedInlineObserved =
                    pProbeRan->load(std::memory_order_acquire);
                if (!bStoppedInlineObserved)
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            Require(bStoppedInlineObserved,
                "FiberFull interleave did not observe the stopped inline boundary");

            bReleaseGoJob.store(true, std::memory_order_release);

            const Clock::time_point childDeadline =
                Clock::now() + std::chrono::seconds(10);
            while (iChildrenEntered.load(std::memory_order_acquire) < 2 &&
                   Clock::now() < childDeadline)
            {
                std::this_thread::yield();
            }
            const bool bBothChildrenEntered =
                iChildrenEntered.load(std::memory_order_acquire) == 2;
            bReleaseChildren.store(true, std::memory_order_release);
            shutdown.join();

            Require(bBothChildrenEntered,
                "FiberFull interleave did not yield both inline Submit contexts");
            Require(parentCounter.IsComplete() && goCounter.IsComplete() &&
                    releaseCounter.IsComplete(),
                "FiberFull interleave left a counter incomplete");
            Require(iChildrenCompleted.load(std::memory_order_acquire) == 2,
                "FiberFull FLS submission context lost a child completion");
            Require(iFollowupsCompleted.load(std::memory_order_acquire) == 2,
                "FiberFull FLS submission context corrupted a follow-up Submit");
            const CJobSystemStats stats = jobs.GetStats();
            Require(stats.uInlineExecutions >= 5,
                "FiberFull interleave did not cross the stopped inline boundary");
            Require(stats.uFiberWaits == stats.uFiberResumes,
                "FiberFull interleave wait/resume mismatch");
            std::cout
                << "case=fiber_submission_context_interleave"
                << " children=2 followups=2"
                << " waits=" << stats.uFiberWaits
                << " resumes=" << stats.uFiberResumes
                << " inline=" << stats.uInlineExecutions
                << '\n';
        }
        catch (...)
        {
            bReleaseGoJob.store(true, std::memory_order_release);
            bReleaseChildren.store(true, std::memory_order_release);
            if (shutdown.joinable())
                shutdown.join();
            else
                jobs.Shutdown();
            throw;
        }
    }

    void RunFiberPoolSaturation()
    {
        constexpr std::uint32_t kWaitingParents = 80;
        CJobSystem jobs;
        jobs.SetExecutionMode(eJobExecutionMode::FiberFull);
        jobs.Initialize(1);

        std::atomic<bool> bStartQueue{ false };
        std::atomic<bool> bReleaseGate{ false };
        std::atomic<std::uint32_t> iParentsEntered{ 0 };
        std::atomic<std::uint32_t> iParentsCompleted{ 0 };
        CJobCounter parentCounter;
        CJobCounter gateCounter;

        jobs.Submit([&]() {
            while (!bStartQueue.load(std::memory_order_acquire))
                std::this_thread::yield();
        });
        for (std::uint32_t i = 0; i < kWaitingParents; ++i)
        {
            jobs.Submit([&]() {
                iParentsEntered.fetch_add(1, std::memory_order_release);
                jobs.WaitForCounter(&gateCounter);
                iParentsCompleted.fetch_add(1, std::memory_order_relaxed);
            }, &parentCounter);
        }
        // FIFO order makes this run only after all parent jobs have either
        // parked on a fiber or entered the allocation-free inline fallback.
        jobs.Submit([&]() {
            while (!bReleaseGate.load(std::memory_order_acquire))
                std::this_thread::yield();
        }, &gateCounter);
        bStartQueue.store(true, std::memory_order_release);

        const Clock::time_point deadline =
            Clock::now() + std::chrono::seconds(10);
        while (iParentsEntered.load(std::memory_order_acquire) < kWaitingParents &&
               Clock::now() < deadline)
        {
            std::this_thread::yield();
        }
        const bool bAllParentsEntered =
            iParentsEntered.load(std::memory_order_acquire) == kWaitingParents;
        bReleaseGate.store(true, std::memory_order_release);
        PassiveWait(parentCounter, std::chrono::seconds(30));

        Require(bAllParentsEntered,
            "FiberFull pool saturation did not reach all waiting parents");
        Require(gateCounter.IsComplete(),
            "FiberFull pool saturation gate did not complete");
        Require(iParentsCompleted.load(std::memory_order_acquire) ==
                kWaitingParents,
            "FiberFull pool saturation lost a parent completion");
        const CJobSystemStats stats = jobs.GetStats();
        Require(stats.uFiberPoolMisses > 0,
            "FiberFull pool saturation did not exercise inline fallback");
        Require(stats.uFiberWaits == stats.uFiberResumes,
            "FiberFull pool saturation wait/resume mismatch");
        std::cout
            << "case=fiber_pool_saturation"
            << " parents=" << kWaitingParents
            << " waits=" << stats.uFiberWaits
            << " resumes=" << stats.uFiberResumes
            << " pool_misses=" << stats.uFiberPoolMisses
            << '\n';
        jobs.Shutdown();
    }

    void RunDequeLastItemRace()
    {
        constexpr std::uint32_t kIterations = 100000;
        CWorkStealingDeque<std::uintptr_t> deque;
        std::barrier phaseBarrier(2);
        bool bThiefWon = false;
        std::uintptr_t uThiefValue = 0;
        std::uint32_t iOwnerWins = 0;
        std::uint32_t iThiefWins = 0;
        bool bValid = true;

        std::thread thief([&]() {
            for (std::uint32_t i = 0; i < kIterations; ++i)
            {
                phaseBarrier.arrive_and_wait();
                std::uintptr_t uValue = 0;
                bThiefWon = deque.Steal(uValue);
                uThiefValue = uValue;
                phaseBarrier.arrive_and_wait();
            }
        });

        const Clock::time_point begin = Clock::now();
        for (std::uint32_t i = 0; i < kIterations; ++i)
        {
            const std::uintptr_t uExpected = static_cast<std::uintptr_t>(i) + 1u;
            if (!deque.Push(uExpected))
                bValid = false;

            phaseBarrier.arrive_and_wait();
            std::uintptr_t uOwnerValue = 0;
            const bool bOwnerWon = deque.Pop(uOwnerValue);
            phaseBarrier.arrive_and_wait();

            if (bOwnerWon == bThiefWon)
                bValid = false;
            if (bOwnerWon)
            {
                if (uOwnerValue != uExpected)
                    bValid = false;
                ++iOwnerWins;
            }
            else
            {
                if (!bThiefWon || uThiefValue != uExpected)
                    bValid = false;
                ++iThiefWins;
            }
            if (deque.SizeApprox() != 0)
                bValid = false;
        }
        thief.join();
        const Clock::duration elapsed = Clock::now() - begin;

        Require(iOwnerWins + iThiefWins == kIterations,
            "last-item race winner count mismatch");
        Require(bValid, "last-item CAS race violated uniqueness or generation integrity");
        const double fMilliseconds =
            std::chrono::duration<double, std::milli>(elapsed).count();
        std::cout
            << "case=deque_last_item_race"
            << " iterations=" << kIterations
            << " owner_wins=" << iOwnerWins
            << " thief_wins=" << iThiefWins
            << " elapsed_ms=" << fMilliseconds
            << " wraps=" << (kIterations / CWorkStealingDeque<std::uintptr_t>::kCapacity)
            << '\n';
    }

    void RunMode(eJobExecutionMode eMode)
    {
        const std::uint32_t iFanOutJobs =
            (eMode == eJobExecutionMode::FiberShell) ? 5000u : 100000u;
        RunFanOut(eMode, 4, iFanOutJobs);
        if (eMode != eJobExecutionMode::FiberShell)
            RunPureWorkerFanOut(eMode, 4, iFanOutJobs);
        RunNestedWait(eMode, 4);
        RunOverflow(eMode);
        RunExceptionCompletion(eMode);
        RunShutdownDrain(eMode);
    }
}

int main(int argc, char** argv)
{
    try
    {
        const std::string_view mode = (argc > 1) ? argv[1] : "all";
        if (mode == "thread" || mode == "all")
        {
            RunDequeLastItemRace();
            RunMode(eJobExecutionMode::ThreadOnly);
            RunMultiInstanceOwnerIsolation();
            RunSubmitShutdownRace();
            RunStoppedInlineRecursiveSubmit();
            RunExternalWaitShutdownRace();
            RunReinitialize();
            RunWorkerLifecycleGuard();
        }
        if (mode == "shell" || mode == "all")
            RunMode(eJobExecutionMode::FiberShell);
        if (mode == "fiber" || mode == "all")
        {
            RunMode(eJobExecutionMode::FiberFull);
            RunFiberSubmissionContextInterleave();
            RunFiberPoolSaturation();
        }

        if (mode != "thread" && mode != "shell" && mode != "fiber" && mode != "all")
            throw std::runtime_error("mode must be thread, shell, fiber, or all");

        std::cout << "JobSystem stress PASS\n";
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "JobSystem stress FAIL: " << exception.what() << '\n';
        return 1;
    }
}
