// g++ -g -Wall -std=c++11 join-shared.cpp -pthread
// ./a.out
// Note: will not crash, fixed the problem with bad-dyn-create3.cpp using shared pointers
// Note: when you create an object and assign it to a local variable and you want to add it to a global list, using a shared pointer is highly recommended
#include <iostream>
#include <vector>
#include <thread>
#include <unistd.h>
using namespace std;
vector<shared_ptr<thread>> v; // vector of shared pointers to threads
void server(int k) {
  cout << "Hello, World - " << k << endl;
}
void start_threads() {
  for (int i = 0; i < 10; i++) {
    shared_ptr<thread> thr_ptr = make_shared<thread>(thread(server, i));
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
    v.pop_back();
    cout << "Joined " << ++counter << " thread(s)" << endl;
  }
  return 0;
}
