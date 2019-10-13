import serial.tools.list_ports
import threading
import math
try:
    # for Python2
    import Tkinter as tk
except ImportError:
    # for Python3
    import tkinter as tk


class StoppableThread(threading.Thread):
    """Thread class with a stop() method. The thread itself has to check
    regularly for the stopped() condition."""

    def __init__(self, _name, _label):
        super(StoppableThread, self).__init__()
        self.name = _name
        self.label = _label
        self._stop_event = threading.Event()

    def run(self):
        # target function of the thread class
        import esptool
        import time
        # Spin forever
        while True:
            # Actually stop if we're told to stop
            if(self.stopped()):
                return

            # Draw the label
            self.label.config(text="Attempting flash on " +
                              self.name, bg="gray")

            # Try to flash the firmware
            try:
                esptool.main(["-b", "2000000", "--port", self.name, "write_flash", "-fm", "dio", "0x00000", "image.elf-0x00000.bin", "0x10000",
                              "image.elf-0x10000.bin", "0x1FB000", "blank.bin", "0x1FC000", "esp_init_data_default_v08.bin", "0x1FE000", "blank.bin"])
                # It worked! Display a nice green message
                self.label.config(
                    text="Flash succeeded on " + self.name, bg="green")
                time.sleep(4)
            except:
                # It failed, just wait a second a try again
                time.sleep(1)

    def stop(self):
        self._stop_event.set()

    def stopped(self):
        return self._stop_event.is_set()


class ProgrammerApplication:

    def init(self):
        # First get a list of all serial ports
        comList = serial.tools.list_ports.comports()
        programmerComPorts = []
        for comPort in comList:
            print(comPort)
            # If the VID and PID match the CP2012N's values, use it
            if(comPort.vid == 0x10C4 and comPort.pid == 0xEA60):
                programmerComPorts.append(str(comPort.device))
        print("programmerComPorts: " + str(programmerComPorts))

        # Create the UI root
        self.root = tk.Tk()

        # Figure out how many rows and columns to display, each com port gets a cell
        numCols = int(math.ceil(math.sqrt(float(len(programmerComPorts)))))
        numRows = int(math.ceil(len(programmerComPorts) / float(numCols)))
        for col in range(numCols):
            tk.Grid.columnconfigure(self.root, col, weight=1)
        for row in range(numRows):
            tk.Grid.rowconfigure(self.root, row, weight=1)

        # Keep track of all the threads so they can be stopped later
        self.threads = []

        # Keep track of where Labels are placed in the grid
        rowIdx = 0
        colIdx = 0

        # For each com port
        for comPort in programmerComPorts:
            # Make a label and add it to the grid
            comPortLabel = tk.Label(self.root, text=comPort)
            comPortLabel.grid(row=rowIdx, column=colIdx, sticky=[
                tk.W, tk.E, tk.N, tk.S])
            colIdx = colIdx + 1
            if(colIdx == numCols):
                colIdx = 0
                rowIdx = rowIdx + 1
            # Then create a thread for esptool associated with this com port and label and run it
            programmerThread = StoppableThread(
                _name=comPort, _label=comPortLabel)
            self.threads.append(programmerThread)
            programmerThread.start()

        # Set up how to exit
        self.exitButton = tk.Button(
            self.root, text="Exit", command=exit_function)
        self.exitButton.grid(row=numRows+1, columnspan=numCols,
                             sticky=[tk.W, tk.E])
        self.root.protocol("WM_DELETE_WINDOW", exit_function)

        # Start the main UI loop
        self.root.winfo_toplevel().title("Swadge Programmer")
        self.root.mainloop()

    def exit(self):
        self.exitButton.config(text="Waiting for threads to finish")
        # Tell all threads to stop
        for thread in self.threads:
            thread.stop()
        # Wait for all threads to actually stop
        for thread in self.threads:
            thread.join()
        # Destroy the UI
        self.root.destroy()


def exit_function():
    app.exit()


if __name__ == "__main__":
    app = ProgrammerApplication()
    app.init()
