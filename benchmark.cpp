#include "multithreaded_container.h"

int main(int argc, char** argv)
{
	// no. of data operation, emements in container and # of threads
	const size_t iterations_count = 2000000;
	const size_t container_size = 100000;
	std::vector<std::thread> vec_thread(std::thread::hardware_concurrency());
	const bool measure_latency = false;
	std::function<void(void)> burn_cpu = []() {};

	// specify max threads from command line
	if (argc >= 2){
		vec_thread.resize(std::stoi(std::string(argv[1])));
	}

	std::cout << "CPU Cores: " << std::thread::hardware_concurrency() << std::endl;
	std::cout << "Container size = " << container_size << std::endl;
	std::cout << "Threads =  " << vec_thread.size() << ", iteration per thread = " << iterations_count << std::endl;
	std::cout << "Time & MOps - steady_clock is_steady = " << std::chrono::steady_clock::is_steady << ", num/den = " << (double)std::chrono::steady_clock::period::num << " / " << std::chrono::steady_clock::period::den << std::endl;
	std::cout << "Latency - high_resolution_clock is_steady = " << std::chrono::high_resolution_clock::is_steady << ", num/den = " << (double)std::chrono::high_resolution_clock::period::num << " / " << std::chrono::high_resolution_clock::period::den << std::endl;

	std::chrono::steady_clock::time_point steady_start, steady_end;
	double took_time = 0;
	steady_start = std::chrono::steady_clock::now();

	for(size_t i = 0; i < 1000000; ++i)
		burn_cpu();
	steady_end = std::chrono::steady_clock::now();

	std::cout << "burn_cpu() for each multithread operations took " << std::chrono::duration<double>(steady_end- steady_start).count()*1000 << "nano-sec \n";
	std::cout <<std::endl;

	return 0;
}
