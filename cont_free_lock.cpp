#include <iostream>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <unordered_map>
#include <cassert>

namespace sf
{
	//contention free shared mutex
	template< unsigned contention_free_count = 36, bool shared_flag = false >
		class contention_free_shared_mutex
		{
			std::atomic< bool > want_x_lock;

			struct cont_free_flag_t{
				char tmp[60];
				std::atomic< int > value;
				cont_free_flag_t(){
					value = 0;
				}
			};
			typedef std::array< cont_free_flag_t, contention_free_count> array_slock_t;

			const std::shared_ptr< array_slock_t > shared_locks_array_ptr; // 0- unregistered, 1 registered and free, 2- busy
			char avoid_falsesharing_1[64];

			array_slock_t &shared_locks_array;
			char avoid_falsesharing_2[64];

			int recursive_xlock_count;

			enum index_op_t{ unregister_thread_op, get_index_op, register_thread_op };


			typedef std::thread::id thread_id_t;
			std::atomic< std::thread::id > owner_thread_id;
			std::thread::id get_fast_this_thread_id(){
				return std::this_thread::get_id();
			}

			struct unregister_t{
				int thread_index;
				std::shared_ptr< array_slock_t > array_slock_ptr;
				unregister_t(int index, std::shared_ptr< array_slock_t > const& ptr): thread_index(index), array_slock_ptr(ptr) {}
				unregister_t(unregister_t &&src): thread_index(src.thread_index), array_slock_ptr(src.array_slock_ptr) {}
				~unregister_t(){ if (array_slock_ptr.use_count() > 0) (*array_slock_ptr)[thread_index].value--; }
			};

			int get_or_set_index(index_op_t index_op = get_index_op, int set_index = -1)
			{
				thread_local static std::unordered_map< void*, unregister_t> thread_local_index_hashmap;
				// get thread index
				auto it = thread_local_index_hashmap.find(this);
				if(it != thread_local_index_hashmap.cend()){
					set_index = it->second.thread_index;
				}

				if(index_op == unregister_thread_op){
					if (shared_locks_array[set_index].value == 1) // if it isn't shared lock now
						thread_local_index_hashmap.erase(this);
					else
						return -1;
				}
				else if (index_op == register_thread_op){
					thread_local_index_hashmap.emplace(this, unregister_t(set_index, shared_locks_array_ptr));

					// remove info about deleted contfree-mutexes
					for( auto it = thread_local_index_hashmap.begin(), ite = thread_local_index_hashmap.end(); it != ite; ){
						if (it->second.array_slock_ptr->at(it->second.thread_index).value < 0)
							it = thread_local_index_hashmap.erase(it);
						else
							++it;
					}
				}
				return set_index;
			}

		public:
			contention_free_shared_mutex():
				shared_locks_array_ptr(std::make_shared< array_slock_t >()), shared_locks_array(*shared_locks_array_ptr), want_x_lock(false), recursive_xlock_count(0), owner_thread_id(thread_id_t()) {}

			~contention_free_shared_mutex(){
				for(auto &i : shared_locks_array)
					i.value = -1;
			}

			bool unregister_thread(){
				return get_or_set_index(unregister_thread_op) >= 0;
			}

