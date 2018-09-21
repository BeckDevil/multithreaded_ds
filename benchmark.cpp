#include <functional>
#include "multithreaded_container.h"

struct field_t
{
	int money, time;
	field_t(int m, int t): money(m), time(t) {}
	field_t(): money(0), time(0) {}
};

typedef sf::safe_obj<field_t, sf::spinlock_t> safe_obj_field_t;

// Container -1
std::map <int, field_t> map_global;
std::mutex mtx_map;

// Container -2
sf::safe_ptr< std::map< int, field_t > > safe_map_mutex_global;

// container -3
#ifdef SHARED_MTX
sf::shared_mutex_safe_ptr< std::map< int, field_t > > safe_map_shared_mutex_global;
#endif

// container -4
sf::contfree_safe_ptr<std::map< int, field_t > >safe_map_contfree_global;

// container -5
sf::contfree_safe_ptr<std::map< int, safe_obj_field_t > >safe_map_contfree_rowlock_global;

// container -6
sf::safe_map_partitioned_t < int, safe_obj_field_t> safe_map_part_mutex_global(0, 100000, 100000);

// container -7
sf::safe_map_partitioned_t <int, safe_obj_field_t> safe_map_part_contfree_global(0, 100000, 100000);

enum{ insert_op, delete_op, update_op, read_op };

std::uniform_int_distribution<size_t> percent_distribution(1,100);
sf::safe_ptr< std::vector<double> > safe_vec_max_latency;
static const size_t median_array_size = 1000000;
sf::safe_ptr<std::vector<double> > safe_vec_median_latency;


// benchmark for std::map with naive std::mutex Container - 1 
template<typename T>
void benchmark_std_map(T &test_map, 
		size_t const iterations_count, 
		size_t const percent_write, 
		std::function<void(void)>burn_cpu, 
		const bool measure_latency = false)
{
	const unsigned int seed = (unsigned)std::chrono::system_clock::now().time_since_epoch().count();
	std::default_random_engine generator(seed);
	std::uniform_int_distribution<size_t> index_distribution(0, test_map.size()-1);
	std::chrono::high_resolution_clock::time_point hrc_end, hrc_start = std::chrono::high_resolution_clock::now();
	double max_time = 0;
	std::vector<double> median_arr;

	for(size_t i = 0; i < iterations_count; ++i){
		int const rnd_index = index_distribution(generator);
		bool const write_flag = (percent_distribution(generator) < percent_write);
		int const num_op = (write_flag)? i % 3 : read_op;

		if (measure_latency){
			hrc_end = std::chrono::high_resolution_clock::now();
			const double cur_time = std::chrono::duration<double>(hrc_end - hrc_start).count();
			max_time = std::max(max_time, cur_time);
			if(median_arr.size() == 0) median_arr.resize(std::min(median_array_size, iterations_count));
			if(i < median_arr.size()) median_arr[i] = cur_time;
			hrc_start = std::chrono::high_resolution_clock::now();
		}
		// initialize the lock for mutex
		std::lock_guard<std::mutex> lock(mtx_map);

		switch(num_op){
			case insert_op:{
				burn_cpu();
				test_map.emplace(rnd_index, field_t(rnd_index, rnd_index));
				break;
			}
			case delete_op:{
				burn_cpu();
				size_t erased_elements = test_map.erase(rnd_index);
				break;
			}
			case update_op:{
				auto it = test_map.find(rnd_index);
				if(it != test_map.cend()){
					it->second.money += rnd_index;
					burn_cpu();
				}
				break;
			}
			case read_op:{
				auto it = test_map.find(rnd_index);
				if(it != test_map.cend()){
					volatile int money = it->second.money;
					burn_cpu();
				}
				break;
			}
			default:{
				std::cout << "\n wrong operation \n";
				break;
			}
		}
	}
	safe_vec_max_latency->push_back(max_time);
	safe_vec_median_latency->insert(safe_vec_median_latency->end(), median_arr.begin(), median_arr.end());
}


