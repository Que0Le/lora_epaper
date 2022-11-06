import os, sys, logging
import time, random, string, struct, array, json, zlib, datetime
# from xmlrpc.client import Boolean

from LoRaRF import SX127x

from lora_helper import *
from utils import *
from profiles import *
from msg_helper import *

currentdir = os.path.dirname(os.path.realpath(__file__))
sys.path.append(os.path.dirname(os.path.dirname(currentdir)))


msg_type =""
this_host_code = 123
chunks = []
current_session = -1
max_try = 10
current_tries = 0
last_rqst_shard = -1
last_rqst_chunk = -1
inactive_timeout_sec = 5
last_active_time: datetime.datetime = 0


def main() -> int:
    # logging.info(f"######################## {'{:%d/%m/%Y %I:%M:%S %p}'.format(datetime.datetime.now())} ########################")
    # logging.basicConfig(
    #     handlers=[
    #         logging.FileHandler(filename="./log_records.txt", encoding='utf-8', mode='a+')],
    #     format='%(asctime)s %(message)s', datefmt='%m/%d/%Y %I:%M:%S %p', 
    #     level=logging.DEBUG)
    LoRa = init_lora()
    while True:
        print("waiting for message ...")
        LoRa.request()
        LoRa.wait()
        reiv_message = ""
        while LoRa.available() > 1 :
            reiv_message += chr(LoRa.read())
        counter = LoRa.read()

        if "msg_type" not in reiv_message:
            continue
        # Parse message
        msg_json = json.loads(reiv_message)
        host_id = msg_json["hid"]
        client_id = msg_json["cid"]
        session_id = msg_json["sid"]
        msg_type = msg_json["msg_type"]
        if this_host_code!=host_id:
            continue


        if current_session >= 1:
            # send false session msg and ignore this 
            if check_false_session_and_return_msg(session_id=session_id, current_session=current_session):
                continue
            # Check timeout
            if (datetime.datetime.now() - last_active_time).total_seconds > inactive_timeout_sec:
                send_abort_request_msg(msg=f"Timeout", session_id=current_session)
                chunks = []
                current_session = last_rqst_shard = last_rqst_chunk = -1
                current_tries = 0


        if msg_type == "img_rqst":
            # deny request if server is occuppied
            if current_session >= 1:
                send_deny_request_msg(msg="occupied", session_id=0)
            board_type: str = msg_json["board"]
            display_type: str = msg_json["dsp_type"]
            # 1. prepare image: split data in chunk, each chunk contains shards of max_char bytes
            # 2. send number of chunk and shard back, and wait for request of shard from client.
            # 3. Set param
            chunks, chunk_sizes, total_length, compressed_length = get_data_for_request(
                board_type=board_type, display_type=display_type)
            rand_int = random.randint(1, 127)
            if send_accept_img_request(
                LoRa=LoRa, chunk_sizes=chunk_sizes, host_code=this_host_code, flag=0,
                session_id=rand_int, is_encrypted=1, is_compressed=1, request_status=1
                ):
                current_session = rand_int
                last_active_time = datetime.datetime.now()


        if msg_type == "rqst_done" and current_session >= 1:
            chunks = []
            current_session = last_rqst_shard = last_rqst_chunk = -1
            current_tries = 0
        # 
        if msg_type == "shard_rqst":
            # Not expect such request
            if current_session<1:
                send_abort_request_msg(msg=f"No such session", session_id=current_session)
                continue

            shard_id = 0
            chunk_id = 0
            # if request the same chunk and shard, check if need to abort
            if last_rqst_shard == shard_id and last_rqst_chunk==chunk_id:
                if current_tries >= max_try:
                    send_abort_request_msg(msg=f"max_try={max_try};chunk={chunk_id};shard={shard_id}", session_id=current_session)
                current_tries = current_tries + 1
            last_rqst_shard = shard_id
            last_rqst_chunk = chunk_id
            # if shard and chunk numbers are correct, send the shard.
            send_shard(chunk_id=chunk_id,shard_id=shard_id, session_id=current_session)
            last_active_time = datetime.datetime.now()

