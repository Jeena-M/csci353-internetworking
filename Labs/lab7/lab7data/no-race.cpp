// g++ -g -Wall -std=c++11 no-race.cpp -pthread
// ./a.out
// Note: run 50 threads in parallel, each thread deposits $10,000 one dollar at a time
// Note: the account balance is always $500,000
#include <iostream>
#include <thread>
#include <mutex>

using namespace std;

class Account
{
  int balance_;
  mutex m_;
public:
  Account() : balance_(0) { }
  int getBalance() { return balance_; }
  void deposit(int amount) {
    for (int i=0; i < amount; i++) {
      m_.lock();
      balance_++;
      m_.unlock();
    }
  }
};

Account acc;

void child() {
  acc.deposit(10000);
}

void start_threads() {
  int n = 50;
  thread thr[n];
  for (int i = 0; i < n; i++) {
    thr[i] = thread(child);
  }
  for (int i = 0; i < n; i++) {
    thr[i].join();
  }
  cout << acc.getBalance() << endl;
}

int main(int argc, char *argv[])
{
  start_threads();
  return 0;
}

