/*
 * view.cpp
 * Created on: May 30, 2016
 * Author: Lijing 
 */

#include "view.hpp"
namespace sgate {

View::View(node_id_t node_id, std::string cf) 
  : node_id_(node_id), master_id_(0), period_(500) {

  LOG_INFO("loading config file %s ...", cf.c_str());
	
	YAML::Node config;

  if (cf.empty()) {
    // default only one node
	  config = YAML::LoadFile("config/localhost-1.yaml");
    node_id_ = 0;
  } else {
	  config = YAML::LoadFile(cf);
  }
  if (config == NULL) {
    printf("cannot open config file: %s.", cf.c_str());
  }

	YAML::Node nodes = config["host"];
  YAML::Node clients = config["client"];
  YAML::Node lease = config["lease"];

  for (std::size_t i = 0; i < nodes.size(); i++) {

		YAML::Node node = nodes[i];

		std::string name = node["name"].as<std::string>();
		std::string addr = node["addr"].as<std::string>();
    uint32_t port = node["port"].as<int>();
    // set a node in view
    host_info_t host_info = host_info_t(name, addr, port);
    host_nodes_.push_back(host_info);
  }
    
  size_ = host_nodes_.size();

  for (std::size_t i = 0; i < clients.size(); i++) {

		YAML::Node node = clients[i];

		std::string name = node["name"].as<std::string>();
		std::string addr = node["addr"].as<std::string>();
    uint32_t port = node["port"].as<int>();
    // set a node in view
    host_info_t host_info = host_info_t(name, addr, port);
    host_nodes_.push_back(host_info);
  }

  if (lease) {
    master_id_ = lease["master_id"].as<int>();
    period_ = lease["period"].as<int>();
  } else {
    LOG_INFO("No lease Node Found, using default master_id/0 period/500");
  }
  
  if (node_id_ >= host_nodes_.size()) {
    std::cout << "Node_Id " << node_id_ << " > host_nodes_.size " << size_ << "Invalid!" << std::endl;
    std::cout << "Set Node_Id = 0" << std::endl;
    node_id_ = 0;
  }
  LOG_INFO("config file loaded");
 
}

View::View(node_id_t node_id) : node_id_(node_id) {
  nodes_.insert(node_id);
}

View::View(node_id_t node_id, std::set<node_id_t> &nodes) 
  : node_id_(node_id), nodes_(nodes) {
}


std::set<node_id_t> * View::get_nodes() {
  return &nodes_;
}

std::vector<host_info_t> * View::get_host_nodes() {
  return &host_nodes_;
}

node_id_t View::whoami() {
  return node_id_;
}

bool View::if_master() {
  return node_id_ == master_id_ ? true : false; 
}

void View::set_master(node_id_t node_id) {
  master_id_ = node_id;
}

std::string View::hostname() {
  return host_nodes_[node_id_].name;
}

std::string View::address() {
  return host_nodes_[node_id_].addr;
}

uint32_t View::port() {
  return host_nodes_[node_id_].port;
}

std::string View::hostname(node_id_t node_id) {
  return host_nodes_[node_id].name;
}

std::string View::address(node_id_t node_id) {
  return host_nodes_[node_id].addr;
}

uint32_t View::port(node_id_t node_id) {
  return host_nodes_[node_id].port;
}

node_id_t View::master_id() {
  return master_id_;
}

uint64_t View::nodes_size() {
  return size_;
}

uint32_t View::period() {
  return period_;
}


void View::print_host_nodes() {
  std::cout << "-----*-*-*-*-*-*-*-*-*------" << std::endl;
  std::cout << "\t My Node" << std::endl;
  std::cout << "\tNode_ID: " << node_id_ << std::endl;
  std::cout << "\tName: " << host_nodes_[node_id_].name << std::endl;
  std::cout << "\tAddr: " << host_nodes_[node_id_].addr << std::endl;
  std::cout << "\tPort: " << host_nodes_[node_id_].port << std::endl;
  std::cout << "-----*-*-*-*-*-*-*-*-*------\n" << std::endl;
  std::cout << "\t Nodes INFO" << std::endl;
  for (int i = 0; i < size_; i++) {
    std::cout << "-*-*-*-*-*-*-*-*-*-*-*-*-*-*" << std::endl;
    std::cout << "\tNode_ID: " << i << std::endl;
    std::cout << "\tName: " << host_nodes_[i].name << std::endl;
    std::cout << "\tAddr: " << host_nodes_[i].addr << std::endl;
    std::cout << "\tPort: " << host_nodes_[i].port << std::endl;
    std::cout << "-*-*-*-*-*-*-*-*-*-*-*-*-*-*" << std::endl;
  }
  std::cout << "\tTotal Size: " << size_ << std::endl;
  std::cout << "-----*-*-*-*-*-*-*-*-*------\n" << std::endl;
  std::cout << "\t Lease INFO" << std::endl;
  std::cout << "  -----*-*-*-*-*-*-*-----   " << std::endl;
  std::cout << "\t Master_ID: " << master_id_ << std::endl;
  std::cout << "\t Period: " << period_ << std::endl;
  std::cout << "-----*-*-*-*-*-*-*-*-*------\n" << std::endl;
}

} // namespace sgate 
