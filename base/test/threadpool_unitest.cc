#include "../threadpool.h"
#include "../count_down_latch.h"
#include "../current_thread.h"

#include <boost/bind.hpp>
#include <stdio.h>
#include <unistd.h>  // usleep
#include <iostream>

void print() {
  printf("tid=%d\n", kaycc::currentthread::tid());
}

void printString(const std::string& str) {
  std::cout << str << std::endl;
  usleep(100*1000);
}

void test(int maxSize) {
  std::cout << "Test ThreadPool with max queue size = " << maxSize;
  kaycc::ThreadPool pool("MainThreadPool");
  pool.setMaxQueueSize(maxSize);
  pool.start(5);
  std::cout << "Adding" << std::endl;

  pool.run(print);
  pool.run(print);
  for (int i = 0; i < 100; ++i)
  {
    char buf[32];
    snprintf(buf, sizeof buf, "task %d", i);
    pool.run(boost::bind(printString, std::string(buf)));
  }

  std::cout << "Done" << std::endl;

  kaycc::CountDownLatch latch(1);
  pool.run(boost::bind(&kaycc::CountDownLatch::countDown, &latch));
  latch.wait();
  pool.stop();
}

int main() {
  test(0);
  test(1);
 // test(5);
 // test(10);
  //test(50);
}