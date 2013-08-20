from os.path import expanduser
import tornado

class PGNHandler(tornado.web.RequestHandler):
    def get(self):
        print "Downloading pgn.."
        gfile = open(expanduser("~/git/Stockfish/src/game.pgn"), "r")
        self.set_header('Content-Type', 'application/x-chess-pgn')
        self.set_header('Content-Disposition', 'attachment; filename=game.pgn')

        self.write(gfile.read())

    def post(self):
        self.render('../templates/charts.html', page="charts")
