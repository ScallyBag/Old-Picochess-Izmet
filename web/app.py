#!/usr/bin/python

import tornado.ioloop
import tornado.web
import tornado.auth
import os
import logging
import sys

from handlers import *

try:
    cookie_secret = os.environ['BSALT']
except KeyError:
    cookie_secret = 'CHANGE_ME'

settings = {
    "static_path": os.path.join(os.path.dirname(__file__), "static"),
    "autoescape": None,
    "cookie_secret": cookie_secret
}

logging.basicConfig(
    stream=sys.stdout,
    level=logging.DEBUG,
    format='"%(asctime)s %(levelname)8s %(name)s - %(message)s"',
    datefmt='%H:%M:%S'
)


application = tornado.web.Application([
    # Web app part
    (r"/", PGNHandler),
    (r"/game.pgn", PGNHandler),
], debug=True, **settings)


if __name__ == "__main__":
    application.listen(8888)
    tornado.ioloop.IOLoop.instance().start()
