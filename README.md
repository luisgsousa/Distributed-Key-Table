# Distributed Key Table
 Simplified version of a distributed hash table

To run the program open the terminal on the file location and type "./dkt IP PORT", where IP and port are the IP and port of your choice in yyour computer(dor example "./dkt localhost 50000"). You can run several instances of the program in different terminals using the same IP with different ports so as to create a network with various terminals.

The available commands are:

- "new node_id node_ip node_port"
    This command creates a new key table with the specified node. node_id is an arbitrary number to be used as the node key

- "sentry node_id succ_id succ_ip succ_port"
    This command allow you to enter a node directly into the network behind the successor of your choice. node_id is the key of the node that will enter the network and the following three parameters are of the supposed successor of this node.

- "entry node_id host_id host_ip host_port"
    This command will insert the node with they key node_id into the network in the correct position. The boot parameters refer to the host of the network

- "leave"
    Makes the current node leave the network

- "show"
    Shows the state of the node (its key, IP and port as well as the one of its successor and the successor's successor)

- "exit"
    Closes the program


    To test the program you can try the following test. Open 3 terminals and type the following commands into each terminal:

Terminal 1:
  "./dkt localhost 50000"
  "new 1 localhost 50000"

Terminal 2:
  "./dkt localhost 51000"
  "sentry 2 1 localhost 50000"

Terminal 3:
  "./dkt localhost 50000"
  "entry 3 1 localhost 50000"
  "show"
