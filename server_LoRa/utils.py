import os, glob


def get_bmp_raw_data_from_file(filepath: str, include_header: bool = False):
    with open(filepath, mode='rb') as file: # b is important -> binary
        fileContent = file.read()
    if include_header:
        return fileContent
    return fileContent[62:]

def list_file_with_filter(dirpath: str, has_str: str, file_extension: str):
    all_files_and_dirs = glob.glob(f"{dirpath}/*.{file_extension}")
    result = []
    for path in all_files_and_dirs:
        if has_str in path and os.path.isfile(path):
            result.append(path)
    return result