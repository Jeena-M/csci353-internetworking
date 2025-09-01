// g++ -g -Wall -std=c++11 join-new.cpp -pthread
// ./a.out
// Note: will not crash, fixed the problem with bad-dyn-create3.cpp using new
#include <iostream>
#include <vector>
#include <thread>
#include <unistd.h>
using namespace std;
vector<thread*> v; // vector of pointers to threads
void server(int k) {
  cout << "Hello, World - " << k << endl;
}
void start_threads() {
  for (int i = 0; i < 10; i++) {
    thread *thr_ptr = new thread(server, i);
    v.push_back(thr_ptr);
  }
}
int main(int argc, char *argv[])
{
  start_threads();
  usleep(5000000);
  int counter = 0;
  while (v.size() > 0) {
    v.at(v.size()-1)->join();
    delete v.at(v.size()-1);
    v.pop_back();
    cout << "Joined " << ++counter << " thread(s)" << endl;
  }
  return 0;
}
