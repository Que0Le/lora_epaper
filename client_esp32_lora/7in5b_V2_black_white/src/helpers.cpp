

void reset_vars(int *img_rqst_sent, int *flag, int *request_status, unsigned int *written_bytes,
                unsigned char *shard_max_length, unsigned char *nbr_chunks,
                unsigned int *session_id, int *current_chunk, int *current_shard)
{
    *img_rqst_sent = -1;
    *flag = -1;
    *request_status = -1;
    *written_bytes = 0;
    *shard_max_length = 0;
    *nbr_chunks = 0;
    *session_id = 0;
    *current_chunk = -1;
    *current_shard = -1;
}