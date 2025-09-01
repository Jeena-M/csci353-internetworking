// g++ -g -Wall -std=c++11 join-dyn.cpp -pthread
// ./a.out
// Note: this code works, if you don't use a local variable, you can add dynamically created threads to a global vector
// Note: this code demonstrate how to iterate through a vector from the back
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
  int counter = 0;
  while (v.size() > 0) {
    v.at(v.size()-1).join();
    v.pop_back();
    cout << "Joined " << ++counter << " thread(s)" << endl;
  }
  return 0;
}
