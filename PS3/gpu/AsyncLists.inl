// Copyright (c) 2012- PPSSPP Project.
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2 or later.

#if PPSSPP_PLATFORM(PS3)
bool g_geAsyncLists = false;
bool g_geAsyncRunAhead = false;
bool g_geAsyncNoSyncWait = false;
#else
// Test builds: PPSSPP_GE_ASYNC=1 runs the lists on their own thread, 2 also with run-ahead.
bool g_geAsyncLists = getenv("PPSSPP_GE_ASYNC") != nullptr;
bool g_geAsyncRunAhead = getenv("PPSSPP_GE_ASYNC") && atoi(getenv("PPSSPP_GE_ASYNC")) >= 2;
bool g_geAsyncNoSyncWait = getenv("PPSSPP_GE_ASYNC") && atoi(getenv("PPSSPP_GE_ASYNC")) == 3;
#endif
// True on the GE list thread: what it calls itself doesn't wait for itself.
static thread_local bool t_geListThread = false;

// One job at a time: the emulation thread starts it (Run) and waits for it to be done (Wait).
class GeListThread {
public:
	explicit GeListThread(GPUCommon *gpu) : gpu_(gpu) {
#if PPSSPP_PLATFORM(PS3)
		// Its own stack size: the software GPU's draw path has big locals (the emulation thread's
		// stack is large, a std::thread's may not be).
		pthread_attr_t attr;
		pthread_attr_init(&attr);
		pthread_attr_setstacksize(&attr, 1024 * 1024);
		started_ = pthread_create(&thread_, &attr, &GeListThread::Entry, this) == 0;
		pthread_attr_destroy(&attr);
#else
		thread_ = std::thread([this] { Loop(); });
		started_ = true;
#endif
	}
	~GeListThread() {
		{
			std::lock_guard<std::mutex> lock(mutex_);
			quit_ = true;
		}
		cond_.notify_all();
		if (!started_)
			return;
#if PPSSPP_PLATFORM(PS3)
		pthread_join(thread_, nullptr);
#else
		thread_.join();
#endif
	}
	bool Started() const { return started_; }
	void Run(std::function<void()> job) {
		std::unique_lock<std::mutex> lock(mutex_);
		// The last job may have cleared its busy flag but not yet this.
		done_.wait(lock, [this] { return !pending_; });
		job_ = std::move(job);
		pending_ = true;
		cond_.notify_one();
	}
	void Wait() {
		std::unique_lock<std::mutex> lock(mutex_);
		done_.wait(lock, [this] { return !pending_; });
	}
	// Until the job is done or `progress` passes `need`. (Timed: the list thread only signals when
	// it sees someone waiting, and may look just before that.)
	void WaitProgress(const std::atomic<int> &progress, int need, std::atomic<bool> &waiting) {
		std::unique_lock<std::mutex> lock(mutex_);
		waiting.store(true, std::memory_order_relaxed);
		while (pending_ && progress.load(std::memory_order_acquire) <= need)
			done_.wait_for(lock, std::chrono::microseconds(200));
		waiting.store(false, std::memory_order_relaxed);
	}
	void Notify() {
		std::lock_guard<std::mutex> lock(mutex_);
		done_.notify_all();
	}

private:
#if PPSSPP_PLATFORM(PS3)
	static void *Entry(void *self) {
		((GeListThread *)self)->Loop();
		return nullptr;
	}
#endif
	void Loop() {
		t_geListThread = true;
#if PPSSPP_PLATFORM(PS3)
		if (g_frontendThreadStartedHook) {
			g_frontendThreadStartedHook("ge-list");
		}
#endif
		std::unique_lock<std::mutex> lock(mutex_);
		while (true) {
			cond_.wait(lock, [this] { return quit_ || pending_; });
			if (quit_)
				break;
			std::function<void()> job = std::move(job_);
			lock.unlock();
			job();
			lock.lock();
			pending_ = false;
			done_.notify_all();
		}
	}

	GPUCommon *gpu_;
	std::mutex mutex_;
	std::condition_variable cond_, done_;
	std::function<void()> job_;
	bool pending_ = false;
	bool quit_ = false;
	bool started_ = false;
#if PPSSPP_PLATFORM(PS3)
	pthread_t thread_;
#else
	std::thread thread_;
#endif
};

// CoreTiming calls this at each slice (mustFinish false: only if the list thread is done) and before
// idling (true: the time skipped could be the time its interrupt or sync event is due).
static void GeAsyncCoreTimingHook(bool mustFinish) {
	if (gpu)
		gpu->AsyncGate(mustFinish);
}

void GPUCommon::NotifyAsyncGate() {
	if (asyncThread_)
		asyncThread_->Notify();
}

