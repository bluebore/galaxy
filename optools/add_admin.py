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
    user_folder = raw_input("Enter user folder:")
    if not user_folder:
        print "root user folder is required"
        sys.exit(-1)
    username = raw_input("Enter root user name:")
    if not username:
        print "root user name is required"
        sys.exit(-1)
    password = raw_input("Enter user password:")
    if not password:
        print "root user password is required"
        sys.exit(-1)
    user = galaxy_pb2.User()
    user.uid = str(uuid.uuid1())
    print user.uid
    user.name = username
    user.password = password
    user.super_user = True
    user.workspace = "/root"
    value = user.SerializeToString()
    print nexus.put(user_folder+"/"+user.uid, value)
   

if __name__ == "__main__":
    main()
