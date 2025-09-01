// g++ -g -Wall -std=c++11 first.cpp -pthread
// ./a.out
// Note: this works, but pretty boring.
#include <iostream>
#include <thread>
using namespace std;
void server() {
  // first procedure of child
  cout << "Hello, World" << endl;
}
int main(int argc, char *argv[]) {
  thread thr[10];
  for (int i = 0; i < 10; i++)
    thr[i] = thread(server);
  for (int i = 0; i < 10; i++)
    thr[i].join();
  return 0;
}
