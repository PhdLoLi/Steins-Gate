#include "zhelpers.hpp"

int main (int argc, char *argv[])
{
    zmq::context_t context(1);

	  zmq::socket_t responder(context, ZMQ_REP);

    responder.connect("tcp://localhost:5560");

    while (true) {
        zmq::message_t request;

        //  Wait for next request from client
        responder.recv (&request);
        std::cout << "Received Hello" << std::endl;

        //  做一些“工作”
        sleep (1);

        zmq::message_t reply (5);
        memcpy (reply.data (), "World", 5);
        responder.send (reply);
    }
  
    return 0;
}
