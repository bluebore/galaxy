import sys
import galaxy_pb2
import ins_sdk
import uuid
def main():
    nexus_servers = raw_input("Enter nexus servers:")
    if not nexus_servers:
        print "nexus is required"
        sys.exit(-1)
    nexus = ins_sdk.InsSDK(nexus_servers)
    admin_id = raw_input("Enter total admin id key:")
    if not admin_id:
        print "total admin is required"
        sys.exit(-1)
    total_quota_folder = raw_input("Enter total quota folder:")
    if not total_quota_folder:
        print "total quota is required"
        sys.exit(-1)
    name = raw_input("Enter quota name key:")
    if not name:
        print "total quota name is required"
        sys.exit(-1)
    millicores = raw_input("Enter cluster millicores:")
    if not millicores:
        print "millicores is required"
        sys.exit(-1)
    memory = raw_input("Enter cluster memory:")
    if not memory:
        print "memory is required"
        sys.exit(-1)
    quota = galaxy_pb2.Quota()
    quota.qid = str(uuid.uuid1())
    quota.name = name
    quota.target = admin_id
    quota.cpu_assigned = long(millicores)
    quota.mem_assigned = long(memory)
    value = user.SerializeToString()
    print nexus.put(total_quota_folder + "/" + quota.qid, value)

if __name__ == "__main__":
    main()
