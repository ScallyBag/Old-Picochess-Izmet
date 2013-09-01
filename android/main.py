from kivy.app import App
from kivy.uix.button import Button
from kivy.uix.boxlayout import BoxLayout
from kivy_util import ScrollableLabel
from kivy.uix.label import Label
from kivy.uix.popup import Popup

class Picochess(App):
    def build(self):
        self.picochess_connected = False
        b = BoxLayout(orientation='vertical')

        self.pico_bt = Button(markup=True, size_hint=(0.2,0.2))
        #fwd_bt.background_normal="img/empty-d.png"
        self.pico_bt.text="Connect Pico"

        self.pico_bt.bind(on_press=self.operate_pico)
        b.add_widget(self.pico_bt)

        # save_bt.text="Save
        self.pico_log = ScrollableLabel('Pico Output')
        # self.pico_log.children[0].text="[color=fcf7da]%s[/color]"%score

        # ref_callback=self.output_action
        b.add_widget(self.pico_log)

        return b

    def operate_pico(self, bt):
        def connect_pico():
            print "Connecting pico"
            popup = Popup(title='DGT board command', content=Label(text='dgt /dev/ttyUSB0'))
            popup.open()

            self.picochess_connected = True
            self.pico_bt.text="Disconnect Pico"

        def disconnect_pico():
            print "Disconnecting pico"
            self.picochess_connected = False
            self.pico_bt.text="Connect Pico"

        if self.picochess_connected:
            disconnect_pico()
        else:
            connect_pico()




if __name__ == '__main__':
    Picochess().run()