// benchmark for  Container - 2, 3, 4
template<typename T>
void benchmark_safe_ptr(T &safe_map, 
		size_t const iterations_count, 
		size_t const percent_write, 
		std::function<void(void)>burn_cpu, 
		const bool measure_latency = false)
{
	const unsigned int seed = (unsigned)std::chrono::system_clock::now().time_since_epoch().count();
	std::default_random_engine generator(seed);
	std::uniform_int_distribution<size_t> index_distribution(0, safe_map->size()-1);
	std::chrono::high_resolution_clock::time_point hrc_end, hrc_start = std::chrono::high_resolution_clock::now();
	double max_time = 0;
	std::vector<double> median_arr;

	for(size_t i = 0; i < iterations_count; ++i){
		int const rnd_index = index_distribution(generator);
		bool const write_flag = (percent_distribution(generator) < percent_write);
		int const num_op = (write_flag)? i % 3 : read_op;

		if (measure_latency){
			hrc_end = std::chrono::high_resolution_clock::now();
			const double cur_time = std::chrono::duration<double>(hrc_end - hrc_start).count();
			max_time = std::max(max_time, cur_time);
			if(median_arr.size() == 0) median_arr.resize(std::min(median_array_size, iterations_count));
			if(i < median_arr.size()) median_arr[i] = cur_time;
			hrc_start = std::chrono::high_resolution_clock::now();
		}

		switch(num_op){
			case insert_op:{
				safe_map->emplace(rnd_index, (field_t(rnd_index, rnd_index)));
				burn_cpu();
				break;
			}
			case delete_op:{
				size_t erased_elements = safe_map->erase(rnd_index);
				burn_cpu();
				break;
			}
			case update_op:{
				auto x_safe_map = xlock_safe_ptr(safe_map);
				auto it = x_safe_map->find(rnd_index);
				if(it != x_safe_map->cend()){
					it->second.money += rnd_index;
					burn_cpu();
				}
				break;
			}
			case read_op:{
				auto s_safe_map = slock_safe_ptr(safe_map);
				auto it = s_safe_map->find(rnd_index);
				if(it != s_safe_map->cend()){
					volatile int money = it->second.money;
					burn_cpu();
				}
				break;
			}
			default:{
				std::cout << "\n wrong operation \n";
				break;
			}
		}
	}
	safe_vec_max_latency->push_back(max_time);
	safe_vec_median_latency->insert(safe_vec_median_latency->end(), median_arr.begin(), median_arr.end());
}


// benchmark for Container -5 
template<typename T>
void benchmark_safe_ptr_rowlock(T safe_map, 
		size_t const iterations_count, 
		size_t const percent_write, 
		std::function<void(void)>burn_cpu, 
		const bool measure_latency = false)
{
	const unsigned int seed = (unsigned)std::chrono::system_clock::now().time_since_epoch().count();
	std::default_random_engine generator(seed);
	std::uniform_int_distribution<size_t> index_distribution(0, safe_map->size()-1);
	std::chrono::high_resolution_clock::time_point hrc_end, hrc_start = std::chrono::high_resolution_clock::now();
	double max_time = 0;
	std::vector<double> median_arr;

	for(size_t i = 0; i < iterations_count; ++i){
		int const rnd_index = index_distribution(generator);
		bool const write_flag = (percent_distribution(generator) < percent_write);
		int const num_op = (write_flag)? i % 3 : read_op;

		if (measure_latency){
			hrc_end = std::chrono::high_resolution_clock::now();
			const double cur_time = std::chrono::duration<double>(hrc_end - hrc_start).count();
			max_time = std::max(max_time, cur_time);
			if(median_arr.size() == 0) median_arr.resize(std::min(median_array_size, iterations_count));
			if(i < median_arr.size()) median_arr[i] = cur_time;
			hrc_start = std::chrono::high_resolution_clock::now();
		}

		switch(num_op){
			case insert_op:{
				sf::slock_safe_ptr(safe_map)->find(rnd_index);
				safe_map->emplace(rnd_index, (field_t(rnd_index, rnd_index)));
				burn_cpu();
				break;
			}
			case delete_op:{
				sf::slock_safe_ptr(safe_map)->find(rnd_index);
				size_t erased_elements = safe_map->erase(rnd_index);
				burn_cpu();
				break;
			}
			case update_op:{
				auto s_safe_map = sf::slock_safe_ptr(safe_map);
				auto it = s_safe_map->find(rnd_index);
				if(it != s_safe_map->cend()){
					auto x_field = sf::xlock_safe_ptr(it->second);
					x_field->money += rnd_index;
					burn_cpu();
				}
				break;
			}
			case read_op:{
				auto s_safe_map = sf::slock_safe_ptr(safe_map);
				auto it = s_safe_map->find(rnd_index);
				if(it != s_safe_map->cend()){
					auto s_field = sf::slock_safe_ptr(it->second);
					volatile int money = s_field->money;
					burn_cpu();
				}
				break;
			}
			default:{
				std::cout << "\n wrong operation \n";
				break;
			}
		}
	}
	safe_vec_max_latency->push_back(max_time);
	safe_vec_median_latency->insert(safe_vec_median_latency->end(), median_arr.begin(), median_arr.end());
}


