/**
 * master.cpp
 * Created on: May 30, 2016
 * Author: Lijing Wang
 */

#include "view.hpp"
//#include <boost/bind.hpp>

#include "captain.hpp"

//#include "threadpool.hpp" 
//#include <boost/thread/mutex.hpp>
#include <fstream>
//#include <boost/filesystem.hpp>
#include <chrono>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <algorithm>
//#include <thread>
#include <fstream>
//#include <boost/thread/condition.hpp>



namespace sgate {

using namespace std;
//using namespace boost::filesystem;

  
class Master {
 public:
  Master(node_id_t my_id, int node_num, int value_size, int win_size, int total) 
    : my_id_(my_id), node_num_(node_num), 
      value_size_(value_size), win_size_(win_size), total_(total),
      commit_counter_(0), thr_counter_(0), starts_(total), periods_(total), trytimes_(total), done_(false) {

    std::string config_file = "config/localhost-" + to_string(node_num_) + ".yaml";

    // init view_ for one captain_
    view_ = new View(my_id_, config_file);
    view_->print_host_nodes();
    
    my_name_ = view_->hostname();

    // init callback
    callback_latency_t call_latency = boost::bind(&Master::count_latency, this, _1, _2, _3);
//    callback_full_t callback_full = bind(&Master::count_exe_latency, this, _1, _2, _3);
    captain_ = new Captain(*view_, win_size_);

    captain_->set_callback(call_latency);
//    captain_->set_callback(callback);
//    captain_->set_callback(callback_full);

//    my_pool_ = new pool(win_size);

  }

  ~Master() {
  }

  void wait() {
    boost::unique_lock<boost::mutex> lock(done_mut_);
    while(!done_)
    {
        done_cond_.wait(lock);
    }
  }

  void commit_thread(std::string &value) {
    captain_->commit(value);
  } 

  void start_commit() {

    start_ = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < win_size_; i++) {
//      std::string value = "Commiting Value Time_" + std::to_string(i) + " from " + view_->hostname();
      counter_mut_.lock();
      starts_[commit_counter_] = std::chrono::high_resolution_clock::now(); 
      std::string value = "Commiting Value Time_" + std::to_string(commit_counter_) + " from " + view_->hostname();
      commit_counter_++;
      counter_mut_.unlock();
//      LOG_INFO(" +++++++++++ ZERO Init Commit Value: %s +++++++++++", value.c_str());
      captain_->commit(value);
//      LOG_INFO(" +++++++++++ ZERO FINISH Commit Value: %s +++++++++++", value.c_str());

//      LOG_INFO("COMMIT DONE***********************************************************************");
    }
  }


  void count_exe_latency(slot_id_t slot_id, PropValue& prop_value, node_id_t node_id) {
  
  }
  