void GPUCommon::TakeAsyncEffects() {
	std::vector<AsyncEffect> effects;
	{
		std::lock_guard<std::mutex> lock(asyncEffectsLock_);
		effects.swap(asyncEffects_);
	}
	for (const AsyncEffect &e : effects) {
		if (e.interrupt)
			__GeTriggerInterrupt(e.id, e.pc, e.ticks);
		else
			__GeTriggerSync(e.type, e.id, e.ticks);
	}
}

void GPUCommon::AsyncGate(bool mustFinish) {
	if (!asyncThread_ || t_geListThread)
		return;
	if (mustFinish || !AsyncBusy()) {
		AsyncWait(GEASYNC_IDLE);
		return;
	}
	if (g_geAsyncRunAhead) {
		// What it has raised so far, at its times (or now, if those have passed).
		TakeAsyncEffects();
		return;
	}
	const s64 now = (s64)CoreTiming::GetTicks(currentMIPS);
	const s64 need = now - (s64)asyncStartTicks_;
	if (asyncProgress_.load(std::memory_order_acquire) <= need) {
#if PPSSPP_ARCH(POWERPC)
		const uint64_t t0 = PS3Profile::Now();
#endif
		asyncThread_->WaitProgress(asyncProgress_, (int)std::min<s64>(need, 0x7FFFFFFF), asyncGateWaiting_);
#if PPSSPP_ARCH(POWERPC)
		PS3Profile::geAsyncGate += PS3Profile::Now() - t0;
		PS3Profile::geAsyncGates++;
#endif
	}
	if (!AsyncBusy()) {
		AsyncWait();
		return;
	}
	TakeAsyncEffects();
	CoreTiming::LimitSlice((s64)asyncStartTicks_ + asyncProgress_.load(std::memory_order_acquire) - now);
}

GPUCommon::~GPUCommon() {
	StopAsync();
}

void GPUCommon::StopAsync() {
	if (!asyncThread_)
		return;
	AsyncWait();
	delete asyncThread_;
	asyncThread_ = nullptr;
}

uint64_t g_geAsyncWaitTicks[GEASYNC_COUNT], g_geAsyncWaitCount[GEASYNC_COUNT];

void GPUCommon::AsyncWait(int reason) {
	if (!asyncThread_ || t_geListThread)
		return;
	if (asyncBusy_.load(std::memory_order_acquire)) {
#if PPSSPP_ARCH(POWERPC)
		const uint64_t t0 = PS3Profile::Now();
#endif
		asyncThread_->Wait();
#if PPSSPP_ARCH(POWERPC)
		const uint64_t dt = PS3Profile::Now() - t0;
		PS3Profile::geAsyncWait += dt;
		PS3Profile::geAsyncWaits++;
		g_geAsyncWaitTicks[reason] += dt;
		g_geAsyncWaitCount[reason]++;
#endif
	}
	CoreTiming::g_asyncWorkHook = nullptr;
	TakeAsyncEffects();
	if (!asyncCpuOps_.empty()) {
		std::vector<AsyncCpuOp> ops;
		ops.swap(asyncCpuOps_);
		for (const AsyncCpuOp &o : ops) {
			switch (o.op) {
			case DeferredOp::INVALIDATE: InvalidateCache(o.a, o.size, (GPUInvalidationType)o.extra); break;
			case DeferredOp::COPY: PerformMemoryCopy(o.a, o.b, o.size, (GPUCopyFlag)o.extra); break;
			case DeferredOp::SET: PerformMemorySet(o.a, (u8)o.extra, o.size); break;
			case DeferredOp::READBACK: PerformReadbackToMemory(o.a, o.size); break;
			case DeferredOp::WRITE_COLOR: PerformWriteColorFromMemory(o.a, o.size); break;
			case DeferredOp::VIDEO_COPY: NotifyVideoCopy(o.a, o.b, o.size); break;
			}
		}
	}
}

bool GPUCommon::AsyncDefer(DeferredOp op, u32 a, u32 b, int size, u32 extra) {
	if (!asyncBusy_.load(std::memory_order_acquire) || t_geListThread)
		return false;
	asyncCpuOps_.push_back(AsyncCpuOp{ op, a, b, size, extra });
	return true;
}

void GPUCommon::TriggerSync(GPUSyncType type, int id, u64 atTicks) {
	if (t_geListThread) {
		std::lock_guard<std::mutex> lock(asyncEffectsLock_);
		asyncEffects_.push_back(AsyncEffect{ false, type, id, 0, atTicks });
		return;
	}
	__GeTriggerSync(type, id, atTicks);
}

bool GPUCommon::TriggerInterrupt(int listid, u32 pc, u64 atTicks) {
	if (t_geListThread) {
		// (__GeTriggerInterrupt always succeeds.)
		std::lock_guard<std::mutex> lock(asyncEffectsLock_);
		asyncEffects_.push_back(AsyncEffect{ true, GPU_SYNC_DRAW, listid, pc, atTicks });
		return true;
	}
	return __GeTriggerInterrupt(listid, pc, atTicks);
}

