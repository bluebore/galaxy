#include "nexus_proxy.h"
#include <glog/logging.h>

namespace baidu {
    namespace galaxy {
        namespace util {

            NexusProxy::NexusProxy(const std::string& addr) :
                nexus_(new ::galaxy::ins::sdk::InsSDK(addr)) {
            }

            NexusProxy::~NexusProxy() {
            }

            boost::shared_ptr<NexusProxy> NexusProxy::s_instance = boost::shared_ptr<NexusProxy>();
            std::string NexusProxy::s_nexus_addr = "";
            boost::mutex NexusProxy::s_mutex;


            void NexusProxy::Setup(const std::string& nexus_addr) {
                s_nexus_addr = nexus_addr;
            }

            boost::shared_ptr<NexusProxy> NexusProxy::GetInstance() {
                boost::mutex::scoped_lock lock(s_mutex);
                if (NULL == s_instance.get()) {
                    s_instance.reset(new NexusProxy(s_nexus_addr));
                }

                return s_instance;
            }

            baidu::galaxy::util::ErrorCode NexusProxy::Register(const std::string& lock_path, 
                        const std::string& value_path,
                        const std::string& value) {
                ::galaxy::ins::sdk::SDKError err;
                if (!nexus_->Lock(lock_path, &err) || err != ::galaxy::ins::sdk::kOK) {
                    return ERRORCODE(-1, "register nexus(%s) failed: %u",
                                lock_path.c_str(),
                                err);
                }

                if (!nexus_->Put(value_path, value, &err)) {
                    return ERRORCODE(-1, "write value %s to %s failed",
                                value_path.c_str(),
                                value.c_str()
                                );
                }

                if (!nexus_->Watch(lock_path, NexusProxy::OnLockChanged, this, &err)) {
                    return ERRORCODE(-1, "watch %s failed: %d",
                                lock_path.c_str(),
                                (int)err);
                }

                return ERRORCODE_OK;
            }

            void NexusProxy::OnLockChanged(const ::galaxy::ins::sdk::WatchParam& param,
                        ::galaxy::ins::sdk::SDKError err) {
                NexusProxy* self = (NexusProxy*)param.context;
                std::string session_id = self->nexus_->GetSessionID();
                if (session_id != param.value) {
                    LOG(FATAL) << "master lost lock, go die ...";
                }
            }



            baidu::galaxy::util::ErrorCode NexusProxy::Put(const std::string& key, const std::string& value) {
                ::galaxy::ins::sdk::SDKError err;
                if (!nexus_->Put(key, value, &err)) {
                    return ERRORCODE(-1, "put value to %s failed:", key.c_str());
                }
                return ERRORCODE_OK;

            }

            baidu::galaxy::util::ErrorCode NexusProxy::Remove(const std::string& key) {
                ::galaxy::ins::sdk::SDKError err;
                if (!nexus_->Delete(key, &err)) {
                    return ERRORCODE(-1, "remove key  %s failed:", key.c_str());
                }
                return ERRORCODE_OK;
            }

            baidu::galaxy::util::ErrorCode NexusProxy::Scan(const std::string& start_key, 
                        const std::string& end_key,
                        std::vector<std::string>& values) {
                return ERRORCODE_OK;

            }



            
        }
    }
}
