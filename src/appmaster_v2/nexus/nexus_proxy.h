#include <string>
#include "agent/util/error_code.h"
#include "boost/scoped_ptr.hpp"
#include "boost/thread/mutex.hpp"
#include "ins_sdk.h"

namespace baidu {
    namespace galaxy {
        namespace util {

            class NexusProxy {
                public:
                    ~NexusProxy();
                    static void Setup(const std::string& nexus_addr);
                    static boost::shared_ptr<NexusProxy> GetInstance();

                    baidu::galaxy::util::ErrorCode Register(const std::string& lock_path, 
                                const std::string& value_path,
                                const std::string& value);
                    baidu::galaxy::util::ErrorCode Put(const std::string& key, const std::string& value);
                    baidu::galaxy::util::ErrorCode Remove(const std::string& key);
                    baidu::galaxy::util::ErrorCode Scan(const std::string& start_key, 
                                const std::string& end_key,
                                std::vector<std::string>& values);

                private:
                    NexusProxy();
                    NexusProxy(const NexusProxy&);
                    explicit NexusProxy(const std::string& addr);

                    static void OnLockChanged(const ::galaxy::ins::sdk::WatchParam& param,
                                ::galaxy::ins::sdk::SDKError err);

                    static std::string s_nexus_addr;
                    static boost::shared_ptr<NexusProxy> s_instance;
                    static boost::mutex s_mutex;
                    boost::scoped_ptr< ::galaxy::ins::sdk::InsSDK> nexus_;
                    
            };
        }
    }
}
