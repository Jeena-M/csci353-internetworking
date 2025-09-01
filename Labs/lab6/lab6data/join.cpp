// g++ -g -Wall -std=c++11 join.cpp -pthread
// ./a.out
// Note: will not crash, join in order while threads may die in any order
#include <iostream>
#include <thread>
using namespace std;
void server(int k) {
  cout << "Hello, World - " << k << endl;
}
void start_threads() {
  thread thr[10];
  for (int i = 0; i < 10; i++) {
    thr[i] = thread(server, i);
  }
  for (int i = 0; i < 10; i++) {
    thr[i].join();
    cout << "Thread " << i << " joined" << endl;
  }
}
int main(int argc, char *argv[])
{
  start_threads();
  return 0;
}
