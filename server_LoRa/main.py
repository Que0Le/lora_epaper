import os, sys
from typing import List
import time, random, string, struct, array, json
# from xmlrpc.client import Boolean

from LoRaRF import SX127x

from lora_helper import *
from utils import *
from profiles import *
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
        # available() method return remaining received payload length and will decrement each read() or get() method called
        while LoRa.available() > 1 :
            reiv_message += chr(LoRa.read())
        counter = LoRa.read()
        print((reiv_message))
        if '"msg_type":"rqst_img"' not in reiv_message:
            continue

        print("reiv: " + reiv_message)
        msg_json = json.loads(reiv_message)
        board_type: str = msg_json["board"]
        display_type: str = msg_json["dsp_type"]
        uniq_id: str = msg_json["uniq_id"]
        if display_type not in SUPPORTED_DISPLAYS:
            continue

        ### Sent back accept:
        host_code = 0
        flag = 0
        packet_struct = [host_code, flag, 0, 0, "".encode()]
        send_message = struct.pack("BBBB30s", *packet_struct)
        send_a_struct(
            LoRa=LoRa, struct_data=packet_struct, format_characters="BBBB30s", endpacket_timeout=0
        )

        ###
        print(f"Begin trasmission for display_type: {display_type}")
        if display_type=="2in13b_V3":
            # Prepare data
            bmp_black_corrected: List = get_corrected_and_rotated_bmp_for_waveshare_epp(get_bmp_raw_data_from_file("images/den.bmp"))
            bmp_red_corrected: List = get_corrected_and_rotated_bmp_for_waveshare_epp(get_bmp_raw_data_from_file("images/do.bmp"))
            pic_data = [bmp_black_corrected, bmp_red_corrected]
            max_chars_length = 100
            for data_type in range(0, 2):   # type 0: black image, type 1: red
                sub_strings = [
                    pic_data[data_type][i:i+max_chars_length] for i in range(0, len(pic_data[data_type]), max_chars_length)
                ]
                print(f"Datatype: {data_type}")
                for i in range(0, len(sub_strings)):
                    # Prepare struct data
                    sub_str = sub_strings[i]
                    total_length = 123456
                    index_start = i*max_chars_length
                    data_length = len(sub_str)
                    values = array.array('B')
                    values.fromlist(sub_str)
                    packet_struct = [host_code, flag, data_type, total_length, index_start, data_length, values.tobytes()]
                    send_a_struct(
                        LoRa=LoRa, struct_data=packet_struct, format_characters="BBBIIB100s", endpacket_timeout=0
                    )
                    print(f"... sent {data_length} bytes {i+1}/{len(sub_strings)}, Transmit time: {LoRa.transmitTime():0.2f} ms")
                    # print("Transmit time: {0:0.2f} ms | Data rate: {1:0.2f} byte/s".format(LoRa.transmitTime(), LoRa.dataRate()))

                    time.sleep(0.3)
        elif display_type=="7in5b_V2":
            pass

        ### Sent done status:
        packet_struct = [host_code, flag, 0, 3, "".encode()]
        # send_message = struct.pack("BBBB30s", *packet_struct)
        # LoRa.beginPacket()
        # LoRa.put(send_message)
        # LoRa.endPacket(250)
        # LoRa.wait()
        send_a_struct(
            LoRa=LoRa, struct_data=packet_struct, format_characters="BBBB30s", endpacket_timeout=0
        )
        print("Sent done message ...")
        time.sleep(2)

        print("Done transmitting!")


    return 0

if __name__ == '__main__':
    sys.exit(main())  # next section explains the use of sys.exit