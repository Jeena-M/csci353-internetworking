// g++ -g -Wall -std=c++11 fac.cpp
#include <iostream>
using namespace std;
int fac(int n) {
  if (n == 1) {
    return 1;
  }
  int rc = n * fac(n - 1);
  return rc;
}
int main(int argc, char *argv[]) {
  cout << fac(3) << endl;
  return 0;
}
