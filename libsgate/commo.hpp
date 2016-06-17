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
  void client_waiting_msg();
  void broadcast_msg(google::protobuf::Message *, MsgType);
  void send_one_msg(google::protobuf::Message *, MsgType, node_id_t);
  void reply_msg(google::protobuf::Message *, MsgType, zmq::message_t &id);
  void reply_client(google::protobuf::Message *, MsgType, node_id_t);


 private:
  std::vector<Captain *> captains_;
  Captain *captain_;
  Client *client_;
  View *view_;
//  pool *pool_;
  zmq::context_t context_;
  zmq::socket_t *receiver_;
  std::vector<zmq::socket_t *> senders_;

};
} // namespace sgate
