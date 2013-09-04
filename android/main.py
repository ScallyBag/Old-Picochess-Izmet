import kivy
from kivy.app import App
from kivy.uix.button import Button
from kivy.uix.boxlayout import BoxLayout
from kivy_util import ScrollableLabel
from kivy.uix.label import Label
from kivy.uix.popup import Popup
from kivy.uix.textinput import TextInput
import subprocess
import sys
from threading import Thread
from uci import UCIEngine

#
#try:
#    from Queue import Queue, Empty
#except ImportError:
#    from queue import Queue, Empty  # python 3.x


ON_POSIX = 'posix' in sys.builtin_module_names
PICO_OUTPUT = ''


class Picochess(App):
    def build(self):
        self.picochess_connected = False
        window = BoxLayout(orientation='vertical')
        top_bar = BoxLayout(orientation='horizontal', size_hint=(1,0.1))

        self.pico_bt = Button(markup=True)
        #fwd_bt.background_normal="img/empty-d.png"
        self.pico_bt.text="Connect Pico"

        self.pico_bt.bind(on_press=self.operate_pico)
        top_bar.add_widget(self.pico_bt)

        default_device_str = '/dev/ttyUSB0'

        if self.get_platform() =='macosx':
            default_device_str = '/dev/cu.usbserial-00004006'

        self.device = TextInput(text=default_device_str, multiline = False)
        top_bar.add_widget(self.device)
        window.add_widget(top_bar)

        self.pico_log = ScrollableLabel(PICO_OUTPUT)

        window.add_widget(self.pico_log)
        self.eng_process = None

        self.uci_engine = None
#        if self.is_desktop():
#            self._keyboard = Window.request_keyboard(
#                self._keyboard_closed, self)
#            self._keyboard.bind(on_key_down=self._on_keyboard_down)

        self.start_engine_thread()
        self.use_engine = False

        return window

    def operate_pico(self, bt):
        def connect_pico():
            print "Connecting pico"
#            popup = Popup(title='DGT board device', content=TextInput(text='/dev/ttyUSB0'))
##            popup.add_widget(Button(markup=True, size_hint=(0.2,0.2)))
#            popup.open()
            self.start_engine()

            self.picochess_connected = True
            self.pico_bt.text="Disconnect Pico"

        def disconnect_pico():
            print "Disconnecting pico"
            self.picochess_connected = False
            self.pico_bt.text="Connect Pico"
            self.uci_engine.quit()
            self.uci_engine = None

        if self.picochess_connected:
            disconnect_pico()
        else:
            connect_pico()

    def start_engine_thread(self):
        t = Thread(target=self.update_engine_output, args=(self.pico_log,))
        t.daemon = True # thread dies with the program
        t.start()

    def get_platform(self):
        return kivy.utils.platform()

    def start_engine(self):
        eng_exec = 'engines/stockfish-arm'
        if self.get_platform() =='macosx':
            eng_exec = 'engines/stockfish-mac'
        else:
            eng_exec = 'engines/stockfish-arm'
        uci_engine = UCIEngine(['su','-c', eng_exec+' dgt '+self.device.text])
#        uci_engine.start()
#        uci_engine.configure({'Threads': '1'})

        # Wait until the uci connection is setup
#        while not uci_engine.ready:
#            uci_engine.registerIncomingData()

#        uci_engine.startGame()
        # uci_engine.requestMove()
        self.uci_engine=uci_engine


    def update_engine_output(self, output):
#        if not self.use_engine:
#            self.start_engine()

#        def parse_score(line):
#            analysis_board = ChessBoard()
#            analysis_board.setFEN(self.chessboard.getFEN())
#            tokens = line.split()
#            try:
#                score_index = tokens.index('score')
#            except ValueError:
#                score_index = -1
#            score = None
#            move_list = []
#            if score_index!=-1 and tokens[score_index+1]=="cp":
#                score = float(tokens[score_index+2])/100*1.0
#            try:
#                line_index = tokens.index('pv')
#                for mv in tokens[line_index+1:]:
#                    analysis_board.addTextMove(mv)
#                    move_list.append(analysis_board.getLastTextMove())
#
#            except ValueError:
#                line_index = -1
#            variation = self.generate_move_list(move_list,start_move_num=self.chessboard.getCurrentMove()+1) if line_index!=-1 else None
#
#            del analysis_board
#            if variation and score:
#                return move_list, "[b]%s[/b][color=0d4cd6][ref=engine_toggle]         Stop[/ref][/color]\n[color=77b5fe]%s[/color]" %(score,"".join(variation))

        while True:
            if self.uci_engine:
                line = self.uci_engine.getOutput()

                if line:
#                    output.children[0].text+=line
                    output.update(line)
#                    out_score = parse_score(line)
#                    if out_score:
#                        raw_line, cleaned_line = out_score
#                        if cleaned_line:
#                            output.children[0].text = cleaned_line
#                        if raw_line:
#                            output.raw = raw_line
            elif output.children[0].text != PICO_OUTPUT:
                output.children[0].text = PICO_OUTPUT






if __name__ == '__main__':
    Picochess().run()
