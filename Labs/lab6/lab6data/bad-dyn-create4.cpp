// g++ -g -Wall -std=c++11 bad-dyn-create4.cpp -pthread
// Note: will crash in the destructor of thread (because thread is not joined) when the destructor of vector is execute
// Note: basically the same code as bad-dyn-create2.cpp
#include <iostream>
#include <vector>
#include <thread>
#include <unistd.h>
using namespace std;
vector<thread> v;
void server(int k) {
  cout << "Hello, World - " << k << endl;
}
void start_threads() {
  for (int i = 0; i < 10; i++) {
    thread thr = thread(server, i);
    v.push_back(move(thr));
  }
}
int main(int argc, char *argv[])
{
  start_threads();
  usleep(5000000);
  return 0;
}