  void count_latency(slot_id_t slot_id, PropValue& prop_value, int try_time) {
  
    if (prop_value.has_cmd_type()) {
      counter_mut_.lock();
      commit_counter_++;
      counter_mut_.unlock();
      LOG_INFO("count_latency triggered! but this is a command slot_id : %llu commit_counter_ : %llu ", slot_id, commit_counter_);
      return;
    }
//    LOG_INFO("count_latency triggered! slot_id : %llu", slot_id);

    auto finish = std::chrono::high_resolution_clock::now();

    value_id_t value_id = prop_value.id() >> 16;
//    std::cout << "value_id " << value_id << std::endl;

    // record latency for value_id - 1
    periods_[value_id - 1] = std::chrono::duration_cast<std::chrono::nanoseconds>
                     (finish-starts_[value_id - 1]).count();
    trytimes_[value_id - 1] = try_time;

    thr_mut_.lock();
    thr_counter_++;
    if (thr_counter_ % 10000 == 0) {
      auto finish = std::chrono::high_resolution_clock::now();
      uint64_t period = std::chrono::duration_cast<std::chrono::milliseconds>(finish-start_).count();
      int throughput = 10000 * 1000 / period;
      LOG_INFO("callback -- counter:%d milliseconds:%llu throughput:%d", thr_counter_, period, throughput);
      LOG_INFO("periods[%d] = %d thr_counter = %d\n", value_id - 1, periods_[value_id - 1], thr_counter_);
      throughputs_.push_back(throughput);
      start_ = std::chrono::high_resolution_clock::now();
    }

    if (thr_counter_ == total_) {
      LOG_INFO("Writing File Now!");

      std::string thr_name = "results/t_" + std::to_string(node_num_) + "_" + std::to_string(win_size_) + ".txt";
      file_throughput_.open(thr_name);
      for (int i = 0; i < throughputs_.size(); i++) {
        file_throughput_ << throughputs_[i] << "\n";
      }
      file_throughput_.close();

      std::string lat_name = "results/l_" + std::to_string(node_num_) + "_" + std::to_string(win_size_) + ".txt";
      file_latency_.open(lat_name);
      for (int i = 0; i < periods_.size(); i++) {
        file_latency_ << periods_[i] << "\n";
      }
      file_latency_.close();

      LOG_INFO("Writing File Finished!");

      {
        boost::lock_guard<boost::mutex> lock(done_mut_);
        done_ = true;
      }
      done_cond_.notify_one();
    }
    thr_mut_.unlock();

    counter_mut_.lock();
     

    if (commit_counter_ < total_) {
      std::string value = "Commiting Value Time_" + std::to_string(commit_counter_) + " from " + my_name_;
      starts_[commit_counter_] = std::chrono::high_resolution_clock::now();
      commit_counter_++;
      counter_mut_.unlock();
  //    LOG_INFO("++++ I just Commit Value: %s ++++", value.c_str());
//      boost::thread commit_first(bind(&Master::commit_thread, this, value));
//      LOG_INFO(" +++++++++++ Init Commit Value: %s +++++++++++", value.c_str());
//      my_pool_->schedule(bind(&Master::commit_thread, this, value));
      captain_->commit(value);
//      LOG_INFO(" +++++++++++ FINISH Commit Value: %s +++++++++++", value.c_str());
//      std::cout << "master want to commit Value Finish: " << value << std::endl;
    } else {
//      LOG_INFO("commit_counter %d Finish!!!!!!!!!!!!!\n", commit_counter_);
      commit_counter_++;
      counter_mut_.unlock();
    } 

  }
  
  

  std::string my_name_;
  node_id_t my_id_;
  node_id_t node_num_;
  int value_size_;
  int win_size_;
  
  Captain *captain_;
  View *view_;

  int total_;
  slot_id_t commit_counter_;
  slot_id_t thr_counter_;

  
  boost::mutex counter_mut_;
  boost::mutex thr_mut_;
  
  std::vector<std::chrono::high_resolution_clock::time_point> starts_;
  std::vector<uint64_t> periods_;
  std::vector<uint64_t> throughputs_;
  std::vector<int> trytimes_;
  
  std::chrono::high_resolution_clock::time_point start_;
//  std::string filename = "values/client_" +  std::to_string(node_id);
//  std::cout << "FileName " << filename << std::endl;
  std::ofstream file_throughput_;
  std::ofstream file_latency_;

  boost::condition_variable done_cond_;
  boost::mutex done_mut_;
  bool done_;
};



static void sig_int(int num) {
  std::cout << "Control + C triggered! " << std::endl;
  exit(num);
}  

int main(int argc, char** argv) {
  signal(SIGINT, sig_int);
 

  if (argc < 6) {
    std::cerr << "Usage: Node_ID Node_Num Value_Size Window_Size Total_time" << std::endl;
    return 0;
  }

  node_id_t my_id = stoul(argv[1]); 
  int node_num = stoi(argv[2]);
  int value_size = stoi(argv[3]);
  int win_size = stoi(argv[4]);
  int total = stoi(argv[5]);
  
  Master master(my_id, node_num, value_size, win_size, total);
  master.start_commit();

  LOG_INFO("I'm waiting ... ");
  master.wait();
  sleep(2);
  LOG_INFO("Master ALL DONE!");

  return 0;
}



} // namespace sgate

int main(int argc, char** argv) {
  return sgate::main(argc, argv);
}
