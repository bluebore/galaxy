import subprocess
import sys
import os
import json
def get_jobs():
    p = subprocess.Popen(["./galaxy","jobs"], stdout=subprocess.PIPE)
    out, err = p.communicate()
    if err:
        print "fail to list jobs"
        sys.exit(-1)
    lines = out.splitlines()
    # job id and job name pair
    jobs = {}
    for line in lines[2:]:
        parts = line.split("  ")
        if len(parts) < 3:
            continue
        jobs[parts[2].strip()] = parts[3].strip()
    return jobs

def get_all_pods(jobs):
    pods = {}
    for jobid in jobs:
        print "get pods from job %s"%jobid
        p = subprocess.Popen(["./galaxy", "pods", "--j=%s"%jobid], stdout=subprocess.PIPE)
        out, err = p.communicate() 
        lines = out.splitlines()
        for line in lines[2:]:
            parts = line.split("  ")
            if len(parts) < 3:
                continue
            pods[parts[2].strip()] = {"jobid": jobid, "name":jobs[jobid], "state":parts[3].strip()}
    return pods

def preempt(pods, jobid, podid, endpoint):
    pods_on_agent = []
    p = subprocess.Popen(["./galaxy", "pods", "--e=%s"%endpoint], stdout=subprocess.PIPE)
    out, err = p.communicate()
    if err:
        print "fail to ./galaxy pods -e %s for err %s"%(endpoint, err)
        sys.exit(-1)
    lines = out.splitlines()
    print " %s will preempt the following pods on %s:"%(pods[podid]["name"], endpoint)
    for line in lines[2:]:
        parts = line.split("  ")
        if len(parts) < 2:
            continue
        ipodid = parts[2].strip()
        if ipodid not in pods:
            continue
        name = pods[ipodid]["name"]
        if name.find("nfs") != -1:
            continue
        pods_on_agent.append(ipodid)
        print name
    yes = raw_input('sure to preempt (y/n):')
    if yes != "y":
        return False
    preempt_json = {
                    "addr":endpoint,
                        "pending_pod":{
                            "jobid":jobid,
                            "podid":podid
                        },
                        "preempted_pods":[]
                   }
    for pod in pods_on_agent:
        preempt_json["preempted_pods"].append({"jobid": pods[pod]["jobid"], "podid":pod})
    filename = endpoint.replace(".", "_").replace(":", "_")+ ".json"
    with open(filename, "wb+") as fd:
        content = json.dumps(preempt_json)
        fd.write(content)
    p = subprocess.Popen(["./galaxy", "preempt", "--f=%s"%filename], stdout=subprocess.PIPE)
    out, err = p.communicate()
    if p.returncode == 0:
        return  True
    return False

def real_main(jobid, podid, endpoint):
    pods = {}
    if os.path.isfile("pods_cache"):
        print "use pods_cache to load pods"
        with open("pods_cache", "rb") as fd:
            pods = json.load(fd)
    else:
        print "get pods from galaxy"
        jobs = get_jobs()
        pods = get_all_pods(jobs)
        with open("pods_cache", "wb") as fd:
            fd.write(json.dumps(pods))
    ok = preempt(pods, jobid, podid, endpoint)
    if ok :
        print "preempt for pod %s successfully"%podid
    else:
        print "fail to preempt for pod %s "%podid

if __name__ == "__main__":
    if len(sys.argv) < 4:
        print "python preempt.py jobid podid endpoint" 
        sys.exit(-1)
    real_main(sys.argv[1], sys.argv[2], sys.argv[3])

