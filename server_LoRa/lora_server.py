import os, sys
from typing import List
from xmlrpc.client import Boolean
currentdir = os.path.dirname(os.path.realpath(__file__))
sys.path.append(os.path.dirname(os.path.dirname(currentdir)))
from LoRaRF import SX127x
import time
import random
import string
import struct
import array

def get_bmp_data(filepath: str) -> bytes:
    with open(filepath, mode='rb') as file: # b is important -> binary
        fileContent = file.read()

    bmp_raw_data = fileContent[62:]
    print(len(bmp_raw_data))
    bmp_corrected = []
    hl_mask = 0b11110000
    hr_mask = 0b00001111
    pieces = []
    byte_per_line = 28
    # print(len(bmp_raw_data)/byte_per_line)
    for i in range(0, int(len(bmp_raw_data)/byte_per_line)):
        current_line = bmp_raw_data[i*byte_per_line:(i+1)*byte_per_line]
        for j in range(0, byte_per_line-1):
            current_byte = current_line[j]
            for byte_side in ["left", "right"]:
                if byte_side=="left":
                    if len(pieces)==0:  # use as left
                        pieces.append(hl_mask & current_line[j])
                    else:
                        pieces.append((hl_mask & current_line[j]) >> 4)
                else:
                    if j<(byte_per_line-2):
                        if len(pieces)==0:
                            pieces.append((hr_mask & current_line[j]) << 4)
                        else:
                            pieces.append(hr_mask & current_line[j])

                if len(pieces)==2:
                    bmp_corrected.append(pieces[0] | pieces[1])
                    pieces = []

    return bmp_corrected

# def flip_and_rotate(bmp_corrected, R: int = 212, C: int = 13, BMP_HEADER_LEN: int = 62):
#     """R number of rows, C number of colume devided by 8"""
#     for r in range(0, int(R/2)):
#         for col in range(0, C):
#             temp_byte = bmp_corrected[r*C+col + BMP_HEADER_LEN]
#             bmp_corrected[r*C+col + BMP_HEADER_LEN] = bmp_corrected[(R-r-1)*C+col + BMP_HEADER_LEN]
#             bmp_corrected[(R-r-1)*C+col + BMP_HEADER_LEN] = temp_byte

#     return bmp_corrected

def get_corrected_and_rotated_bmp(bmp_raw_data, R: int = 212, C: int = 13):
    """"""
    mask_bits = [0b10000000, 0b01000000, 0b00100000, 0b00010000, 0b00001000, 0b00000100, 0b00000010, 0b00000001]
    corrected_rotated_bmp = []
    for column_th in range(0, 27):
        bits_to_take = 8 if column_th!=26 else 4
        for bit_th in range(0, bits_to_take):   # bit_th of the output
            for eight_row_th in range(0, 13): #8-rows
                bits = []
                output_byte = 0
                for row_th_of_8 in range(0, 8):  #row_th in vertical
                    bits.append((mask_bits[bit_th] & bmp_raw_data[(eight_row_th*8+row_th_of_8)*28+column_th]) << bit_th)
                # Store result of current byte
                for i in range(0, 8):
                    output_byte = output_byte | (bits[i] >> i)
                # bits = []
                corrected_rotated_bmp.append(output_byte)
    return corrected_rotated_bmp


def get_bmp_raw_data_from_file(filepath: str, include_header: bool = False):
    with open(filepath, mode='rb') as file: # b is important -> binary
        fileContent = file.read()
    if include_header:
        return fileContent
    return fileContent[62:]


# print(get_bmp_raw_data_from_file("den.bmp"))



    # for (int r=0; r<int(R/2); r++) {
    #     for (int col=0; col<C; col++) {
    #         unsigned char temp = buff_48k[r*C+col + BMP_HEADER_LEN];
    #         buff_48k[r*C+col + BMP_HEADER_LEN] = buff_48k[(R-r-1)*C+col + BMP_HEADER_LEN];
    #         buff_48k[(R-r-1)*C+col + BMP_HEADER_LEN] = temp;
    #     }
    # }          

bmp_black_corrected: List = get_corrected_and_rotated_bmp(get_bmp_raw_data_from_file("den.bmp"))
# bmp_black_corrected: List = ([0]*1 + [255]*12 + [0]*5 + [255]*8 + [0]*13  + [255]*2717)#get_bmp_data("den.bmp")
# bmp_black_corrected: List = get_corrected_and_rotated_bmp([0]*300 +  [255]*2456)#get_bmp_data("den.bmp")
# bmp_black_corrected: List = get_corrected_and_rotated_bmp([0]*20 + [255]*8 + [0]*10 + [255]*28 + [0]*8 + [255]*20  + [255]*2662)#get_bmp_data("den.bmp")

