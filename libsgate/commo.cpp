/**
 * commo.cpp
 * Created on: May 30, 2016
 * Author: Lijing Wang
 */

#include "commo.hpp"
#include "captain.hpp"
#include "client.hpp"
#include <iostream>

namespace sgate {

Commo::Commo(Captain *captain, View &view) 
  : captain_(captain), view_(&view), context_(1) {
  if (view_->if_master()) {
    LOG_INFO_COM("%s Master Init START", view_->hostname().c_str());
    for (uint32_t i = 0; i < view_->nodes_size(); i++) {
      if (i != view_->whoami()) {
        ctxes_[i] = zmq::context_t(1);
        senders_[i] = new zmq::socket_t(ctxes_[i], ZMQ_DEALER);
        std::string identity = std::to_string(view_->whoami());
        senders_[i]->setsockopt(ZMQ_IDENTITY, identity.c_str(), identity.size());
        std::string address = "tcp://" + view_->address(i) + ":" + std::to_string(view_->port(i));
        LOG_INFO_COM("Connect to address %s, host_name %s", address.c_str(), view_->hostname(i).c_str());
        senders_[i]->connect(address.c_str());
        sender_threads_[i] = new boost::thread(boost::bind(&Commo::sender_waiting, this, i));
//        sender_threads_[i]->detach();
      }
    } 
  }
  else {
    LOG_INFO_COM("%s Servant Init START", view_->hostname().c_str());
  }
//  boost::thread listen(boost::bind(&Commo::waiting, this)); 
  receiver_ = new zmq::socket_t(context_, ZMQ_ROUTER);
  std::string address = "tcp://*:" + std::to_string(view_->port());
  LOG_INFO_COM("My address %s, host_name %s", address.c_str(), view_->hostname().c_str());
  receiver_->bind(address.c_str());
}

Commo::Commo(Client *client, View &view) 
  : client_(client), view_(&view), context_(1) {
//    frontend_(context_, ZMQ_ROUTER), backend_(context_, ZMQ_DEALER) {
  LOG_INFO_COM("Init START for Client");
  for (uint32_t i = 0; i < view_->nodes_size(); i++) {
    senders_[i] = new zmq::socket_t(ctxes_[i], ZMQ_DEALER);
    std::string identity = std::to_string(view_->whoami());
    senders_[i]->setsockopt(ZMQ_IDENTITY, identity.c_str(), identity.size());
    std::string address = "tcp://" + view_->address(i) + ":" + std::to_string(view_->port(i));
    LOG_INFO_COM("Connect to address %s, host_name %s", address.c_str(), view_->hostname(i).c_str());
    senders_[i]->connect(address.c_str());
  }
  boost::thread listen(boost::bind(&Commo::client_waiting, this)); 
}

Commo::~Commo() {
}

void Commo::waiting_msg() {
  
  std::vector<zmq::pollitem_t> items(1);
  items[0].socket = (void *)(*receiver_);
  items[0].events = ZMQ_POLLIN; 
  while (true) {
    zmq::poll(&items[0], 1, 0);
      if (items[0].revents & ZMQ_POLLIN) {
      zmq::message_t identity;
      zmq::message_t request;
      while (receiver_->recv(&identity, ZMQ_DONTWAIT) > 0) {
        receiver_->recv(&request);
        std::string msg_str(static_cast<char*>(request.data()), request.size());
        int type = int(msg_str.back() - '0');
        google::protobuf::Message *msg = nullptr;
        switch(type) {
          case PREPARE: {
            msg = new MsgPrepare();
            break;
          }
          case ACCEPT: {
            msg = new MsgAccept();
            break;
          }
          case DECIDE: {
            msg = new MsgDecide();
            break;
          }
          case TEACH: {
            msg = new MsgTeach();
            break;
          }
          case COMMIT: {
            msg = new MsgCommit();
            break;
          }
          default: 
            break;
        }
        msg_str.pop_back();
        msg->ParseFromString(msg_str);
        std::string text_str;
        google::protobuf::TextFormat::PrintToString(*msg, &text_str);
        LOG_DEBUG_COM("Receiver Received %s", text_str.c_str());
        captain_->re_handle_msg(msg, static_cast<MsgType>(type), identity);
        LOG_DEBUG("Receiver Handle finish!");
      }
    }

    if (view_->if_master()) {
      while (!receiver_queue_.empty()) {
        receiver_lock_.lock();
        zmq::message_t identity;
        identity.copy(receiver_id_queue_.front());
        receiver_id_queue_.pop();
        zmq::message_t request;
        request.copy(receiver_queue_.front());
        receiver_queue_.pop();
        receiver_lock_.unlock();

        receiver_->send(identity, ZMQ_SNDMORE);
        receiver_->send(request, ZMQ_DONTWAIT);
      } 
    }
  }
}

void Commo::sender_waiting(node_id_t id) {

  std::vector<zmq::pollitem_t> items(1);
  items[0].socket = (void *)(*senders_[id]);
  items[0].events = ZMQ_POLLIN; 

  while (true) {

    while (!sender_queues_[id].empty()) {
      LOG_DEBUG("I'm sending sender_%d", id);
      sender_locks_[id].lock();
      zmq::message_t request;
      request.copy(sender_queues_[id].front());
      sender_queues_[id].pop();
      sender_locks_[id].unlock();
      senders_[id]->send(request, ZMQ_DONTWAIT);
      LOG_DEBUG("I'm sending sender_%d FINISH", id);
    }

    zmq::poll(&items[0], 1, 0);
    if (items[0].revents & ZMQ_POLLIN) {
      zmq::message_t request;
      senders_[id]->recv(&request, ZMQ_DONTWAIT);
      std::string msg_str(static_cast<char*>(request.data()), request.size());
      int type = int(msg_str.back() - '0');
      google::protobuf::Message *msg = nullptr;
      switch(type) {
        case PROMISE: {
          msg = new MsgAckPrepare();
          break;
        }
        case ACCEPTED: {
          msg = new MsgAckAccept();
          break;
        }
        case LEARN: {
          msg = new MsgLearn();
          break;
        }                        
        default: 
          break;
      }
      msg_str.pop_back();
      msg->ParseFromString(msg_str);
      std::string text_str;
      google::protobuf::TextFormat::PrintToString(*msg, &text_str);
      LOG_DEBUG_COM("Sender Received %s", text_str.c_str());
      captain_->handle_msg(msg, static_cast<MsgType>(type));
      LOG_DEBUG("Sender Handle finish!");
    }

  }
}

void Commo::client_waiting() {
  std::vector<zmq::pollitem_t> items(view_->nodes_size());
  for (int i = 0; i < view_->nodes_size(); i++) {
    items[i].socket = (void *)(*senders_[i]);
    items[i].events = ZMQ_POLLIN; 
  }
  while (true) {
    zmq::poll(&items[0], items.size(), -1);
    for (int i = 0; i < view_->nodes_size(); i ++) {
      if (items[i].revents & ZMQ_POLLIN) {
        zmq::message_t request;
        while (senders_[i]->recv(&request, ZMQ_DONTWAIT) > 0) {
          LOG_DEBUG_COM("senders_[%d] received!", i);
          std::string msg_str(static_cast<char*>(request.data()), request.size());
          MsgAckCommit *msg = new MsgAckCommit();
          msg_str.pop_back();
          msg->ParseFromString(msg_str);
          std::string text_str;
          google::protobuf::TextFormat::PrintToString(*msg, &text_str);
          LOG_DEBUG_COM("Received %s", text_str.c_str());
          client_->handle_reply(msg);
          LOG_DEBUG("Handle finish!");
        }
      }
    }
  }
}

// PREPARE ACCEPT DECIDE now only ACCEPT
void Commo::broadcast_msg(google::protobuf::Message *msg, MsgType msg_type) {

  if (view_->nodes_size() == 1) {
    captain_->re_handle_msg(msg, msg_type);
    return;
  }
  for (uint32_t i = 0; i < view_->nodes_size(); i++) {
    if (i == view_->whoami()) {
      captain_->re_handle_msg(msg, msg_type);
      continue;
    }
    std::string msg_str;
    msg->SerializeToString(&msg_str);
    msg_str.append(std::to_string(msg_type));
    // create a zmq message from the serialized string
    zmq::message_t *request = new zmq::message_t(msg_str.size());
    memcpy((void *)request->data(), msg_str.c_str(), msg_str.size());
    LOG_DEBUG_COM("Broadcast save to queues --%s (msg_type):%d", view_->hostname(i).c_str(), msg_type);
    sender_locks_[i].lock();
    sender_queues_[i].push(request);
    sender_locks_[i].unlock();
//    senders_[i]->send(request, ZMQ_DONTWAIT);
    LOG_DEBUG_COM("Broadcast save to queues --%s (msg_type):%d finished", view_->hostname(i).c_str(), msg_type);
  }
}

// COMMIT for client & TEACH for master
void Commo::send_one_msg(google::protobuf::Message *msg, MsgType msg_type, node_id_t node_id) {
//  std::cout << " --- Commo Send ONE to captain " << node_id << " MsgType: " << msg_type << std::endl;
  LOG_DEBUG_COM("Send ONE to --%s (msg_type):%d", view_->hostname(node_id).c_str(), msg_type);
  std::string msg_str;
  msg->SerializeToString(&msg_str);
  msg_str.append(std::to_string(msg_type));
  // create a zmq message from the serialized string
  zmq::message_t request(msg_str.size());
  memcpy((void *)request.data(), msg_str.c_str(), msg_str.size());
  LOG_DEBUG_COM("senders[%d] send request", node_id);
  senders_[node_id]->send(request, ZMQ_DONTWAIT);
  LOG_DEBUG_COM("senders[%d] send finish", node_id);
}

// PROMISE ACCEPTED LEARN for acc, COMMITTED READ for master 
void Commo::reply_msg(google::protobuf::Message *msg, MsgType msg_type, zmq::message_t &identity) {
//  std::cout << " --- Commo Send ONE to captain " << node_id << " MsgType: " << msg_type << std::endl;
  LOG_DEBUG_COM("Reply to --%s (msg_type):%d", view_->hostname(node_id).c_str(), msg_type);
  std::string msg_str;
  msg->SerializeToString(&msg_str);
  msg_str.append(std::to_string(msg_type));
  zmq::message_t request(msg_str.size());
  memcpy((void *)request.data(), msg_str.c_str(), msg_str.size());
  zmq::message_t copied_id;
  copied_id.copy(&identity);
  LOG_DEBUG_COM("receiver_ reply request to %s", view_->hostname(node_id).c_str());
  receiver_->send(copied_id, ZMQ_SNDMORE);
  receiver_->send(request, ZMQ_DONTWAIT);
  LOG_DEBUG_COM("receiver_ reply request to %s finish", view_->hostname(node_id).c_str());
}
// COMMITTED WRITE for master 
void Commo::reply_client(google::protobuf::Message *msg, MsgType msg_type, node_id_t node_id) {
//  std::cout << " --- Commo Send ONE to captain " << node_id << " MsgType: " << msg_type << std::endl;
  LOG_DEBUG_COM("Reply to --%s (msg_type):%d", view_->hostname(node_id).c_str(), msg_type);
  std::string msg_str;
  msg->SerializeToString(&msg_str);
  msg_str.append(std::to_string(msg_type));
  zmq::message_t *request = new zmq::message_t(msg_str.size());
  memcpy((void *)request->data(), msg_str.c_str(), msg_str.size());
  std::string data_id = std::to_string(node_id);
  zmq::message_t* identity = new zmq::message_t(data_id.size());
  memcpy((void *)identity->data(), data_id.c_str(), data_id.size());
  LOG_DEBUG_COM("receiver_ reply request to %d", node_id);
  receiver_lock_.lock();
  receiver_id_queue_.push(identity);
  receiver_queue_.push(request);
  receiver_lock_.unlock();
//  receiver_->send(identity, ZMQ_SNDMORE);
//  receiver_->send(request, ZMQ_DONTWAIT);
}
} // namespace sgate
