class DestTest(object):

    def init(self):
        return True

    def deinit(self):
        pass

    def open(self):
        return True

    def close(self):
        pass

    def is_open(self):
        return True

    def send(self, msg):
        with open('test-python.log', 'a') as f:
            f.write('{DATE} {HOST} {MSGHDR}{MSG}\n'.format(**msg))

        return True
