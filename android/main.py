import kivy
from kivy.app import App
from kivy.uix.button import Button
from kivy.uix.boxlayout import BoxLayout
from kivy.uix.gridlayout import GridLayout
from kivy_util import ScrollableLabel
from kivy.uix.label import Label
from kivy.uix.popup import Popup
from kivy.uix.textinput import TextInput
from kivy.uix.checkbox import CheckBox
from kivy.uix.switch import Switch
from kivy.uix.togglebutton import ToggleButton
import subprocess
import sys
from threading import Thread
from uci import UCIEngine
import os

ON_POSIX = 'posix' in sys.builtin_module_names

PICO_NOENGINE_OUTPUT = 'No Engine output yet ..'
BLANK_OUTPUT = ''

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

        options_grid = GridLayout(cols=2)
        self.superuser_checkbox = Switch(active=True)

        options_grid.add_widget(Label(text='Superuser'))
        options_grid.add_widget(self.superuser_checkbox)
        top_bar.add_widget(options_grid)

        window.add_widget(top_bar)

        self.pico_log = ScrollableLabel(PICO_NOENGINE_OUTPUT)

        window.add_widget(self.pico_log)
        self.eng_process = None

        self.uci_engine = None

        self.start_engine_thread()
        self.use_engine = False

        return window

    def operate_pico(self, bt):
        def connect_pico():
            print "Connecting pico"
            try:
                self.start_engine()
                self.picochess_connected = True
                self.pico_bt.text="Disconnect Pico"
            except OSError:
                self.pico_log.update("Sorry, your phone/tablet is not rooted. You need to root it before using the DGT board.")
                self.picochess_connected = False

        def disconnect_pico():
            print "Disconnecting pico"
            self.picochess_connected = False
            self.pico_bt.text="Connect Pico"
            if self.get_platform() =='macosx':
                self.uci_engine.quit()
            else:
                # android
                try:
                    if self.superuser_checkbox.active:
                        subprocess.call(['su','-c', 'pkill stockfish-arm'])
                    else:
                        subprocess.call(['pkill','stockfish-arm'])
                except OSError:
                    self.pico_log.update("Could not disconnect pico, nothing was connected?")
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
        uci_engine = None
        if self.get_platform() =='macosx':
#            print "starting mac os x engine"
            eng_exec = 'engines/stockfish-mac'
#            print "command: "+ eng_exec +' dgt '+self.device.text
            uci_engine = UCIEngine([eng_exec, 'dgt', self.device.text])
        else:
            eng_exec = 'engines/stockfish-arm'
            # Check file permissions if android
            if not os.access(eng_exec,os.X_OK):
                oct(os.stat(eng_exec).st_mode & 0755)
                os.chmod(eng_exec, 0755)
                if self.superuser_checkbox.active:
                    uci_engine = UCIEngine(['su','-c', eng_exec+' dgt '+self.device.text])
                else:
                    uci_engine = UCIEngine([eng_exec,' dgt ',self.device.text])
        self.uci_engine=uci_engine

    def update_engine_output(self, output):
        while True:
            if self.uci_engine:
                if output.children[0].text == PICO_NOENGINE_OUTPUT:
                    output.children[0].text = BLANK_OUTPUT
                line = self.uci_engine.getOutput()
                if line:
                    output.update(line)
            else:
                if output.children[0].text != PICO_NOENGINE_OUTPUT:
                    output.children[0].text = PICO_NOENGINE_OUTPUT

if __name__ == '__main__':
    Picochess().run()
