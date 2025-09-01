// g++ -g -Wall -std=c++11 bad-create.cpp -pthread
// ./a.out
// Note: will crash in the destructor of thread because thread is not joined
#include <iostream>
#include <thread>
#include <unistd.h>
using namespace std;
void server(int k) {
  cout << "Hello, World - " << k << endl;
}
void start_threads() {
  thread thr[10];
  for (int i = 0; i < 10; i++)
    thr[i] = thread(server, i);
}
int main(int argc, char *argv[]) {
  start_threads();
  usleep(5000000);
  return 0;
}
