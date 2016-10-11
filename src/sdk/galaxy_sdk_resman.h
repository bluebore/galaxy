// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BAIDU_GALAXY_SDK_RESMAN_H
#define BAIDU_GALAXY_SDK_RESMAN_H

#include "galaxy_sdk.h"

namespace baidu {
namespace galaxy {
namespace sdk {

class ResourceManager {
public:

    static ResourceManager* ConnectResourceManager(const std::string& nexus_addr, 
                                                   const std::string& path);
    virtual ~ResourceManager() = 0; 

    //SafeMode
    virtual bool EnterSafeMode(const EnterSafeModeRequest& request, 
                               EnterSafeModeResponse* response) = 0;
    virtual bool LeaveSafeMode(const LeaveSafeModeRequest& request, 
                               LeaveSafeModeResponse* response) = 0;

    //Status
    virtual bool Status(const StatusRequest& request, StatusResponse* response) = 0;
    virtual bool MasterEndpoint(const std::string& appmaster_path, 
                          std::string* appmaster_endpoint, 
                          std::string* resman_endpoint) = 0;

    //Container
    virtual bool CreateContainerGroup(const CreateContainerGroupRequest& request, 
                                      CreateContainerGroupResponse* response) = 0;
    virtual bool RemoveContainerGroup(const RemoveContainerGroupRequest& request, 
                                      RemoveContainerGroupResponse* response) = 0;
    virtual bool UpdateContainerGroup(const UpdateContainerGroupRequest& request, 
                                      UpdateContainerGroupResponse* response) = 0;
    virtual bool ListContainerGroups(const ListContainerGroupsRequest& request, 
                                     ListContainerGroupsResponse* response) = 0;
    virtual bool ShowContainerGroup(const ShowContainerGroupRequest& request, 
                                    ShowContainerGroupResponse* response) = 0;
    //Agent
    virtual bool AddAgent(const AddAgentRequest& request, AddAgentResponse* response) = 0;
    virtual bool RemoveAgent(const RemoveAgentRequest& request, RemoveAgentResponse* response) = 0;
    virtual bool ShowAgent(const ShowAgentRequest& request, ShowAgentResponse* response) = 0;
    virtual bool OnlineAgent(const OnlineAgentRequest& request, OnlineAgentResponse* response) = 0;
    virtual bool OfflineAgent(const OfflineAgentRequest& request, 
                              OfflineAgentResponse* response) = 0;
    virtual bool ListAgents(const ListAgentsRequest& request, ListAgentsResponse* response) = 0;

    //Tag
    virtual bool CreateTag(const CreateTagRequest& request, CreateTagResponse* response) = 0;
    virtual bool ListTags(const ListTagsRequest& request, ListTagsResponse* response) = 0;
    virtual bool ListAgentsByTag(const ListAgentsByTagRequest& request, 
                                 ListAgentsByTagResponse* response) = 0;
    virtual bool GetTagsByAgent(const GetTagsByAgentRequest& request, 
                                GetTagsByAgentResponse* response) = 0;
    //Pool
    virtual bool AddAgentToPool(const AddAgentToPoolRequest& request, 
                                AddAgentToPoolResponse* response) = 0;
    virtual bool RemoveAgentFromPool(const RemoveAgentFromPoolRequest& request, 
                                     RemoveAgentFromPoolResponse* response) = 0;
    virtual bool ListAgentsByPool(const ListAgentsByPoolRequest& request, 
                                  ListAgentsByPoolResponse* response) = 0;
    virtual bool GetPoolByAgent(const GetPoolByAgentRequest& request, 
                                GetPoolByAgentResponse* response) = 0;

    //User
    virtual bool AddUser(const AddUserRequest& request, AddUserResponse* response) = 0;
    virtual bool RemoveUser(const RemoveUserRequest& request, RemoveUserResponse* response) = 0;
    virtual bool ListUsers(const ListUsersRequest& request, ListUsersResponse* response) = 0;
    virtual bool ShowUser(const ShowUserRequest& request, ShowUserResponse* response) = 0;
    virtual bool GrantUser(const GrantUserRequest& request, GrantUserResponse* response) = 0;
    virtual bool AssignQuota(const AssignQuotaRequest& request, AssignQuotaResponse* response) = 0;

    //Preempt
    virtual bool Preempt(const PreemptRequest& request, PreemptResponse* response) = 0;

}; //end class ResourceManager

} // end namespace sdk 
} // end namespace galaxy
} // end namespace baidu

#endif  // BAIDU_GALAXY_SDK_RESMAN_H

/* vim: set ts=4 sw=4 sts=4 tw=100 */
