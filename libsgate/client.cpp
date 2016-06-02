/**
 * client.cpp
 * Created on: May 31, 2016
 * Author: Lijing 
 */

#include "client.hpp"
#include "commo.hpp"
#include <cstdlib>
#include <math.h>
#include <stdlib.h>

namespace sgate {

Client::Client(node_id_t node_id, int commit_win, int ratio, node_id_t read_node) 
 : node_id_(node_id), com_win_(commit_win), ratio_(ratio), read_node_(read_node), master_id_(1),
   commit_counter_(0), rand_counter_(0), thr_counter_(0), starts_(20000000),
   recording_(false), done_(false) {

  write_or_read_ = ratio_ == 10 ? true : false;
}

Client::~Client() {
}

/**
 * Return Msg_commit 
 */
MsgCommit *Client::msg_commit(slot_id_t slot_id, bool read, std::string &data) {
  MsgHeader *msg_header = new MsgHeader();
  msg_header->set_msg_type(MsgType::COMMIT);
  msg_header->set_node_id(node_id_);
  msg_header->set_slot_id(slot_id);

  ClientValue *cli_value = new ClientValue();
  cli_value->set_read(read);
  cli_value->set_data(data);

  MsgCommit *msg_com = new MsgCommit();
  msg_com->set_allocated_msg_header(msg_header);
  msg_com->set_allocated_cli_value(cli_value);
  return msg_com; 
}

void Client::start_commit() {

  int warming = 1;
  int interval = 2;
//  for (int i = 0; i < warming; i++) {
//    LOG_INFO("Warming Counting %d", i + 1);
//    sleep(1);
//  }

#if MODE_TYPE == 1  
  int usec = 1000 * 2;
  com_win_ = 500 * 20;
  done_ = true;
  recording_ = true;
#endif

  for (int i = 0; i < com_win_; i++) {

    counter_mut_.lock();
    starts_[commit_counter_] = std::chrono::high_resolution_clock::now(); 
    std::string value = "Commiting Value Time_" + std::to_string(commit_counter_) + " from " + "client_" + std::to_string(i);
#if MODE_TYPE == 1
#else   
    LOG_INFO(" +++++++++++ ZERO Init Commit Value: %s +++++++++++", value.c_str());
#endif
    // random generate write or read num 0 ~ 9
    bool read = write_or_read_;
    if (ratio_ < 100) {
      if (ratio_ == 0) {
        read = true;
//        LOG_INFO("I read %d", commit_counter_);
      } else {
        if (commit_counter_ % 10 == 0) {
          rand_counter_ = commit_counter_ + (rand() % 10); 
        }
        if (commit_counter_ == rand_counter_) {
//          LOG_INFO("I 10per %d", commit_counter_);
          read = ~write_or_read_;
        } else {
//          LOG_INFO("I 90per %d", commit_counter_);
        }
      }
    } else {
//      LOG_INFO("I write %d", commit_counter_);
    }
  
    MsgCommit *msg_com = msg_commit(commit_counter_, read, value); 
    commit_counter_++;
    counter_mut_.unlock();
    
    if (read) 
      commo_->send_one_msg(msg_com, COMMIT, read_node_);
    else
      commo_->send_one_msg(msg_com, COMMIT, master_id_);

#if MODE_TYPE == 1
#else     
    LOG_INFO(" +++++++++++ ZERO FINISH Commit Value: %s +++++++++++", value.c_str());
#endif

#if MODE_TYPE == 1  
    usleep(usec);

    thr_mut_.lock();
    int throughput = thr_counter_;
    thr_mut_.unlock();
    throughputs_.push_back(throughput);
    if ((i % 500) == 0) {
      LOG_INFO("i = %d PUNCH!  -- counter:%lu", i, throughput);
    }
#endif

  }

#if MODE_TYPE == 1
#else  

  for (int i = 0; i < interval; i++) {
    LOG_INFO("Not Recording Counting %d", i + 1);
    sleep(1);
  }
  LOG_INFO("%d s passed start punching", interval);

  thr_mut_.lock();
  recording_ = true;
  thr_mut_.unlock();

  uint64_t before = 0;
  uint64_t throughput = 0;

  for (int j = 0; j < interval * 4; j++) {
    LOG_INFO("Time %d", j + 1);

    thr_mut_.lock();
    before = thr_counter_;
    thr_mut_.unlock();

    sleep(1);

    thr_mut_.lock();
    throughput = thr_counter_ - before; 

    if (periods_.size() > 0) {
      LOG_INFO("PUNCH!  -- counter:%llu second:1 throughput:%llu latency:%f ms", thr_counter_, throughput, periods_[periods_.size() - 1] / 1000.0);
    }
    else {
      LOG_INFO("PUNCH! -- counter:%llu second:1 throughput:%llu periods_.size() == 0", thr_counter_, throughput);
    }

    thr_mut_.unlock();
    throughputs_.push_back(throughput);
  }
  
  thr_mut_.lock();
  recording_ = false;
  done_ = true;
  thr_mut_.unlock();
#endif

  LOG_INFO("Last %d s period", interval);
  for (int i = interval; i > 0; i--) {
    LOG_INFO("Stop Committing Counting %d", i);
    sleep(1);
  }

  std::ofstream file_throughput_;
  std::ofstream file_latency_;
  std::string thr_name;
  std::string lat_name;
  
  LOG_INFO("Writing File Now!");

  thr_name = "results/N_t_" + std::to_string(com_win_) + "_" + std::to_string(ratio_) + "_" + std::to_string(read_node_) + ".txt";
  lat_name = "results/N_l_" + std::to_string(com_win_) + "_" + std::to_string(ratio_) + "_" + std::to_string(read_node_) + ".txt";

  file_throughput_.open(thr_name);

  file_latency_.open(lat_name);


  for (int i = 0; i < throughputs_.size(); i++) {
    file_throughput_ << throughputs_[i] << "\n";
  }

  file_throughput_.close();

  for (int j = 0; j < periods_.size(); j++) {
    file_latency_ << periods_[j] << "\n";
  }
  file_latency_.close();

  LOG_INFO("Writing File Finished!");
}

void Client::handle_reply(MsgAckCommit *msg_ack_com) {
  // counting
  slot_id_t commit_counter = msg_ack_com->msg_header().slot_id();
  if (msg_ack_com->cli_res_value().res_type() == ResType::RET) {
    if(msg_ack_com->cli_res_value().read()) {
      std::string res =  msg_ack_com->cli_res_value().data();
    } else {
      bool ok = msg_ack_com->cli_res_value().ok();
    }

    thr_mut_.lock();

    if (recording_) {
      auto finish = std::chrono::high_resolution_clock::now();
      periods_.push_back(std::chrono::duration_cast<std::chrono::microseconds>
                        (finish-starts_[commit_counter]).count());
      thr_counter_++;
    }
    thr_mut_.unlock();
    
    if (done_ == false) {
      counter_mut_.lock();
      std::string value = "Commiting Value Time_" + std::to_string(commit_counter_) + " from " + "client_" + std::to_string(node_id_);
  
      bool read = write_or_read_;
      if (ratio_ < 100) {
        if (ratio_ == 0) {
          read = true;
        } else {
          if (commit_counter_ % 10 == 0) {
            rand_counter_ = commit_counter_ + (rand() % 10); 
          }
          if (commit_counter_ == rand_counter_) {
            read = ~write_or_read_;
          } 
        }
      }   
      MsgCommit *msg_com = msg_commit(commit_counter_, read, value); 
      starts_[commit_counter_] = std::chrono::high_resolution_clock::now();
  
      commit_counter_++;
      counter_mut_.unlock();
  
      if (read) 
        commo_->send_one_msg(msg_com, COMMIT, read_node_);
      else
        commo_->send_one_msg(msg_com, COMMIT, master_id_);
    }
  } else if (msg_ack_com->cli_res_value().res_type() == ResType::MASTER_ID) {

    master_mut_.lock();
    if (master_id_ != msg_ack_com->cli_res_value().master_id()) {
      LOG_INFO("Master ID changed from %u to %u", master_id_, msg_ack_com->cli_res_value().master_id());
      master_id_ = msg_ack_com->cli_res_value().master_id();
    }
    std::string value = msg_ack_com->cli_res_value().data();
    MsgCommit *msg_com = msg_commit(commit_counter, false, value);
    commo_->send_one_msg(msg_com, COMMIT, master_id_);
    master_mut_.unlock();
  }

}

}  //  namespace sgate

