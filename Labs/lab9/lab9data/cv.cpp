// g++ -g -Wall -std=c++11 cv.cpp -pthread
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <unistd.h> // for usleep();

using namespace std;

queue<int> q;
mutex m;
condition_variable cv;

int total = 0;

void add_work(int w) {
  m.lock();
  q.push(w);
  cv.notify_all();
  total++;
  m.unlock();
}

int wait_for_work() {
  unique_lock<mutex> l(m);

  while (q.empty()) {
    if (total == 100) {
      return (-1);
    }
    cv.wait(l);
  }
  int k = q.front();
  q.pop();

  return k;
}

void consume(int idx)
{
  for (;;) {
    int k = wait_for_work();
    if (k == (-1)) {
      return;
    }
    m.lock();
    cout << "Consumer " << idx << " got " << k << endl;
    m.unlock();
    usleep(1000);
  }
}

void produce(int idx, int base)
{
  for (int i=0; i < 10; i++) {
    add_work(base+i);
    m.lock();
    cout << "Producer " << idx << " added " << base+i << endl;
    m.unlock();
    usleep(1000);
  }
}

int main(int argc, char *argv[])
{
  thread consumer[10], producer[10];
  for (int i=0; i < 10; i++) {
    consumer[i] = thread(consume, i);
    producer[i] = thread(produce, i, i*10);
  }
  for (int i=0; i < 10; i++) {
    producer[i].join();
    m.lock();
    cout << "Joined producer " << i << endl;
    m.unlock();
    consumer[i].join();
    m.lock();
    cout << "Joined consumer " << i << endl;
    m.unlock();
  }
  return 0;
}

