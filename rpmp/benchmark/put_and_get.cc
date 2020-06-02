/*
 * Filename: /mnt/spark-pmof/tool/rpmp/benchmark/allocate_perf.cc
 * Path: /mnt/spark-pmof/tool/rpmp/benchmark
 * Created Date: Friday, December 20th 2019, 8:29:23 am
 * Author: root
 *
 * Copyright (c) 2019 Intel
 */

#include <string.h>
#include <cstdlib>
#include <thread>  // NOLINT
#include "Config.h"
#include "pmpool/Base.h"
#include "pmpool/client/PmPoolClient.h"

uint64_t timestamp_now() {
  return std::chrono::high_resolution_clock::now().time_since_epoch() /
         std::chrono::milliseconds(1);
}

char str[1048576];
int numReqs = 2048;

bool comp(char* str, char* str_read, uint64_t size) {
  auto res = memcmp(str, str_read, size);
  if (res != 0) {
    fprintf(stderr,
            "** strcmp is %d, read res is not aligned with wrote. **\nreaded "
            "content is \n",
            res);
    for (int i = 0; i < 100; i++) {
      fprintf(stderr, "%X ", *(str_read + i));
    }
    fprintf(stderr, " ...\nwrote content is \n");
    for (int i = 0; i < 100; i++) {
      fprintf(stderr, "%X ", *(str + i));
    }
    fprintf(stderr, " ...\n");
  }
  return res == 0;
}

void get(int map_id, int start, int end, std::shared_ptr<PmPoolClient> client) {
  int count = start;
  while (count < end) {
    std::string key =
        "block_" + std::to_string(map_id) + "_" + std::to_string(count++);
    char str_read[1048576];
    client->begin_tx();
    client->get(key, str_read, 1048576);
    client->end_tx();
    if (comp(str, str_read, 1048576) == false) {
      throw;
    }
  }
}

void put(int map_id, int start, int end, std::shared_ptr<PmPoolClient> client) {
  int count = start;
  while (count < end) {
    std::string key =
        "block_" + std::to_string(map_id) + "_" + std::to_string(count++);
    client->begin_tx();
    client->put(key, str, 1048576);
    client->end_tx();
  }
}

int main(int argc, char** argv) {
  /// initialize Config class
  std::shared_ptr<Config> config = std::make_shared<Config>();
  CHK_ERR("config init", config->init(argc, argv));

  char temp[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K',
                 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V',
                 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f'};
  for (int i = 0; i < 1048576 / 32; i++) {
    memcpy(str + i * 32, temp, 32);
  }

  int threads = config->get_num_threads();
  int map_id = config->get_map_id();
  numReqs = config->get_num_reqs();
  std::string host = config->get_ip();
  std::string port = config->get_port();

  std::cout << "=================== Put and get ======================="
            << std::endl;
  std::cout << "RPMP server is " << host << ":" << port << std::endl;
  std::cout << "Total Num Requests is " << numReqs << std::endl;
  std::cout << "Total Num Threads is " << threads << std::endl;
  std::cout << "Block key pattern is "
            << "block_" << map_id << "_*" << std::endl;

  auto client = std::make_shared<PmPoolClient>(host, port);
  client->init();
  std::cout << "start put." << std::endl;
  int start = 0;
  int step = numReqs / threads;
  std::vector<std::shared_ptr<std::thread>> threads_1;
  uint64_t begin = timestamp_now();
  for (int i = 0; i < threads; i++) {
    auto t =
        std::make_shared<std::thread>(put, map_id, start, start + step, client);
    threads_1.push_back(t);
    start += step;
  }
  for (auto thread : threads_1) {
    thread->join();
  }
  uint64_t end = timestamp_now();
  std::cout << "[block_" << map_id << "_*]"
            << "pmemkv put test: 1048576 "
            << " bytes test, consumes " << (end - begin) / 1000.0
            << "s, throughput is " << numReqs / ((end - begin) / 1000.0)
            << "MB/s" << std::endl;

  std::cout << "start get." << std::endl;
  std::vector<std::shared_ptr<std::thread>> threads_2;
  begin = timestamp_now();
  start = 0;
  for (int i = 0; i < threads; i++) {
    auto t =
        std::make_shared<std::thread>(get, map_id, start, start + step, client);
    threads_2.push_back(t);
    start += step;
  }
  for (auto thread : threads_2) {
    thread->join();
  }
  end = timestamp_now();
  std::cout << "[block_" << map_id << "_*]"
            << "pmemkv get test: 1048576 "
            << " bytes test, consumes " << (end - begin) / 1000.0
            << "s, throughput is " << numReqs / ((end - begin) / 1000.0)
            << "MB/s" << std::endl;

  client.reset();
  return 0;
}