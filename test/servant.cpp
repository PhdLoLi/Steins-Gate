/**
 * servant.cpp
 * Created on: May 30, 2016
 * Author: Lijing Wang
 */

#include "view.hpp"
//#include <boost/bind.hpp>

#include "captain.hpp"

//#include "threadpool.hpp" 
#include <boost/thread/mutex.hpp>
#include <fstream>
//#include <boost/filesystem.hpp>
#include <chrono>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <algorithm>
//#include <thread>

namespace sgate {

//using namespace std::placeholders;
using namespace std;
//using namespace boost::filesystem;
//using namespace boost::threadpool;


  
class Servant {
 public:
  Servant(node_id_t my_id, int node_num, int total) 
    : my_id_(my_id), node_num_(node_num), thr_counter_(0), total_(total), done_(false) {

    std::string config_file = "config/localhost-" + to_string(node_num_) + ".yaml";

    // init view_ for one captain_
    view_ = new View(my_id_, config_file);
    view_->print_host_nodes();
    
    my_name_ = view_->hostname();

    // init callback
//    callback_latency_t call_latency = boost::bind(&Servant::count_latency, this, _1, _2, _3);
    callback_full_t callback_full = boost::bind(&Servant::count_exe_latency, this, _1, _2, _3);
    captain_ = new Captain(*view_, 1);

//    captain_->set_callback(call_latency);
//    captain_->set_callback(callback);
    captain_->set_callback(callback_full);

//    my_pool_ = new pool(win_size);

  }

  ~Servant() {
  }

  void wait() {
    boost::unique_lock<boost::mutex> lock(done_mut_);
    while(!done_)
    {
        done_cond_.wait(lock);
    }
  }

  void count_exe_latency(slot_id_t slot_id, PropValue& prop_value, node_id_t node_id) {
//    LOG_INFO("count_latency triggered! slot_id : %llu", slot_id);
    thr_mut_.lock();
    thr_counter_++;
    if (thr_counter_ == total_) {
      LOG_INFO("count_latency triggered! slot_id : %llu Finished!", slot_id);
      {
        boost::lock_guard<boost::mutex> lock(done_mut_);
        done_ = true;
      }
      done_cond_.notify_one();
    }
    thr_mut_.unlock(); 
  }
  
  void count_latency(slot_id_t slot_id, PropValue& prop_value, int try_time) {

  }

  std::string my_name_;
  node_id_t my_id_;
  node_id_t node_num_;
  int total_;

  Captain *captain_;
  View *view_;
  slot_id_t thr_counter_;
  boost::mutex thr_mut_;

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
 

  if (argc < 4) {
    std::cerr << "Usage: Node_ID Node_Num Total_Num" << std::endl;
    return 0;
  }

  node_id_t my_id = stoul(argv[1]); 
  int node_num = stoi(argv[2]);
  int total = stoi(argv[3]);
  
  Servant servant(my_id, node_num, total);

  LOG_INFO("I'm waiting ... ");
  servant.wait();
  sleep(2);
  LOG_INFO("Servant ALL DONE!");
  return 0;
}


} // namespace sgate

int main(int argc, char** argv) {
  return sgate::main(argc, argv);
}
