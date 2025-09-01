// g++ -g -Wall -std=c++11 detach.cpp -pthread
// ./a.out
// Note: will not crash, if you wait long enough, all threads will finish (not a good way to go)
#include <iostream>
#include <thread>
#include <unistd.h> // for usleep()
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
  usleep(1000000);
  return 0;
}
