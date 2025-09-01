// g++ -g -Wall -std=c++11 bad-dyn-create2.cpp -pthread
// ./a.out
// Note: will crash in the destructor of thread (because thread is not joined) when the destructor of vector is execute
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
  for (int i = 0; i < 10; i++)
    v.push_back(thread(server, i));
}
int main(int argc, char *argv[])
{
  start_threads();
  usleep(5000000);
  return 0;
}
