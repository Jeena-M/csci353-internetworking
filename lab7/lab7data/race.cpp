// g++ -g -Wall -std=c++11 race.cpp -pthread
// ./a.out
// Note: run 50 threads in parallel, each thread deposits $10,000 one dollar at a time
// Note: in the end, the account balance is not $500,000
#include <iostream>
#include <thread>
#include <unistd.h>

using namespace std;

class Account
{
  int balance_;
public:
  Account() : balance_(0) { }
  int getBalance() { return balance_; }
  void deposit(int amount) {
    for (int i=0; i < amount; i++) {
      balance_++;
      usleep(1);
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