			int register_thread(){
				int cur_index = get_or_set_index();

				if(cur_index == -1){
					if(shared_locks_array_ptr.use_count() <= (int)shared_locks_array.size()){
						for(size_t i = 0; i < shared_locks_array.size(); ++i){
							int unregistered_value = 0;
							if(shared_locks_array[i].value == 0){
								if (shared_locks_array[i].value.compare_exchange_strong(unregistered_value, 1)){
									cur_index = i;
									get_or_set_index(register_thread_op, cur_index);
									break;
								}
							}
						}
					}
				}
			}
			// lock function for shared read_lock
			void lock_shared()
			{
				int const register_index = register_thread();

				if(register_index >= 0){
					int recursion_depth = shared_locks_array[register_index].value.load(std::memory_order_acquire);
					assert(recursion_depth >= 1);

					if (recursion_depth > 1){
						shared_locks_array[register_index].value.store(recursion_depth+1, std::memory_order_release);
					}
					else{
						shared_locks_array[register_index].value.store(recursion_depth+1, std::memory_order_seq_cst);
						while(want_x_lock.load(std::memory_order_seq_cst)){
							shared_locks_array[register_index].value.store(recursion_depth, std::memory_order_seq_cst);
							for(volatile size_t i =0; want_x_lock.load(std::memory_order_seq_cst); ++i){
								if (i%100000 == 0) std::this_thread::yield();
							}
							shared_locks_array[register_index].value.store(recursion_depth+1, std::memory_order_seq_cst);
						}
					}

				}
				else{
					if (owner_thread_id.load(std::memory_order_acquire) != get_fast_this_thread_id()) {
						size_t i = 0;
						for (bool flag = false; !want_x_lock.compare_exchange_weak(flag, true, std::memory_order_seq_cst); flag = false)
							if (++i % 100000 == 0) std::this_thread::yield();
						owner_thread_id.store(get_fast_this_thread_id(), std::memory_order_release);
					}
					++recursive_xlock_count;
				}
			}

			// unlock function for shared read_lock
			void unlock_shared(){
				int const register_index = get_or_set_index();

				if(register_index >= 0){
					int const recursion_depth = shared_locks_array[register_index].value.load(std::memory_order_acquire);
					assert(recursion_depth > 1);

					shared_locks_array[register_index].value.store(recursion_depth-1, std::memory_order_release);
				}
				else
				{
					if (--recursive_xlock_count == 0){
						owner_thread_id.store(decltype(owner_thread_id)(), std::memory_order_release);
						want_x_lock.store(false, std::memory_order_release);
					}
				}
			}
			// lock function for exclusive write_lock
			void lock()
			{
				// forbidden upgrade S-lock to X-lock 
				int const register_index = get_or_set_index();
				if (register_index >= 0){
					assert(shared_locks_array[register_index].value.load(std::memory_order_acquire) == 1);
				}

				if (owner_thread_id.load(std::memory_order_acquire) != get_fast_this_thread_id()){
					size_t i = 0;

					for(bool flag = false; !want_x_lock.compare_exchange_weak(flag, true, std::memory_order_seq_cst); flag = false)
						if(++i %1000000 == 0) std::this_thread::yield();

					owner_thread_id.store(get_fast_this_thread_id(), std::memory_order_release);

					for(auto &i : shared_locks_array)
						while(i.value.load(std::memory_order_seq_cst) > 1);
				}
				++recursive_xlock_count;
			}

			// unlock function for exclusive write_lock
			void unlock()
			{
				assert(recursive_xlock_count > 0);
				if(--recursive_xlock_count == 0){
					owner_thread_id.store(decltype(owner_thread_id)(), std::memory_order_release);
					want_x_lock.store(false, std::memory_order_release);
				}
			}

		}; //class contention_free_shared_mutex
	template< typename mutex_t>
		struct shared_lock_guard
		{
			mutex_t &ref_mtx;
			shared_lock_guard(mutex_t &mtx):ref_mtx(mtx){ref_mtx.lock_shared();}
			~shared_lock_guard() {ref_mtx.unlock_shared();}
		};

	using default_contention_free_shared_mutex = contention_free_shared_mutex<>;
	// --------------------------------------------------

}// namespace sf

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

int main()
{
	int a = 0;
	int b = 0;
	sf::contention_free_shared_mutex<> s_m;

	//20- threads
	std::vector<std::thread> vec_thread(20);
	for(auto &i: vec_thread)
		i = std::move(std::thread([&](){func(s_m, a, b);}));
	
	for(auto &i: vec_thread)
		i.join();
	
	std::cout << "a = " << a << " , b = " << b << std::endl;
	getchar();

	return 0;
}
