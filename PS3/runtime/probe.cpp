// Copyright (c) 2012- PPSSPP Project.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0 or later versions.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License 2.0 for more details.
//
// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>

constexpr std::string_view literal = "PS3 runtime";
static_assert(literal.size() == 11, "C++17 char_traits/string_view support");

int main() {
	std::puts("Runtime probe: main entered");
	std::fflush(stdout);
	std::mutex mutex;
	std::condition_variable condition;
	// Park each worker before creating the next, as ThreadManager does at startup.
	int parked = 0;
	bool release = false;
	std::thread idle[6];
	for (auto &worker : idle) {
		worker = std::thread([&] {
			std::unique_lock<std::mutex> lock(mutex);
			++parked;
			condition.notify_all();
			condition.wait(lock, [&] { return release; });
		});
		std::unique_lock<std::mutex> lock(mutex);
		condition.wait(lock, [&] { return parked == &worker - idle + 1; });
	}
	{
		std::lock_guard<std::mutex> lock(mutex);
		release = true;
	}
	condition.notify_all();
	for (auto &worker : idle) {
		worker.join();
	}
	std::puts("Runtime probe: six idle workers joined");
	std::fflush(stdout);
	std::once_flag once;
	std::atomic<int> initialized{0};
	auto shared = std::make_shared<int>(1234);
	std::puts("Runtime probe: synchronization objects initialized");
	std::fflush(stdout);
	int count = 0;
	int completed = 0;
	std::thread workers[4];
	for (auto &worker : workers) {
		worker = std::thread([&] {
			std::call_once(once, [&] { ++initialized; });
			for (int i = 0; i < 2000; ++i) {
				auto copy = shared;
				std::lock_guard<std::mutex> lock(mutex);
				if (*copy == 1234) {
					++count;
				}
			}
			std::lock_guard<std::mutex> lock(mutex);
			++completed;
			condition.notify_one();
		});
	}
	std::puts("Runtime probe: four workers created");
	std::fflush(stdout);
	std::unique_lock<std::mutex> lock(mutex);
	condition.wait(lock, [&] { return completed == 4; });
	lock.unlock();
	for (auto &worker : workers) {
		worker.join();
	}
	std::puts("Runtime probe: workers joined");
	std::fflush(stdout);
	lock.lock();
	bool signaled = condition.wait_for(lock, std::chrono::milliseconds(10), [] { return false; });
	lock.unlock();
	bool passed = count == 8000 && initialized == 1 && shared.use_count() == 1
		&& !signaled && std::to_string(count) == "8000" && std::stoi("1234") == 1234;
	std::printf("Runtime contention/shared_ptr/once/CV/C++17: count=%d init=%d %s\n",
		count, (int)initialized, passed ? "PASS" : "FAIL");
	return passed ? 0 : 1;
}
