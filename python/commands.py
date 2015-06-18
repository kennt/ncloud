#
# Import this file to send and receive packets from the application
# This will both send a packet and then wait for the response
#
# Assumes UDP
#

import socket
import sys
import json


class Commands:
	CNONE = 0
	CPING = 1
	CGETMEMBERS = 2
	CQUIT = 3
	CCREATE = 4
	CREAD = 5
	CUPDATE = 6
	CDELETE = 7
	CREPLY = 8

	type_map = { CNONE : "",
				 CPING : "ping",
				 CGETMEMBERS : "getmembers",
				 CQUIT: "quit",
				 CCREATE: "create",
			 	 CREAD: "read",
			 	 CUPDATE: "update",
			 	 CDELETE: "delete",
			 	 CREPLY: "reply"
		   	   };

	transid = 1

	sock = None
	
	def __init__(self, address=("127.0.0.1", 9000)):
		self.open(address)


	def open(self, address=("127.0.0.1", 9000)):
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
		#self.sock.shutdown(socket.SHUT_RDWR)
		if self.sock is not None:
			self.sock.close()
		self.sock = None

	def pretty_print_message(self, json_message):
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
				s = "               [{0}]  {1}:{2}  ts = {3}   hb = {4}".format(
						pos, member["a"], member["p"], member["ts"], member["hb"])
				print s
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
		message = {
			"type": Commands.CPING
		}
		self.send_message(address, message)

	def send_getmembers(self, address):
		message = {
			"type": Commands.CGETMEMBERS
		}
		self.send_message(address, message)

	def send_quit(self, address):
		message = {
			"type" : Commands.CQUIT
		}
		self.send_message(address, message)
	

	def send_create(self, address, key, value):
		message = {
			"type": Commands.CCREATE,
			"key": str(key),
			"value": str(value)
		}
		self.send_message(address, message)

	def send_read(self, address, key):
		message = {
			"type": Commands.CREAD,
			"key": str(key)
		}
		self.send_message(address, message)

	def send_update(self, address, key, value):
		message = {
			"type": Commands.CUPDATE,
			"key": str(key),
			"value": str(value)
		}
		self.send_message(address, message)

	def send_delete(self, address, key):
		mesage = {
			"type": Commands.CDELETE,
			"key": str(key)
		}
		self.send_message(address, message)

	def wait_for_response(self):
		data, address = self.sock.recvfrom(4096)	
		print "receiving message from " + str(address)
		self.pretty_print_message(data)
