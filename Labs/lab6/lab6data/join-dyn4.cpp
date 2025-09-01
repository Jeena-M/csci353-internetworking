// g++ -g -Wall -std=c++11 join-dyn4.cpp -pthread
// ./a.out
// Note: will not crash, fixed the problem with bad-dyn-create4.cpp using move()
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
  int counter = 0;
  while (v.size() > 0) {
    v.at(v.size()-1).join();
    v.pop_back();
    cout << "Joined " << ++counter << " thread(s)" << endl;
  }
  return 0;
}
