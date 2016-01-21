# -*- coding:utf-8 -*-
import os
import time
import datetime
import sys
# dump process_name io usage on device
class IoDump(object):
    def __init__(self, process_name=[], top_size = 10):
        self.process_data = {}
        self.process_name = process_name

    def readable(self, num):
        for unit in ['','K','M','G','T','P','E','Z']:
            if abs(num) < 1024.0:
                return "%3.1f%s"%(num, unit)
            num /= 1024.0
        return "%.1f%s" % (num, 'Y')

    def collect(self):
        entries = os.listdir("/proc")
        statistics = {}
        for entry in entries:
            try:
                pid = int(entry)
                exe_path = os.path.realpath("/proc/%d/exe"%pid)
                offset = exe_path.rfind("/")
                bin = exe_path[offset+1:]
                if bin in self.process_name:
                    status, read_bytes_ps, write_bytes_ps = self.calc_process(pid)
                    if not status:
                        continue
                    stat = statistics.get(bin,{"read_bytes_ps":0, "write_bytes_ps":0})
                    stat["read_bytes_ps"] += read_bytes_ps
                    stat["write_bytes_ps"] += write_bytes_ps
                    statistics[bin] = stat
            except Exception,e:
                pass
        stat_array = []
        for key in statistics:
            item = {"name":key}
            item.update(statistics[key])
            stat_array.append(item)
        sorted_array = sorted(stat_array, key=lambda item : item["read_bytes_ps"], reverse = True)
        output = []
        now = time.time()
        output.append("-------------------------------------------------")
        output.append(datetime.datetime.fromtimestamp(now).strftime('%Y-%m-%d %H:%M:%S'))
        for item in sorted_array:
            output.append("%s read:%s/s write:%s/s"%(item["name"], self.readable(item["read_bytes_ps"]),
                self.readable(item["write_bytes_ps"])))
        print "\n".join(output)

    def calc_process(self, pid):
        data = {'read':0,
                'write':0,
                'cancell':0}
        fd = open("/proc/%d/io"%pid, "r")
        for line in fd.readlines():
            parts = line.replace("\n","").split(":")
            if parts[0] == "read_bytes":
                data["read"] = int(parts[1][1:])
            elif parts[0] == "write_bytes":
                data["write"] = int(parts[1][1:])
            elif parts[0] == "cancelled_write_bytes":
                data["cancell"] = int(parts[1][1:])
        fd.close()
        if pid in self.process_data:
            old_data = self.process_data[pid]
            read_bytes_ps = data["read"] - old_data["read"]
            write_bytes_ps = (data["write"] - data["cancell"]) - (old_data["write"] - old_data["cancell"])
            self.process_data[pid] = data
            return True, read_bytes_ps, write_bytes_ps
        else:
            self.process_data[pid] = data
            return False, 0, 0

def main():
    dump = IoDump(sys.argv)
    while True:
        dump.collect()
        time.sleep(1)

if __name__ == "__main__":
    main()
