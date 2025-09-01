// g++ -g -Wall -std=c++11 bad-dyn-create3.cpp -pthread
// Note: won't even compile, cryptic compiler error message
// Note: cannot ask a global vector to hold a local thread variable
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
    v.push_back(thr);
  }
}
int main(int argc, char *argv[])
{
  start_threads();
  usleep(5000000);
  return 0;
}
