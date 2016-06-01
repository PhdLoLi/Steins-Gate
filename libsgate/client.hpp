/**
 * client.hpp
 * Created on: May 31, 2016
 * Author: Lijing 
 */
#pragma once

#include "internal_types.hpp"
#include <boost/thread.hpp>
#include <boost/bind.hpp>
#include <boost/thread/mutex.hpp>

#include <fstream>
#include <unistd.h>
#include <string>
#include <stdlib.h>
#include <chrono>

namespace sgate {

class Commo;
class Client {
 public:
  Client(node_id_t node_id, int commit_win, int ratio, node_id_t read_node);
  ~Client();
  /**
   * set commo_handler 
   */
  void set_commo(Commo *commo) {
    commo_ = commo;
  } 
  void start_commit();
  void handle_reply(MsgAckCommit *);

 private:

  /**
   * Return MsgCommit* 
   */
  MsgCommit *msg_commit(slot_id_t slot_id, bool read, std::string &data); 

  Commo *commo_;
  int node_id_;
  int com_win_;
  int ratio_;
  node_id_t read_node_;
  node_id_t master_id_;

  bool write_or_read_; // write:false read:true

  slot_id_t commit_counter_;
  slot_id_t rand_counter_;
  slot_id_t thr_counter_;

  boost::mutex counter_mut_;
  boost::mutex thr_mut_;
  boost::mutex master_mut_;
  
  std::vector<uint64_t> periods_;
  std::vector<uint64_t> throughputs_;
  std::vector<int> trytimes_;
  std::vector<std::chrono::high_resolution_clock::time_point> starts_;
  
  bool recording_;
  bool done_;

};

}  //  namespace sgate
