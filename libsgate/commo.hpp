/**
 * commo.hpp
 * Created on: May 30, 2016
 * Author: Lijing Wang
 */

#pragma once

#include "view.hpp"
#include "threadpool.hpp" 
#include <boost/thread/mutex.hpp>
#include <boost/bind.hpp>

#include <zmq.hpp>
#include <unistd.h>
#include <google/protobuf/text_format.h>
//#include <map>

using namespace boost::threadpool;
namespace sgate {
class Captain;
class Client;
class server_task;

class Commo {
 public:
  enum { kMaxThread = 3 };
  Commo(Captain *captain, View &view);
  Commo(Client *client, View &view);
  ~Commo();
  void waiting_msg();
  void broadcast_msg(google::protobuf::Message *, MsgType);
  void send_one_msg(google::protobuf::Message *, MsgType, node_id_t);
  void reply_msg(google::protobuf::Message *, MsgType, zmq::message_t &id);
  void reply_client(google::protobuf::Message *, MsgType, node_id_t);
  void deal_msg(zmq::message_t &request);

  void client_waiting_msg();

 private:
  void work(zmq::socket_t *worker);
  void waiting();
  std::vector<Captain *> captains_;
  Captain *captain_;
  Client *client_;
  View *view_;
//  pool *pool_;
  zmq::context_t context_;
//  zmq::context_t ctx_;
  zmq::socket_t *receiver_;
//  zmq::socket_t sender_;
  std::vector<zmq::socket_t *> senders_;
//  std::vector<zmq::context_t> ctxes_;
//  std::vector<boost::thread *> sender_threads;

//  std::vector<zmq::socket_t *> workers_;
//  std::vector<boost::thread *> worker_threads;
//  zmq::socket_t frontend_;
//  zmq::socket_t backend_;
//  server_task *st_;
//  std::vector<int> senders_state_;

};
} // namespace sgate
