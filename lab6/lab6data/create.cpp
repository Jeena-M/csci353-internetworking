// g++ -g -Wall -std=c++11 create.cpp -pthread
// ./a.out
// Note: same code as "first-arg.cpp", cannot control order of execution; printout may get messed up
#include <iostream>
#include <thread>
using namespace std;
void server(int k) {
  // first procedure of child
  cout << "Hello, World - " << k << endl;
}
int main(int argc, char *argv[])
{
  thread thr[10];
  for (int i = 0; i < 10; i++)
    thr[i] = thread(server, i);
  for (int i = 0; i < 10; i++)
    thr[i].join();
  return 0;
}
