#pragma once
#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>
#include <stdexcept>

class ThreadPool
{
private:

	std::vector<std::thread> workers;	//工作线程组
	std::queue<std::function<void()>> tasks;	//任务队列

	std::mutex queue_mutex;	//锁
	std::condition_variable condition;	//条件变量
	bool stop;

public:

	ThreadPool(size_t threads) : stop(false)
	{
		for (size_t i = 0; i < threads; i++)
		{
			workers.emplace_back([this] {
				while (true)
				{
					std::function<void()>task;	//任务
					{
						std::unique_lock<std::mutex> lock(this->queue_mutex);	//上锁
						this->condition.wait(lock, [this] { return this->stop || !this->tasks.empty();});	//睡着，等待唤醒，检查条件
						if (this->stop && this->tasks.empty()) return;	//分辨是不是结束的信号
						task = std::move(this->tasks.front());	//将任务从队列中移出来
						this->tasks.pop();	//弹出任务
					}
					task();	//执行任务
				}
			});
		}
	}

	template<class F, class... Args>
	auto enqueue(F&& f, Args&&... args)	//可以接收任意数量和类型的参数
		-> std::future<typename std::invoke_result<F, Args...>::type>	//这里相当于返回一个future,并计算出了类型
	{
		using return_type = typename std::invoke_result<F, Args...>::type;	//记住返回类型

		auto task = std::make_shared<std::packaged_task<return_type()>>//制作package_task，包装一个可执行的无参函数，返回值是return_type
		(	
			std::bind(std::forward<F>(f), std::forward<Args>(args)...)	//将函数和参数绑定，做成了一个可执行的函数
		);
		std::future<return_type> res = task->get_future();	//取到future
		{
			std::unique_lock<std::mutex> lock(queue_mutex);	//上锁

			if (stop)	//如果出于线程池结束期间，就抛出异常
				throw std::runtime_error("stopped ThreadPool!");

			//向任务队列添加任务，函数[task]{}，函数体是(*task)()，因为它已经被制作好了，参数函数都被绑定了，可直接执行。
			tasks.emplace([task]() { (*task)(); });	

		}	//出作用域，释放锁

		condition.notify_one();	//唤醒一个线程干活

		return res;
	}

	~ThreadPool()
	{
		{
			std::unique_lock<std::mutex> lock(queue_mutex);	//先上锁
			stop = true;	//修改标志
		}	//自动释放锁

		condition.notify_all();	//唤醒所有线程

		for (std::thread& worker : workers)	//回收线程资源
			worker.join();
	}
};
