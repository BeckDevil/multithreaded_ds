#include "multithreaded_container.h"

template <typename T>
void func(T &s_m, int &a, int &b)
{
	for (size_t i = 0; i < 100000; ++i)
	{
		// x-lock for modification
		s_m.lock();
		a++;
		b++;
		s_m.unlock();

		// s-lock for reading
		s_m.lock_shared();
		assert(a == b);
		s_m.unlock_shared();
	}
}

safe_map_partitioned_t<std::string, std::pair<std::string, int> > safe_map_strings_global{ "a", "f", "k", "p", "u" };

void func_partition(decltype(safe_map_strings_global) safe_map_strings)
{
	for(size_t i = 0; i < 10000; ++i){
		(*safe_map_strings.part("apple"))["apple"].first = "fruit";
		(*safe_map_strings.part("apple"))["apple"].second++;
		(*safe_map_strings.part("potato"))["potato"].first = "vegetable";
		(*safe_map_strings.part("potato"))["potato"].second++;
	}
	auto const& readonly_safe_map_string = safe_map_strings;

	std::cout << "potato is " << readonly_safe_map_string.part("potato")->at("potato").first << " " << readonly_safe_map_string.part("potato")->at("potato").second << std::endl;
	std::cout << "apple is " << readonly_safe_map_string.part("apple")->at("apple").first << " " << readonly_safe_map_string.part("apple")->at("apple").second << std::endl;

}

int main()
{/*
	int a = 0;
	int b = 0;
	sf::contention_free_shared_mutex<> s_m;

	//20- threads
	std::vector<std::thread> vec_thread(2);
	for(auto &i: vec_thread)
		i = std::move(std::thread([&](){func(s_m, a, b);}));
	
	for(auto &i: vec_thread)
		i.join();
	
	std::cout << "a = " << a << " , b = " << b << std::endl;
	getchar();
*/
	std::vector<std::thread> vec_thread(10);
	for(auto &i:vec_thread){
		i = std::move(std::thread([&](){func_partition(safe_map_strings_global);}));
	}
	for(auto &i:vec_thread){
		i.join();
	}

	safe_map_strings_global.erase_lower_upper("a", "c");

	// get all keys- values: a-z
	decltype(safe_map_strings_global)::result_vector_t all_range;
	safe_map_strings_global.get_range_lower_upper("a", "z", all_range);
	for(auto &i: all_range)
		std::cout << i.first << " ==> " << i.second.first << ", " << i.second.second << std::endl;

	std::cout << "end";
	int b; std::cin >> b;
	return 0;
}
