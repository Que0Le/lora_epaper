import os, sys
from typing import List
import time, random, string, struct, array, json
# from xmlrpc.client import Boolean

from LoRaRF import SX127x

from lora_helper import init_lora
from lora_server import *
currentdir = os.path.dirname(os.path.realpath(__file__))
sys.path.append(os.path.dirname(os.path.dirname(currentdir)))




def main() -> int:
    LoRa = init_lora()
    while True :
        print("waiting for request ...")
        # Request for receiving new LoRa packet
        LoRa.request()
        # Wait for incoming LoRa packet
        LoRa.wait()

        # Put received packet to message and counter variable
        # read() and available() method must be called after request() or listen() method
        reiv_message = ""
        board_type = ""
        display_type = ""
        uniq_id = ""
        # available() method return remaining received payload length and will decrement each read() or get() method called
        while LoRa.available() > 1 :
            reiv_message += chr(LoRa.read())
        counter = LoRa.read()
        # print((reiv_message))
        if "msg_type=rqst_img" in reiv_message:
            print("reiv: " + reiv_message)
            msg_json = json.loads(reiv_message)
            ### Sent back accept:
            packet_struct = [host_code, flag, 0, 0, "".encode()]
            send_message = struct.pack("BBBB30s", *packet_struct)
            LoRa.beginPacket()
            LoRa.put(send_message)
            LoRa.endPacket(250)
            LoRa.wait()
            time.sleep(2)

            ###
            print("Begin trasmission: ")
            max_chars_msg = 100
            for data_type in range(0, 2):
                sub_strings = [pic_data[data_type][i:i+max_chars_msg] for i in range(0, len(pic_data[data_type]), max_chars_msg)]
                print(f"Datatype: {data_type}")
                for i in range(0, len(sub_strings)):
                    sub_str = sub_strings[i]
                    # Structured message
                    total_length = 123456
                    index_start = i*max_chars_msg
                    data_length = len(sub_str)
                    # packet_struct = [host_code, flag, data_type, total_length, index_start, data_length, sub_str.encode()]
                    values = array.array('B')
                    values.fromlist(sub_str)
                    print(str(sub_str))
                    packet_struct = [host_code, flag, data_type, total_length, index_start, data_length, values.tobytes()]
                    send_message = struct.pack(packet_format, *packet_struct)
                    LoRa.beginPacket()
                    LoRa.put(send_message)
                    LoRa.endPacket()
                    LoRa.wait()

                    print(f"... sent {data_length} bytes {i+1}/{len(sub_strings)}: {sub_str}")
                    time.sleep(0.2)


            ### Sent done status:
            packet_struct = [host_code, flag, 0, 3, "".encode()]
            send_message = struct.pack("BBBB30s", *packet_struct)
            LoRa.beginPacket()
            LoRa.put(send_message)
            LoRa.endPacket(250)
            LoRa.wait()
            print("Sent done message ...")
            time.sleep(2)

            print("Done transmitting!")


    return 0

if __name__ == '__main__':
    sys.exit(main())  # next section explains the use of sys.exit