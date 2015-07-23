# -*- coding:utf-8 -*-
# Author: yuanyi03@baidu.com
# Date: 2015-07-16
import sys,os,getopt,logging, traceback
sys.path.append("../")
from galaxy import sdk

# ======================================== log =================================================
LOGFILE="./sdk_test2.log"
LOGLEVEL="NOTICE"
FORMAT="[%(levelname)s] %(asctime)s : %(pathname)s %(module)s:%(funcName)s[%(lineno)d] %(message)s"
LEVEL = {}
LEVEL['NOTICE'] = logging.NOTSET
LEVEL['DEBUG'] = logging.DEBUG
LEVEL['INFO'] = logging.INFO
LEVEL['WARNING'] = logging.WARNING
LEVEL['ERROR'] = logging.ERROR
LEVEL['CRITICAL'] = logging.CRITICAL

def InitLog():
    logger = logging.getLogger()
    hdlr = logging.FileHandler(LOGFILE)
    formatter = logging.Formatter(FORMAT)
    hdlr.setFormatter(formatter)
    logger.addHandler(hdlr)
    logger.setLevel(LEVEL[str(LOGLEVEL)])
    return logger

LOG=InitLog()

# ==============================================================================================

def usage():
    print "Usage: sdk_test2.py [options] ..."
    print "       --job_id job id"
    print "       --replicate_num "
    print "       --deploy_step_size"
    print "       -s master server addr"
    print "       --help show this"

def main():
    master_addr = "localhost:8102"
    job_id = 0
    replicate_num = 0
    deploy_step_size = -1
    try:
        opts,args = getopt.getopt(sys.argv[1:],"s:vh",["version","help", "job_id=", "replicate_num=", "deploy_step_size="])
    except getopt.GetoptError:
        sys.exit(2)

    for opt in opts: 
        if opt[0] == "s" :
            master_addr = opt[1]
        elif opt[0] == "--job_id" :
            job_id = int(opt[1])
        elif opt[0] == "--replicate_num" :
            replicate_num = int(opt[1])
        elif opt[0] == "--deploy_step_size" :
            deploy_step_size = int(opt[1])
        elif opt[0] == "--help" :
            usage()
            sys.exit(0)
        else : 
            LOG.critical("invalid args %s"%(str(opt)))
            sys.exit(-1)
    master = sdk.GalaxySDK(master_addr) 
    LOG.info("get master sdk")
    if deploy_step_size == -1 :
        ret = master.update_job(job_id, replicate_num)
    else :
        ret = master.update_job(job_id, replicate_num, deploy_step_size)
    if ret == False :
        LOG.critical("update job failed")
        return -1
    return 0

if __name__ == "__main__":
    try :
        ret = main()
    except Exception, e:
        LOG.critical("%s"%(traceback.print_exc()))
        sys.exit(-1)
    sys.exit(ret)
