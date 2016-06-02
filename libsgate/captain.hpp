/**
 * captain.hpp
 * Created on: May 30, 2016
 * Author: Lijing Wang
 */

#pragma once
#include "proposer.hpp"
//#include "commo.hpp"
#include "acceptor.hpp"
#include <queue>
#include <unordered_map>
#include "threadpool.hpp" 
//#include "ThreadPool.h"
#include <map>
//#include "include_all.h"
using namespace boost::threadpool;
namespace sgate {
// This module is responsible for locating the correct proposer and
// acceptor for a certain instance (an instance is identified by slot_id). 

struct proposer_info_t {

  int try_time;
  Proposer *curr_proposer;
  ProposerStatus proposer_status;

  proposer_info_t(int times) 
    : try_time(times), curr_proposer(NULL), proposer_status(EMPTY) {
  };
};


class Commo;
class Captain {
 public:

  Captain(View &view, int window_size);

  ~Captain();


  /**
   * set_callback from outside
   */
  void set_callback(callback_t& cb);

  /**
   * set_callback from outside
   */
  void set_callback(callback_full_t& cb);

  void set_callback(callback_latency_t& cb);

  /**
   * warpper of commit_value and master lease 
   */
  void commit(PropValue *);

  /** 
   * return node_id
   */
//  node_id_t get_node_id(); 

  /**
   * set commo_handler 
   */
  void set_commo(Commo *); 

  /**
   * handle message from commo, all kinds of message
   */
  void handle_msg(google::protobuf::Message *, MsgType);

  /**
   * Add a new chosen_value 
   */
  void add_chosen_value(slot_id_t, PropValue *);

  /**
   * Add a new learn_value 
   */
  void add_learn_value(slot_id_t, PropValue *, node_id_t);

  /**
   * Return Msg_header
   */
  MsgHeader *set_msg_header(MsgType, slot_id_t);

  /**
   * Return Decide Message
   */
  MsgDecide *msg_decide(slot_id_t);

  /**
   * Return Learn Message
   */
  MsgLearn *msg_learn(slot_id_t);

  /**
   * Return Teach Message
   */
  MsgTeach *msg_teach(slot_id_t);

  /**
   * Return Command Message
   */
  MsgCommand *msg_command();

  /**
   * Return Committed Message
   */
  MsgAckCommit *msg_committed(slot_id_t);


//  void clean();
//
//  void crash();
//
//  void recover();
//
//  bool get_status();

  void print_chosen_values();

  std::vector<PropValue *> get_chosen_values();

//  bool if_recommit();

  void add_callback();


 private:

  View *view_;
  Commo *commo_;

  std::vector<Acceptor *> acceptors_;
  std::map<slot_id_t, proposer_info_t *> proposers_;
  std::vector<PropValue *> chosen_values_;
  std::queue<PropValue *> tocommit_values_;

  // max chosen instance/slot id 
  slot_id_t max_chosen_; 
  slot_id_t max_chosen_without_hole_; 
  slot_id_t max_slot_;

  // aim to supporting window
  slot_id_t window_size_;

  /** 
   * trigger this callback 
   * sequentially for each chosen value.
   */
  callback_t callback_;
  callback_full_t callback_full_;
  // when one value commited by this node is chosen, trigger this
  callback_latency_t callback_latency_;

  boost::mutex max_chosen_mutex_;

  boost::mutex tocommit_values_mutex_;
  boost::mutex acceptors_mutex_;
  boost::mutex proposers_mutex_;


  std::map<value_id_t, boost::mutex> commit_mutexs_;
  std::map<value_id_t, boost::condition_variable> commit_conds_;
  std::map<value_id_t, bool> commit_readys_;

};

} //  namespace sgate
