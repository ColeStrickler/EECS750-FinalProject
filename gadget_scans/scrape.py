import os
import json
import sys
import getopt

'''
    Each line of format: file:line:num: REPORT @json

    Parse the json and (right now) filter in file name
'''
def check_filter(line: str, filters: list) -> bool:
    file_name = line.split("REPORT @")[0]
    for filt in filters:
        # filter in file, don't include it
        if filt in file_name:
            return False
    return True

def main(argv):
    if len(argv) < 1:
        print("Usage: scrape.py [-f filter1,filter2,...] [-i in dir] [-o out file]")
        sys.exit(1)
    try:
        opts, args = getopt.getopt(argv, "f:o:i:")
    except:
        print("Usage: scrape.py [-f filter1,filter2,...] [-i in dir] [-o out file]")
        sys.exit(1)

    filters = []
    out_file = None
    in_dir = None
    for opt, arg in opts:
        if opt in ['-f']:
            filters = arg.split(",")
        elif opt in ['-o']:
            out_file = arg
        elif opt in ['-i']:
            in_dir = arg

    if not os.path.exists(in_dir):
        print(f"{in_dir} not found")
        sys.exit(1)

    if not os.path.isdir(in_dir):
        print(f"{in_dir} is not a directory")
        sys.exit(1)

    out = open(out_file, "w")
    for f in os.listdir(in_dir):
        path = os.path.join(in_dir, f)
        if os.path.isfile(path):
            f = open(path, "r")
            for line in f.readlines():
                if check_filter(line, filters):
                    out.write(line)
            f.close()
    out.close()

if __name__ == "__main__":
    main(sys.argv[1:])
