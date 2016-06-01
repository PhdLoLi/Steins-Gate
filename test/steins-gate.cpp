/**
 * steins-gate.cpp
 * Created on: May 31, 2016
 * Author: Lijing 
 */


#include "view.hpp"
#include "commo.hpp"
#include "captain.hpp"

#include <fstream>
#include <chrono>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <algorithm>
#include <boost/bind.hpp>

namespace sgate {

using namespace std;
  
class SteinsGate {
 public:
  SteinsGate(node_id_t my_id, int node_num, int win_size, int local) 
    : my_id_(my_id), node_num_(node_num), win_size_(win_size), local_(local) {

    std::string tag;
    if (local_ == 0)
      tag = "localhost-";
    else 
      tag = "nodes-";

    std::string config_file = "config/" + tag + to_string(node_num_) + ".yaml";

    // init view_ for one captain_
    view_ = new View(my_id_, config_file);
    view_->print_host_nodes();
    my_name_ = view_->hostname();

    // init callback
    captain_ = new Captain(*view_, win_size);
    commo_ = new Commo(captain_, *view_);
    captain_->set_commo(commo_);

  }

  ~SteinsGate() {
  }

  void start() {
    commo_->waiting_msg();
  }

  std::string my_name_;
  node_id_t my_id_;
  node_id_t node_num_;
  int win_size_;

  Captain *captain_;
  View *view_;
  Commo *commo_;
  int local_;
};

static void sig_int(int num) {
  std::cout << "Control + C triggered! " << std::endl;
  exit(num);
}  

int main(int argc, char** argv) {
  signal(SIGINT, sig_int);
 

  if (argc < 5) {
    std::cerr << "Usage: Node_ID Node_Num Win_Size LocalorNot" << std::endl;
    return 0;
  }

  node_id_t my_id = stoul(argv[1]); 
  int node_num = stoi(argv[2]);
  int win_size = stoi(argv[3]);
  int local = stoi(argv[4]);
  
  SteinsGate sgate(my_id, node_num, win_size, local);
  sgate.start();

  return 0;
}


} // namespace sgate

int main(int argc, char** argv) {
  return sgate::main(argc, argv);
}