// benchmark for  Container -5 
template<typename T>
void benchmark_map_partitioned(T &safe_map_partitioned, 
		size_t const iterations_count, 
		size_t const percent_write, 
		std::function<void(void)>burn_cpu, 
		const bool measure_latency = false)
{
	std::default_random_engine generator((unsigned)std::chrono::system_clock::now().time_since_epoch().count());
	std::uniform_int_distribution<size_t> index_distribution(0, safe_map_partitioned.size()-1);
	T const& safe_map_ro = safe_map_partitioned;
	typename T::result_vector_t result_vector;
	std::chrono::high_resolution_clock::time_point hrc_end, hrc_start = std::chrono::high_resolution_clock::now();
	double max_time = 0;
	std::vector<double> median_arr;

	for(size_t i = 0; i < iterations_count; ++i){
		int const rnd_index = index_distribution(generator);
		bool const write_flag = (percent_distribution(generator) < percent_write);
		int const num_op = (write_flag)? i % 3 : read_op;

		if (measure_latency){
			hrc_end = std::chrono::high_resolution_clock::now();
			const double cur_time = std::chrono::duration<double>(hrc_end - hrc_start).count();
			max_time = std::max(max_time, cur_time);
			if(median_arr.size() == 0) median_arr.resize(std::min(median_array_size, iterations_count));
			if(i < median_arr.size()) median_arr[i] = cur_time;
			hrc_start = std::chrono::high_resolution_clock::now();
		}

		switch(num_op){
			case insert_op:{
				safe_map_partitioned.emplace(rnd_index, safe_obj_field_t(rnd_index, rnd_index));
				burn_cpu();
				break;
			}
			case delete_op:{
				size_t erased_elements = safe_map_partitioned.erase(rnd_index);
				burn_cpu();
				break;
			}
			case update_op:{
				auto slock_container = safe_map_ro.read_only_part(rnd_index);
				auto it = slock_container->find(rnd_index);
				if(it != slock_container->end()){
					auto x_field = sf::xlock_safe_ptr(it->second);
					x_field->money += rnd_index;
					burn_cpu();
				}
				break;
			}
			case read_op:{
				auto slock_container = safe_map_ro.read_only_part(rnd_index);
				auto it = slock_container->find(rnd_index);
				if(it != slock_container->end()){
					auto s_field = sf::slock_safe_ptr(it->second);
					volatile int money = s_field->money;
					burn_cpu();
				}
				break;
			}
			default:{
				std::cout << "\n wrong operation \n";
				break;
			}
		}
	}
	safe_vec_max_latency->push_back(max_time);
	safe_vec_median_latency->insert(safe_vec_median_latency->end(), median_arr.begin(), median_arr.end());
}

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

	std::cout << "Filling of containers... ";
	try{
		for(size_t i = 0; i < container_size; ++i){
			map_global.emplace(i, field_t(i, i));
			safe_map_mutex_global->emplace(i, field_t(i, i));
			safe_map_contfree_global->emplace(i, field_t(i, i));
#ifdef SHARED_MTX
			safe_map_shared_mutex_global->emplace(i, field_t(i, i));
#endif

			safe_map_contfree_rowlock_global->emplace(i, safe_obj_field_t(field_t(i, i)));
			safe_map_part_mutex_global.emplace(i, safe_obj_field_t(field_t(i, i)));
			safe_map_part_contfree_global.emplace(i, safe_obj_field_t(field_t(i, i)));
		}
	}
	catch(std::runtime_error &e){
		std::cerr << "\n exception - std::runtime_error = " << e.what() << std::endl;
	}
	catch(...){std::cerr << "\n unknown exception \n";}
	std::cout << "filled containers. " << std::endl;

	// check for different percentages of write operation 0 to 90%
	for (size_t percent_write = 0; percent_write <= 90; percent_write += 15)
	{
		std::cout << percent_write << "\t \% of write operations(1/3 insert, delete and update each) " << std::endl;
		std::cout << "                           (1 operation latency, usec) " << std::endl;
		std::cout << "\t\t\t time, sec, \t MOps  \t Median \t Min \t Max " << std::endl;
		std::cout << std::setprecision(3);
		
		
		std::cout << "std::map -1 thread:";
		steady_start = std::chrono::steady_clock::now();
		for(auto &i : vec_thread){
			benchmark_std_map(map_global, iterations_count, percent_write, burn_cpu, measure_latency);
		}
		steady_end = std::chrono::steady_clock::now();
		took_time = std::chrono::duration<double>(steady_end - steady_start).count();
		std::cout << "\t" << took_time << " \t" << (vec_thread.size() * iterations_count/(took_time * 1000000));

		if(measure_latency){
			std::sort(safe_vec_median_latency->begin(), safe_vec_median_latency->end());
			std::cout << " \t " << (safe_vec_median_latency->at(safe_vec_median_latency->size()/2) * 1000000)<< 
				" \t " << (safe_vec_median_latency->at(5) * 1000000) <<
				" \t " << *std::max_element(safe_vec_max_latency->begin(), safe_vec_max_latency->end()) * 1000000;
		}
		std::cout << std::endl;
		safe_vec_max_latency->clear();
		safe_vec_median_latency->clear();
	
		// std::map with std::mutex
		std::cout << "std::map && std::mutex:";
		steady_start = std::chrono::steady_clock::now();
		for(auto &i : vec_thread) i = std::move(std::thread([&](){
			benchmark_std_map(map_global, iterations_count, percent_write, burn_cpu, measure_latency);
		}));
		for(auto &i:vec_thread) i.join();

		steady_end = std::chrono::steady_clock::now();
		took_time = std::chrono::duration<double>(steady_end - steady_start).count();
		std::cout << "\t" << took_time << " \t" << (vec_thread.size() * iterations_count/(took_time * 1000000));

		if(measure_latency){
			std::sort(safe_vec_median_latency->begin(), safe_vec_median_latency->end());
			std::cout << " \t " << (safe_vec_median_latency->at(safe_vec_median_latency->size()/2) * 1000000)<< 
				" \t " << (safe_vec_median_latency->at(5) * 1000000) <<
				" \t " << *std::max_element(safe_vec_max_latency->begin(), safe_vec_max_latency->end()) * 1000000;
		}
		std::cout << std::endl;
		safe_vec_max_latency->clear();
		safe_vec_median_latency->clear();
		
		
		// std::map with std::mutex
		std::cout << "safe_ptr<map, mutex>:";
		steady_start = std::chrono::steady_clock::now();
		for(auto &i : vec_thread) i = std::move(std::thread([&](){
			benchmark_safe_ptr(safe_map_mutex_global, iterations_count, percent_write, burn_cpu, measure_latency);
		}));
		for(auto &i:vec_thread) i.join();

		steady_end = std::chrono::steady_clock::now();
		took_time = std::chrono::duration<double>(steady_end - steady_start).count();
		std::cout << "\t" << took_time << " \t" << (vec_thread.size() * iterations_count/(took_time * 1000000));

		if(measure_latency){
			std::sort(safe_vec_median_latency->begin(), safe_vec_median_latency->end());
			std::cout << " \t " << (safe_vec_median_latency->at(safe_vec_median_latency->size()/2) * 1000000)<< 
				" \t " << (safe_vec_median_latency->at(5) * 1000000) <<
				" \t " << *std::max_element(safe_vec_max_latency->begin(), safe_vec_max_latency->end()) * 1000000;
		}
		std::cout << std::endl;
		safe_vec_max_latency->clear();
		safe_vec_median_latency->clear();

		// std::map with std::mutex
#ifdef SHARED_MTX
		std::cout << "safe_ptr<map, shared>:";
		steady_start = std::chrono::steady_clock::now();
		for(auto &i : vec_thread) i = std::move(std::thread([&](){
			benchmark_safe_ptr(safe_map_shared_mutex_global, iterations_count, percent_write, burn_cpu, measure_latency);
		}));
		for(auto &i:vec_thread) i.join();

		steady_end = std::chrono::steady_clock::now();
		took_time = std::chrono::duration<double>(steady_end - steady_start).count();
		std::cout << "\t" << took_time << " \t" << (vec_thread.size() * iterations_count/(took_time * 1000000));

		if(measure_latency){
			std::sort(safe_vec_median_latency->begin(), safe_vec_median_latency->end());
			std::cout << " \t " << (safe_vec_median_latency->at(safe_vec_median_latency->size()/2) * 1000000)<< 
				" \t " << (safe_vec_median_latency->at(5) * 1000000) <<
				" \t " << *std::max_element(safe_vec_max_latency->begin(), safe_vec_max_latency->end()) * 1000000;
		}
		std::cout << std::endl;
		safe_vec_max_latency->clear();
		safe_vec_median_latency->clear();
#endif


		// std::map with std::mutex
		std::cout << "safe_ptr<map, confree>:";
		steady_start = std::chrono::steady_clock::now();
		for(auto &i : vec_thread) i = std::move(std::thread([&](){
			benchmark_safe_ptr(safe_map_contfree_global, iterations_count, percent_write, burn_cpu, measure_latency);
		}));
		for(auto &i:vec_thread) i.join();

		steady_end = std::chrono::steady_clock::now();
		took_time = std::chrono::duration<double>(steady_end - steady_start).count();
		std::cout << "\t" << took_time << " \t" << (vec_thread.size() * iterations_count/(took_time * 1000000));

		if(measure_latency){
			std::sort(safe_vec_median_latency->begin(), safe_vec_median_latency->end());
			std::cout << " \t " << (safe_vec_median_latency->at(safe_vec_median_latency->size()/2) * 1000000)<< 
				" \t " << (safe_vec_median_latency->at(5) * 1000000) <<
				" \t " << *std::max_element(safe_vec_max_latency->begin(), safe_vec_max_latency->end()) * 1000000;
		}
		std::cout << std::endl;
		safe_vec_max_latency->clear();
		safe_vec_median_latency->clear();


		// std::map with std::mutex
		std::cout << "safe<map, contfree>rowlock:";
		steady_start = std::chrono::steady_clock::now();
		for(auto &i : vec_thread) i = std::move(std::thread([&](){
			benchmark_safe_ptr_rowlock(safe_map_contfree_rowlock_global, iterations_count, percent_write, burn_cpu, measure_latency);
		}));
		for(auto &i:vec_thread) i.join();

		steady_end = std::chrono::steady_clock::now();
		took_time = std::chrono::duration<double>(steady_end - steady_start).count();
		std::cout << "\t" << took_time << " \t" << (vec_thread.size() * iterations_count/(took_time * 1000000));

		if(measure_latency){
			std::sort(safe_vec_median_latency->begin(), safe_vec_median_latency->end());
			std::cout << " \t " << (safe_vec_median_latency->at(safe_vec_median_latency->size()/2) * 1000000)<< 
				" \t " << (safe_vec_median_latency->at(5) * 1000000) <<
				" \t " << *std::max_element(safe_vec_max_latency->begin(), safe_vec_max_latency->end()) * 1000000;
		}
		std::cout << std::endl;
		safe_vec_max_latency->clear();
		safe_vec_median_latency->clear();

		// partitioned mutex
		std::cout << "safe part <mutex>:";
		steady_start = std::chrono::steady_clock::now();
		for(auto &i : vec_thread) i = std::move(std::thread([&](){
			benchmark_map_partitioned(safe_map_part_mutex_global, iterations_count, percent_write, burn_cpu, measure_latency);
		}));
		for(auto &i:vec_thread) i.join();

		steady_end = std::chrono::steady_clock::now();
		took_time = std::chrono::duration<double>(steady_end - steady_start).count();
		std::cout << "\t" << took_time << " \t" << (vec_thread.size() * iterations_count/(took_time * 1000000));

		if(measure_latency){
			std::sort(safe_vec_median_latency->begin(), safe_vec_median_latency->end());
			std::cout << " \t " << (safe_vec_median_latency->at(safe_vec_median_latency->size()/2) * 1000000)<< 
				" \t " << (safe_vec_median_latency->at(5) * 1000000) <<
				" \t " << *std::max_element(safe_vec_max_latency->begin(), safe_vec_max_latency->end()) * 1000000;
		}
		std::cout << std::endl;
		safe_vec_max_latency->clear();
		safe_vec_median_latency->clear();


		// partitioned mutex
		std::cout << "safe part <contfree>:";
		steady_start = std::chrono::steady_clock::now();
		for(auto &i : vec_thread) i = std::move(std::thread([&](){
			benchmark_map_partitioned(safe_map_part_mutex_global, iterations_count, percent_write, burn_cpu, measure_latency);
		}));
		for(auto &i:vec_thread) i.join();

		steady_end = std::chrono::steady_clock::now();
		took_time = std::chrono::duration<double>(steady_end - steady_start).count();
		std::cout << "\t" << took_time << " \t" << (vec_thread.size() * iterations_count/(took_time * 1000000));

		if(measure_latency){
			std::sort(safe_vec_median_latency->begin(), safe_vec_median_latency->end());
			std::cout << " \t " << (safe_vec_median_latency->at(safe_vec_median_latency->size()/2) * 1000000)<< 
				" \t " << (safe_vec_median_latency->at(5) * 1000000) <<
				" \t " << *std::max_element(safe_vec_max_latency->begin(), safe_vec_max_latency->end()) * 1000000;
		}
		std::cout << std::endl;
		safe_vec_max_latency->clear();
		safe_vec_median_latency->clear();
		
	}


	return 0;
}
