# -*- coding:utf-8 -*-
# Copyright (c) 2015, Galaxy Authors. All Rights Reserved
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
import galaxy_pb2
import ins_sdk
import uuid
import time
import settings
import json
import argparse
parser = argparse.ArgumentParser()
parser.add_argument("-a",
                    action="store",
                    dest="user_config",
                    help="add user with a config json")
parser.add_argument("-d",
                    action="store",
                    dest="delete_config",
                    help="delete user with a config json")
nexus = ins_sdk.InsSDK(settings.NEXUS_SERVERS)

def import_user_data(data_path):
    with open(data_path, "rb") as fd:
        users = json.load(fd)
        for user in users["users"]:
            user_pb = galaxy_pb2.User()
            user_pb.name = user["name"]
            user_pb.super_user = user["super"]
            user_pb.password = user["password"]
            user_pb.workspace = user["workspace"]
            user_pb.uid = str(uuid.uuid1())
            ok = nexus.put(settings.USER_PREFIX + "/" + user_pb.uid,
                           user_pb.SerializeToString())
            if ok:
                print "import user %s successfully"%user_pb.name
            else:
                print "fail import user %s"%user_pb.name
            quota = galaxy_pb2.Quota()
            quota.target = user_pb.uid
            quota.cpu_quota = user['cpu']
            memory = 0
            if user["memory"].endswith("T"):
                quota.memory_quota = int(user["memory"].replace("T","")) * 1024 * 1024 * 1024 * 1024
            elif user["memory"].endswith("G"):
                quota.memory_quota = int(user["memory"].replace("G"),"") * 1024 * 1024 * 1024
            elif user["memory"].endswith("M"):
                quota.memory_quota = int(user["memory"].replace("M"),"") * 1024 * 1024
            quota.name = "%s 's quota"%user_pb.name
            quota.type = galaxy_pb2.kUserQuota
            quota.qid = str(uuid.uuid1())
            ok = nexus.put(settings.QUOTA_PREFIX + "/" + quota.qid,
                           quota.SerializeToString())
            if ok:
                print "put quota for user %s successfully"%user_pb.uid
            else:
                print "fail to put quota for user %s "%user_pb.uid

def delete_user_data(data_path):
    with open(data_path, "rb") as fd:
        ids = json.load(fd)
        for id in ids["ids"]:
            ok = nexus.delete(settings.USER_PREFIX + "/" + id)
            if ok:
                print "delete %s successfully"%id
            else:
                print "fail to delete %s"%id

def main():
    args = parser.parse_args()
    if args.user_config:
        import_user_data(args.user_config)
    if args.delete_config:
        delete_user_data(args.delete_config)

if __name__ == "__main__":
    main()