bmp_red_corrected: List = get_corrected_and_rotated_bmp(get_bmp_raw_data_from_file("do.bmp"))
# bmp_red_corrected: List = ([255]*2717 + [255]*3+[0]*10+ [255]*3+[0]*10+[255]*3+[0]*10)#get_bmp_data("do.bmp")
# bmp_red_corrected: List = get_corrected_and_rotated_bmp([255]*2700 + [0]*56)#get_bmp_data("do.bmp")
# bmp_red_corrected: List = get_corrected_and_rotated_bmp([255]*2662 + [0]*20 + [255]*8 + [0]*10 + [255]*28 + [0]*8 + [255]*20)#get_bmp_data("do.bmp")

pic_data = [bmp_black_corrected, bmp_red_corrected]

# Begin LoRa radio and set NSS, reset, busy, IRQ, txen, and rxen pin with connected Raspberry Pi gpio pins
# IRQ pin not used in this example (set to -1). Set txen and rxen pin to -1 if RF module doesn't have one
busId = 0; csId = 0
resetPin = 22; irqPin = -1; txenPin = -1; rxenPin = -1
LoRa = SX127x()
print("Begin LoRa radio")
if not LoRa.begin(busId, csId, resetPin, irqPin, txenPin, rxenPin) :
    raise Exception("Something wrong, can't begin LoRa radio")

# Set frequency to 915 Mhz
print("Set frequency to 915 Mhz")
LoRa.setFrequency(866000000)

# Set TX power, this function will set PA config with optimal setting for requested TX power
print("Set TX power to +17 dBm")
LoRa.setTxPower(17, LoRa.TX_POWER_PA_BOOST)                     # TX power +17 dBm using PA boost pin

# Configure modulation parameter including spreading factor (SF), bandwidth (BW), and coding rate (CR)
# Receiver must have same SF and BW setting with transmitter to be able to receive LoRa packet
print("Set modulation parameters:\n\tSpreading factor = 7\n\tBandwidth = 125 kHz\n\tCoding rate = 4/5")
LoRa.setSpreadingFactor(7)                                      # LoRa spreading factor: 7
LoRa.setBandwidth(125000)                                       # Bandwidth: 125 kHz
LoRa.setCodeRate(5)                                             # Coding rate: 4/5

# Configure packet parameter including header type, preamble length, payload length, and CRC type
# The explicit packet includes header contain CR, number of byte, and CRC type
# Receiver can receive packet with different CR and packet parameters in explicit header mode
# print("Set packet parameters:\n\tExplicit header type\n\tPreamble length = 12\n\tPayload Length = 15\n\tCRC on")
# LoRa.setHeaderType(LoRa.HEADER_EXPLICIT)                        # Explicit header mode
# LoRa.setPreambleLength(12)                                      # Set preamble length to 12
# LoRa.setPayloadLength(15)                                       # Initialize payloadLength to 15
# LoRa.setCrcEnable(True)                                         # Set CRC enable

# Set syncronize word for public network (0x34)
print("Set syncronize word to 0x34")
LoRa.setSyncWord(0xF3)

print("\n-- LoRa Transmitter --\n")

counter = 0

#
host_code = 123# hex(123)
flag = 1#hex(1)    # 1: request accept, 
payload_length = 100#hex(100)
packet_format = "BBBIIB100s"
headerType = LoRa.HEADER_IMPLICIT
preambleLength = struct.calcsize(packet_format)
payloadLength = struct.calcsize(packet_format)
crcType = True
# LoRa.setLoRaPacket(headerType, preambleLength, payloadLength, crcType)
print(f"Set packet parameters:\n\tImplicit header type\n\tPreamble length = {preambleLength}\n\tPayload Length = {payloadLength}\n\tCRC {'on' if crcType else 'off'}")

# Transmit message continuously
# length_msg = 10
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
    # print((reiv_message))
    if "type=rqst_img" in reiv_message:
        print("reiv: " + reiv_message)
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
                LoRa.endPacket(250)
                LoRa.wait()

                # sub_str = sub_strings[i]
                # LoRa.beginPacket()
                # LoRa.put(sub_str.encode())
                # LoRa.endPacket()
                # LoRa.wait()
                print(f"... sent {data_length} bytes {i+1}/{len(sub_strings)}: {sub_str}")
                time.sleep(1)
                # if i==3:
                #     break
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

    """
    # Transmit message and counter
    # write() method must be placed between beginPacket() and endPacket()
    LoRa.beginPacket()
    # LoRa.write(messageList, len(messageList))
    message = ''.join(random.choices(string.ascii_letters + string.digits, k=length_msg))
    message = message + f"___{str(length_msg)}"


    ml = list((message))
    for i in range(len(ml)): 
        ml[i] = ord(ml[i])
    LoRa.write(ml, len (ml))
    # LoRa.write([counter], 1)
    LoRa.endPacket()

    # Print message and counter
    print(f"Sending payload with len={length_msg} ")

    # Wait until modulation process for transmitting packet finish
    LoRa.wait()

    # Print transmit time and data rate
    print("Transmit time: {0:0.2f} ms | Data rate: {1:0.2f} byte/s".format(LoRa.transmitTime(), LoRa.dataRate()))

    # Don't load RF module with continous transmit
    time.sleep(1)
    counter = (counter + 1) % 256
    length_msg = length_msg + 5
    """
