// g++ -g -Wall -std=c++11 exit-detach.cpp -pthread
// ./a.out
// Note: will not crash, but some threads won't even get to run, not a good way to go
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
    thr[i].detach();
  }
}
int main(int argc, char *argv[])
{
  start_threads();
  return 0;
}
