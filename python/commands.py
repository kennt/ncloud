# -*- coding: utf-8 -*-
""" Import this file to send and receive packets from the application
    This will both send a packet and then wait for the response

    Assumes UDP
"""

import socket
import json


class Commands(object):
    """ Provides an interface for sending commands to the DHT
        members.

        This will encode the messages using json and send the
        messages via UDP.
    """

    # These constants MUST match up with the constants used
    #    in the C++ code.
    #
    CNONE = 0
    CPING = 1
    CGETMEMBERS = 2
    CGETREPLICACOUNT = 3
    CQUIT = 4
    CCREATE = 5
    CREAD = 6
    CUPDATE = 7
    CDELETE = 8
    CREPLY = 9

    type_map = {CNONE: "",
                CPING: "ping",
                CGETMEMBERS: "get_members",
                CGETREPLICACOUNT, "get_replica_count",
                CQUIT: "quit",
                CCREATE: "create",
                CREAD: "read",
                CUPDATE: "update",
                CDELETE: "delete",
                CREPLY: "reply"}

    # transaction id for messages.
    transid = 1

    sock = None

    def __init__(self, address=("127.0.0.1", 9000)):
        self.receiving_address = None
        self.open(address)

    def open(self, address=("127.0.0.1", 9000)):
        """ Opens a socket using the address passed in:

            Args:
            address (tuple of a string and an int):
                The tuple consists of a string containing an
                IP address and an int holding the port number.
                (default: ("128.0.0.1", 9000))
        """
        if self.sock is not None:
            self.close()

        self.receiving_address = address
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(self.receiving_address)

        # set a default timeout of 15 seconds, should be enough
        # time to switch back to a debugger and set it running
        self.sock.settimeout(15)

        print "opening socket on " + str(self.receiving_address)

    def close(self):
        """ Close and clean up the connection. """

        # Using UDP, no need to shutdown
        # self.sock.shutdown(socket.SHUT_RDWR)
        if self.sock is not None:
            self.sock.close()
        self.sock = None

    def pretty_print_message(self, json_message):
        """ Format the message for easy reading. """

        message = json.loads(json_message)
        print "message ===="
        print "  type       : " + self.type_map[message["type"]]
        print "  replytype  : " + self.type_map[message.get("replytype", 0)]
        print "  transid    : " + str(message["transid"])
        print "  success    : " + str(message.get("success", ""))
        print "  key        : " + message.get("key", "")
        print "  value      : " + message.get("value", "")
        print "  errmsg     : " + message.get("errmgs", "")
        if "members" in message:
            print "  members    : size=" + str(len(message["members"]))
            pos = 0
            for member in message["members"]:
                tmp = "               [{0}]  {1}:{2}  ts= {3}  hb= {4}".format(
                    pos, member["a"], member["p"], member["ts"], member["hb"])
                print tmp
                pos += 1

        print "raw message ===="
        print json.dumps(message, indent=2)

    def send_message(self, address, message):
        message["transid"] = self.transid
        self.transid += 1

        json_message = json.dumps(message)
        self.sock.sendto(json_message, address)

        print "sending to " + str(address)
        self.pretty_print_message(json_message)

    # An address below is the combination of a
    # string (the IP address) and a port number
    # for example, ("127.0.0.1", 8080)

    def send_ping(self, address):
        self.send_message(address, {"type": Commands.CPING})

    def send_getmembers(self, address):
        self.send_message(address, {"type": Commands.CGETMEMBERS})

    def send_getreplicacount(self, address):
        self.send_message(address, {"type": Commands.CGETREPLICACOUNT})

    def send_quit(self, address):
        self.send_message(address, {"type": Commands.CQUIT})

    def send_create(self, address, key, value):
        self.send_message(address, {"type": Commands.CCREATE,
                                    "key": str(key),
                                    "value": str(value)})

    def send_read(self, address, key):
        self.send_message(address, {"type": Commands.CREAD,
                                    "key": str(key)})

    def send_update(self, address, key, value):
        self.send_message(address, {"type": Commands.CUPDATE,
                                    "key": str(key),
                                    "value": str(value)})

    def send_delete(self, address, key):
        self.send_message(address, {"type": Commands.CDELETE,
                                    "key": str(key)})

    def wait_for_response(self):
        data, address = self.sock.recvfrom(4096)
        print "receiving message from " + str(address)
        self.pretty_print_message(data)
