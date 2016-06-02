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

using namespace boost::threadpool;
namespace sgate {
class Captain;
class Client;
class Commo {
 public:
  Commo(Captain *captain, View &view);
  Commo(Client *client, View &view);
  ~Commo();
  void waiting_msg();
  void broadcast_msg(google::protobuf::Message *, MsgType);
  void send_one_msg(google::protobuf::Message *, MsgType, node_id_t);
  void deal_msg(zmq::message_t &request);

  void client_waiting_msg();

 private:
  std::vector<Captain *> captains_;
  Captain *captain_;
  Client *client_;
  View *view_;
  pool *pool_;
  zmq::context_t context_;
  zmq::socket_t *receiver_;
//  zmq::socket_t sender_;
  std::vector<zmq::socket_t *> senders_;
  std::vector<std::string> senders_address_;
//  std::vector<int> senders_state_;

};
} // namespace sgate
