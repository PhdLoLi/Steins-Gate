/**
 * clients.cpp
 * Created on: May 31, 2016
 * Author: Lijing 
 */

#include "client.hpp"
#include "view.hpp"
#include "commo.hpp"

namespace sgate {

int main(int argc, char** argv) {

  if (argc < 6) {
    std::cerr << " Node_ID Node_Num Win_Size LocalorNot Write_Ratio(100/90/10/0)" << std::endl;
    return 0;
  }

  int node_id = std::stoi(argv[1]);
  int node_num = std::stoi(argv[2]);
  int win_size = std::stoi(argv[3]);
  int local = std::stoi(argv[4]);
  int ratio = std::stoi(argv[5]);
  int read_node = 0;

  std::string tag;
  if (local == 0)
    tag = "localhost-";
  else 
    tag = "nodes-";

  std::string config_file = "config/" + tag + std::to_string(node_num) + ".yaml";

  // init view_ for one captain_
  View* view = new View(node_id, config_file);
  view->print_host_nodes();

  // init callback
  Client* client = new Client(node_id, win_size, ratio, read_node);
  Commo* commo = new Commo(client, *view);
  client->set_commo(commo);
  client->start_commit();

  return 0;
}

} // sgate

int main(int argc, char** argv) {
  return sgate::main(argc, argv);
}
