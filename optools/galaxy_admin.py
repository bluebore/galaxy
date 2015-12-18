import sys
import galaxy_pb2
import ins_sdk
import uuid
import time


nexus_servers = raw_input("Enter nexus servers:")
if not nexus_servers:
    print "nexus is required"
    sys.exit(-1)
nexus = ins_sdk.InsSDK(nexus_servers)
 
def import_data(file_path, user_folder, quota_folder):
    fd = open(file_path, "rb")
    for line in fd.readlines():
        parts = line.replace("\n","").split(" ")
        user = galaxy_pb2.User()
        user.name = parts[0]
        user.password = parts[1]
        user.super_user = False
        if parts[2] == "true":
            user.super_user = True
        user.workspace = "/home/"+user.name
        user.create_time = long(time.time() * 1000)
        user.uid = str(uuid.uuid1())
        ok = nexus.put(user_folder + "/" + user.uid, user.SerializeToString())
        if ok :
            print "put user %s with id %s ok"%(user.name, user.uid)
        quota = galaxy_pb2.Quota()
        quota.target = user.uid
        if parts[3].endswith("T"):
            quota.mem_assigned = int(parts[3].replace("T","")) * 1024 * 1024 * 1024 * 1024
        elif parts[3].endswith("G"):
            quota.mem_assigned = int(parts[3].replace("G"),"") * 1024 * 1024 * 1024
        elif parts[3].endswith("M"):
            quota.mem_assigned = int(parts[3].replace("M"),"") * 1024 * 1024
        quota.cpu_assigned = long(parts[4])
        quota.name = "%s 's quota"%user.name
        quota.type = galaxy_pb2.kUserQuota
        quota.qid = str(uuid.uuid1())
        ok = nexus.put(quota_folder + "/" + quota.qid,
                       quota.SerializeToString())
        if ok:
            print "put quota %s with id %s ok"%(quota.name, quota.qid)

def main():
    data_path = raw_input("Enter data path:")
    if not data_path:
        print "data path is required"
        sys.exit(-1)
    user_folder = raw_input("Enter user folder:")
    if not user_folder:
        print "root user folder is required"
        sys.exit(-1)
    quota_folder = raw_input("Enter quota folder:")
    if not quota_folder:
        print "root quota_folder is required"
        sys.exit(-1)
    import_data(data_path, user_folder, quota_folder)

if __name__ == "__main__":
    main()
