// g++ -g -Wall -std=c++11 first-arg-class.cpp -pthread
// ./a.out
// Note: cannot control order of execution; printout may get messed up
#include <iostream>
#include <thread>
using namespace std;
class Foo {
public:
  thread thr[10];
  static void server(Foo *self, int k) {
    // first procedure of child
    cout << "Hello, I am thread " << k << endl;
  }
};
int main(int argc, char *argv[])
{
  Foo foo;
  for (int i = 0; i < 10; i++)
    foo.thr[i] = thread(Foo::server, &foo, i);
  for (int i = 0; i < 10; i++)
    foo.thr[i].join();
  return 0;
}
