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
  LOG_INFO_COM("%s Init START", view_->hostname().c_str());
  for (uint32_t i = 0; i < view_->nodes_size(); i++) {
    senders_.push_back(new zmq::socket_t(context_, ZMQ_DEALER));
    if (i != view_->whoami()) {
      std::string identity = std::to_string(view_->whoami());
      senders_[i]->setsockopt(ZMQ_IDENTITY, identity.c_str(), identity.size());
      std::string address = "tcp://" + view_->address(i) + ":" + std::to_string(view_->port(i));
      LOG_INFO_COM("Connect to address %s, host_name %s", address.c_str(), view_->hostname(i).c_str());
      senders_[i]->connect(address.c_str());
    }
  }

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
    senders_.push_back(new zmq::socket_t(context_, ZMQ_DEALER));
    std::string identity = std::to_string(view_->whoami());
    senders_[i]->setsockopt(ZMQ_IDENTITY, identity.c_str(), identity.size());
    std::string address = "tcp://" + view_->address(i) + ":" + std::to_string(view_->port(i));
    LOG_INFO_COM("Connect to address %s, host_name %s", address.c_str(), view_->hostname(i).c_str());
    senders_[i]->connect(address.c_str());
  }
  boost::thread listen(boost::bind(&Commo::client_waiting_msg, this)); 
}

Commo::~Commo() {
}


void Commo::waiting_msg() {

  std::vector<zmq::pollitem_t> items(1 + view_->nodes_size());
  items[0].socket = (void *)(*receiver_);
  items[0].events = ZMQ_POLLIN;

  for (int i = 0; i < view_->nodes_size(); i++) {
    items[i + 1].socket = (void *)(*senders_[i]);
    items[i + 1].events = ZMQ_POLLIN; 
  }

  while (true) {
    zmq::poll(&items[0], items.size(), 1);
    for (int i = 0; i <= view_->nodes_size(); i ++) {
      if (items[i].revents & ZMQ_POLLIN) {
        zmq::message_t identity;
        zmq::message_t request;
        //  Wait for next request from client
        if (i == 0) {
          while (receiver_->recv(&identity, ZMQ_DONTWAIT) > 0) {
            receiver_->recv(&request);
            int size_id = identity.size();
            std::string data_id(static_cast<char*>(identity.data()), size_id);
            LOG_DEBUG_COM("receiver received from %s", data_id.c_str());
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
        else {
          while (senders_[i - 1]->recv(&request, ZMQ_DONTWAIT) > 0) {
            LOG_DEBUG_COM("senders_[%d] received!", i - 1);
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
            LOG_DEBUG_COM("Received %s", text_str.c_str());
            captain_->handle_msg(msg, static_cast<MsgType>(type));
            LOG_DEBUG("Handle finish!");
          }
        } 
      }
    }
  }
}

void Commo::client_waiting_msg() {

  std::vector<zmq::pollitem_t> items(view_->nodes_size());

  for (int i = 0; i < view_->nodes_size(); i++) {
    items[i].socket = (void *)(*senders_[i]);
    items[i].events = ZMQ_POLLIN; 
  }

  while (true) {

    zmq::poll(&items[0], items.size(), 1);

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
//    if (msg_type != DECIDE) 
    captain_->re_handle_msg(msg, msg_type);
    return;
  }
  for (uint32_t i = 0; i < view_->nodes_size(); i++) {
    if (i == view_->whoami()) {
 //     if (msg_type != DECIDE)
      captain_->re_handle_msg(msg, msg_type);
      continue;
    }
    std::string msg_str;
    msg->SerializeToString(&msg_str);
    msg_str.append(std::to_string(msg_type));
    // create a zmq message from the serialized string
    zmq::message_t request(msg_str.size());
    memcpy((void *)request.data(), msg_str.c_str(), msg_str.size());
    LOG_DEBUG_COM("Broadcast to --%s (msg_type):%d", view_->hostname(i).c_str(), msg_type);
    senders_[i]->send(request, ZMQ_DONTWAIT);
    LOG_DEBUG_COM("Broadcast to --%s (msg_type):%d finished", view_->hostname(i).c_str(), msg_type);
  }

}

// COMMIT & TEACH
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
  zmq::message_t request(msg_str.size());
  memcpy((void *)request.data(), msg_str.c_str(), msg_str.size());

  std::string data_id = std::to_string(node_id);
  zmq::message_t identity(data_id.size());
  memcpy((void *)identity.data(), data_id.c_str(), data_id.size());

  LOG_DEBUG_COM("receiver_ reply request to %d", node_id);
  receiver_->send(identity, ZMQ_SNDMORE);
  receiver_->send(request, ZMQ_DONTWAIT);
}
} // namespace sgate
