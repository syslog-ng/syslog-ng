import requests

class LogDestination(object):

    def open(self):
        """Open a connection to the target service"""
        return True

    def close(self):
        """Close the connection to the target service"""
        pass

    def is_opened(self):
        """Check if the connection to the target is able to receive messages"""
        return True

    def init(self):
        """This method is called at initialization time"""
        return True

    def deinit(self):
        """This method is called at deinitialization time"""
        pass

    def send(self, msg):
        """Send a message to the target service

        It should return True to indicate success, False will suspend the
        destination for a period specified by the time-reopen() option."""
        pass


class HTTPDestination(LogDestination):

    def __determine_http_send_handler(self, handler_type):
        handlers = {'PUT': requests.put, 'POST': requests.post }
        return handlers[handler_type]

    def __init__(self, args):
        self.server_address = args['server_address']
        self.server_port = int(args['server_port'])
        self.endpoint = args['server_endpoint']
        self.is_available = False
        self.destination_address = None
        self.http_send_handler = self.__determine_http_send_handler(args['http_send_handler'])

    def __get_destination_address(self):
       return "http://%s:%s/%s" % (self.server_address, self.server_port, self.endpoint)

    def __refresh_destination_address(self):
        self.destination_address = self.__get_destination_address()

    def __check_response_is_ok(self, response):
        return response.status_code == 200

    def init(self):
        self.__refresh_destination_address()
        return True

    def open(self):
        self.__refresh_destination_address()
        return True

    def close(self):
        self.is_available = False
        return True

    def deinit(self):
        return True

    def send(self, msg):
        try:
            response = self.http_send_handler(self.destination_address, msg)
            self.is_available = self.__check_response_is_ok(response)
        except requests.ConnectionError:
            self.is_available = False
        return self.is_available

