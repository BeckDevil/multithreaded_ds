#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <ctime>
#include <numeric>
#include <cmath>
#include <sstream>
#include <thread>
#include <chrono>
#include <ctime>
#include <mutex>

int get_random(int max)
{
	srand(time(NULL));
	return rand() % max;
}

void execute_thread(int id )
{
	auto nowTime = std::chrono::system_clock::now();
	std::time_t sleepTime = std::chrono::system_clock::to_time_t(nowTime);
	tm myLocalTime = *localtime(&sleepTime);
	
	std::cout << "Thread: " << id << " Sleep Time: " << std::ctime(&sleepTime) << "\n";
	std::cout << "Month: " << myLocalTime.tm_mon << "\n";
	std::cout << "Day: " << myLocalTime.tm_mday << "\n";
	std::cout << "Year: " << myLocalTime.tm_year + 1900 << "\n";
	std::cout << "Hours: " << myLocalTime.tm_hour << "\n";
	std::cout << "Minutes: " << myLocalTime.tm_min << "\n";
	std::cout << "Seconds: " << myLocalTime.tm_sec << "\n";

	std::this_thread::sleep_for(std::chrono::seconds(get_random(3)));
	nowTime = std::chrono::system_clock::now();
	sleepTime = std::chrono::system_clock::to_time_t(nowTime);
	std::cout << "Thread: " << id << " Awake Time: " << std::ctime(&sleepTime) << "\n";
}

std::string get_time()
{
	auto nowTime = std::chrono::system_clock::now();
	std::time_t sleepTime = std::chrono::system_clock::to_time_t(nowTime);
	return std::ctime(&sleepTime);
}

double acctBalance = 100;
std::mutex acctlock;

void get_money(int id, double withdrawl)
{
	std::lock_guard<std::mutex> lock(acctlock);
	std::this_thread::sleep_for(std::chrono::seconds(3));
	std::cout << id << " tries to withdrawl $" << withdrawl << " on " << get_time() << "\n";

	if ((acctBalance - withdrawl) >= 0){
		acctBalance -= withdrawl;
		std::cout << "New Account Balance is $" << acctBalance << "\n";
	}
	else{
		std::cout << "Not Enough Balance, current balance is $" << acctBalance << "\n";
	}
	//acctlock.lock();

	//acctlock.unlock();
}

void find_primes(unsigned int start, unsigned int end, std::vector<unsigned int>& vect)
{
	for(unsigned int x = start; x <= end; x += 2){
		for(unsigned int y = 2; y < x; y++){
			if ((x%y) == 0){
				break;
			}
			else if ((y+1) == x){
				vect.push_back(x);
			}
		}
	}
}

std::mutex vectLock;
std::vector<unsigned int> primeVect;

void find_primes_th(unsigned int start, unsigned int end)
{
	for(unsigned int x = start; x <= end; x += 2){
		for(unsigned int y = 2; y < x; y++){
			if ((x%y) == 0){
				break;
			}
			else if ((y+1) == x){
				vectLock.lock();
				primeVect.push_back(x);
				vectLock.unlock();
			}
		}
	}
}

void find_primes_th2(unsigned int start, unsigned int end, unsigned int numThreads)
{
	std::vector< std::thread > threadVect;
	unsigned int threadSpread = end/numThreads;
	unsigned int newEnd = start + threadSpread;
	for(unsigned int x = 0; x <= numThreads; x++){
		threadVect.emplace_back(find_primes_th, start, newEnd);
		start += threadSpread;
		newEnd += threadSpread;
	}
	for (auto& t : threadVect){
		t.join();
	}
}

int main()
{
//	std::thread th1 (execute_thread, 1);
//	th1.join();
//	std::thread th2 (execute_thread, 2);
//	th2.join();

//	std::thread threads[10];
//	for(int i =0; i < 10; ++i){
//		threads[i] = std::thread(get_money, i, 15);
//	}
//	for(int i = 0; i < 10; ++i){
//		threads[i].join();
//	}i

	std::vector< unsigned int >pVect;
	int startTime = clock();
	find_primes(1, 100000, pVect);
	//for (auto i: pVect)
	//	std::cout << i << "\n";
	int endTime = clock();
	std::cout << "Execution Time: " << (endTime-startTime)/double(CLOCKS_PER_SEC) << std::endl;
	
	startTime = clock();
	find_primes_th2(1, 100000, 2);
	endTime = clock();
	//for (auto i: primeVect)
	//	std::cout << i << "\n";
	endTime = clock();
	std::cout << "Execution Time: " << (endTime-startTime)/double(CLOCKS_PER_SEC) << std::endl;

	return 0;
}
