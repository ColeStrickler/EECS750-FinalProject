import sys
import getopt
import os
import json
import matplotlib.pyplot as plt
import scienceplots

def architecture_stats(gadgets):
    x86 = 0
    arm = 0
    other = 0
    not_arch = 0
    for f in gadgets.keys():
        num = len(gadgets[f])
        if "arch" in f:
            if "x86" in str(f):
                x86 += num
            elif "arm" in str(f):
                arm += num
            else:
                other += num
        else:
            not_arch += num
    return None, {"x86": x86, "arm": arm, "other": other, "not_arc": not_arch}

def device_driver_stats(gadgets):
    driver = 0
    bluetooth = 0
    sound = 0
    net = 0
    other = 0
    result = []
    for f in gadgets.keys():
        num = len(gadgets[f])
        if "drivers" in f:
            driver += num
        elif "bluetooth" in f:
            bluetooth += num
        elif "sound" in f:
            sound += num
        elif "net" in f:
            net += num
        else:
            other += num
            result.append(f)
    return result, {"driver":driver,"bluetooth":bluetooth,"sound":sound,"net":net,"other":other}

def in_both_stats(free, use):
    num = 0
    refined = {}
    shared_files = list(set(list(free.keys())) & set(list(use.keys())))
    for f in shared_files:
        funcs1 = [(r,ptr) for (r,_,ptr) in free[f]]
        funcs2 = [(r,ptr) for (r,_,ptr) in use[f]]
        join = list(set(funcs1) & set(funcs2))
        num_shared = len(join)
        if num_shared > 0:
            num += num_shared
            refined[f] = join
    return refined, {"in_both":num}

def parse_gadget(s: str) -> tuple:
    gadget_info = s.split("REPORT @")
    if len(gadget_info) < 2:
        return None

    location = gadget_info[0].split(":")
    g_file, g_loc = location[0], location[1]

    gadget_json = json.loads(gadget_info[1])
    g_func = gadget_json['target_func']
    g_ptr = None
    if 'free_struct_ptr' in gadget_json:
        g_ptr = gadget_json['free_struct_ptr']
    else:
        g_ptr = gadget_json['use_struct_ptr']
    return g_file, g_loc, g_func, g_ptr

def run(argv):
    FREE_DIR = ""
    USE_DIR = ""

    FREE_GADGETS = {}
    USE_GADGETS = {}

    if len(argv) < 4:
        print(f"Usage: {argv[0]} -f <free dir> -u <use dir>")
        sys.exit(1)
    try:
        opts, args = getopt.getopt(argv[1:], "f:u:")
    except:
        print(f"Usage: {argv[0]} -f <free dir> -u <use dir>")
        sys.exit(1)

    for opt, arg in opts:
        if opt == "-f":
            FREE_DIR = arg
        elif opt == "-u":
            USE_DIR = arg

    if not os.path.isdir(FREE_DIR):
        print(f"{FREE_DIR} isn't a directory")
    if not os.path.isdir(USE_DIR):
        print(f"{USE_DIR} isn't a directory")

    for f in os.listdir(FREE_DIR):
        gadgets = open(os.path.join(FREE_DIR,f), "r")
        for g in gadgets.readlines():
            gadget = parse_gadget(g)
            if gadget is not None:
                g_file, g_loc, g_func, g_ptr= gadget
                if g_file in FREE_GADGETS:
                    FREE_GADGETS[g_file].append((g_func, g_loc, g_ptr))
                else:
                    FREE_GADGETS[g_file] = [(g_func, g_loc, g_ptr)]
        gadgets.close()

    for f in os.listdir(USE_DIR):
        gadgets = open(os.path.join(USE_DIR,f), "r")
        for g in gadgets.readlines():
            gadget = parse_gadget(g)
            if gadget is not None:
                g_file, g_loc, g_func, g_ptr = gadget
                if g_file in USE_GADGETS:
                    USE_GADGETS[g_file].append((g_func, g_loc, g_ptr))
                else:
                    USE_GADGETS[g_file] = [(g_func, g_loc, g_ptr)]
        gadgets.close()

    print(" === Overall Statistics ===")
    for stat in [architecture_stats, device_driver_stats]:
        print(f"\t== {stat.__name__}  ==")
        print(f"\tUse: {stat(USE_GADGETS)[1]}")
        print(f"\tFree: {stat(FREE_GADGETS)[1]}")

    num_free = sum([len(gadgets) for gadgets in FREE_GADGETS.values()])
    num_use = sum([len(gadgets) for gadgets in USE_GADGETS.values()])
    total = num_free + num_use

    refined, stats = in_both_stats(FREE_GADGETS, USE_GADGETS)
    print(f"\t{stats}")
    print("=== Refined Statistics : Free and Use the same function ===")
    results, stats = device_driver_stats(refined)
    print(f"\t Total Gadgets: {total}")
    print(f"\t {stats}")
    for r_f in results:
        print(f"\t{r_f}: {refined[r_f]}")

    _, overall_use = device_driver_stats(USE_GADGETS)
    _, overall_free = device_driver_stats(FREE_GADGETS)

    #plt.style.use("ieee")
    plt.figure()
    colors = ["sandybrown", "papayawhip", "lemonchiffon", "darkgray", "whitesmoke"]

    plt.title("Overall Gadget Distribution - Use")
    overall_use = dict(reversed(sorted(overall_use.items(), key=lambda item:item[1])))
    plt.pie(list(overall_use.values()), labels=list(overall_use.keys()), colors=colors, autopct='%.0f%%', pctdistance=0.7,labeldistance=1.15)
    plt.savefig("overall_use.png")

    plt.clf()

    plt.title("Overall Gadget Distribution - Free")
    overall_free = dict(reversed(sorted(overall_free.items(), key=lambda item:item[1])))
    plt.pie(list(overall_free.values()), labels=list(overall_free.keys()), colors=colors, autopct='%.0f%%',pctdistance=0.7, labeldistance=1.15)
    plt.savefig("overall_free.png")

if __name__ == "__main__":
    run(sys.argv)
