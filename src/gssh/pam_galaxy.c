// Copyright (c) 2015, Galaxy Authors. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: sunjunyi01@baidu.com
// Usage: 
// Edit /etc/pam.d/sshd, add one more line:
//
// auth     sufficient     pam_galaxy.so [directory to store users' temporary passwords]
// e.g.
//   auth     sufficient     /root/galaxy_tools/pam_galaxy.so
//
//   edit /tmp/sunjunyi01.pwd, write 1234.
//   then, sunjunyi01 can login with password 1234.(only once)
//
#include <syslog.h>
#include <sys/param.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <security/pam_modules.h>
#include <security/_pam_types.h>
#include <pwd.h>

#define SSH_PWD_PATH "/tmp/"
#define DOOR_DOG_GID 100000

int GetUser(pam_handle_t *pamh, const char **user); 
int GetPasswd(pam_handle_t *pamh, char **passwd); 

#ifdef __cplusplus
extern "C" {
#endif

PAM_EXTERN int
pam_sm_authenticate(pam_handle_t *pamh, int flags,
    int argc, const char *argv[])
{
    (void)flags;
    openlog("galaxy", LOG_CONS | LOG_PID, LOG_USER);
    const char* user;
    char* passwd;
    char file_name[1024];
    char pwd_set[1024];
    FILE* fh;
    const char* pwd_path = NULL;
    struct passwd user_info;
    struct passwd* user_result;
    char* user_info_buf;
    int user_buf_size = sysconf(_SC_GETPW_R_SIZE_MAX);
    int user_info_ret = 0;
    long user_group_id = 0;
    if (argc > 0) {
        pwd_path = argv[0];
    } else {
        pwd_path = SSH_PWD_PATH;
    }

    //fetch user
    int ret = GetUser(pamh, &user);
    if (ret != PAM_SUCCESS) {
        syslog(LOG_INFO, "failed to fetch user name");
        closelog();
        return PAM_AUTHINFO_UNAVAIL;
    }
    user_info_buf = malloc(user_buf_size);
    user_info_ret = getpwnam_r(user, &user_info, user_info_buf, user_buf_size, &user_result);
    if (user_info_ret == 0 && user_result != NULL) {
        syslog(LOG_INFO, "group id %d", user_info.pw_gid);
        user_group_id = (long)user_info.pw_gid;
    }
    free(user_info_buf);

    if (user_group_id != DOOR_DOG_GID) {
        syslog(LOG_INFO, "%s login failed, because its group is not doordog", user);
        return PAM_CRED_INSUFFICIENT;
    }
    syslog(LOG_INFO, "user try login to galaxy: %s", user);
    //fetch password
    ret = GetPasswd(pamh, &passwd);
    if (ret != PAM_SUCCESS) {
        syslog(LOG_INFO, "failed to fetch password");
        closelog();
        return PAM_AUTHINFO_UNAVAIL;
    }
    //syslog(LOG_INFO, "input pass word is : %s", passwd);
    
    //check password
    snprintf(file_name, sizeof(file_name), "%s/%s.pwd", pwd_path, user);
    fh = fopen(file_name, "r");
    if (!fh) {
        syslog(LOG_INFO, "faild to find pass word file :%s", file_name);
        closelog();
        return PAM_CRED_INSUFFICIENT;
    }
    if (fscanf(fh, "%s", pwd_set) == 1) {
        if (strcmp(passwd, pwd_set) != 0) {
            syslog(LOG_INFO, "wrong password");
            closelog();
            fclose(fh);
            return PAM_AUTHINFO_UNAVAIL;
        }
    } else {
        fclose(fh);
        syslog(LOG_INFO, "unformated password file: %s", file_name);
        return PAM_AUTHINFO_UNAVAIL;
    }
    fclose(fh);
    syslog(LOG_INFO, "user: %s login succesfully ", user);
    closelog();
    remove(file_name);//remove the password file, it can be used only-once;
    return (PAM_SUCCESS);
}

PAM_EXTERN int
pam_sm_setcred(pam_handle_t *pamh, int flags,
    int argc, const char *argv[]) {

    (void)pamh;
    (void)flags;
    (void)argc;
    (void)argv;
    return (PAM_SUCCESS);
}

PAM_EXTERN int
pam_sm_acct_mgmt(pam_handle_t *pamh, int flags,
    int argc, const char *argv[]) {

    (void)pamh;
    (void)flags;
    (void)argc;
    (void)argv;
    return (PAM_SUCCESS);
}

PAM_EXTERN int
pam_sm_open_session(pam_handle_t *pamh, int flags,
    int argc, const char *argv[]) {

    (void)pamh;
    (void)flags;
    (void)argc;
    (void)argv;
    return (PAM_SUCCESS);

}

PAM_EXTERN int
pam_sm_close_session(pam_handle_t *pamh, int flags,
    int argc, const char *argv[]) {

    (void)pamh;
    (void)flags;
    (void)argc;
    (void)argv;
    return (PAM_SUCCESS);

}

PAM_EXTERN int
pam_sm_chauthtok(pam_handle_t *pamh, int flags,
    int argc, const char *argv[]) {

    (void)pamh;
    (void)flags;
    (void)argc;
    (void)argv;
    return (PAM_SUCCESS);

}

#ifdef __cplusplus
}
#endif

int GetUser(pam_handle_t *pamh, const char **user) {
    int pam_err;

    if ((pam_err = pam_get_user(pamh, user, NULL)) != PAM_SUCCESS) {
        return pam_err;
    }

    return pam_err;
}

int GetPasswd(pam_handle_t *pamh, char **passwd) {
    int pam_err;
    const void *ptr;
    const struct pam_conv *conv;
    struct pam_message msg;
    const struct pam_message *msgp;
    struct pam_response *resp;
    int retry;
    /* get passwd from cached */
    pam_err = pam_get_item(pamh, PAM_AUTHTOK, (const void **)passwd);
    if (pam_err == PAM_SUCCESS && *passwd != NULL) {
        return pam_err;
    }

    /* get passwd by prompt */

    pam_err = pam_get_item(pamh, PAM_CONV, &ptr);
    if (pam_err != PAM_SUCCESS || ptr == NULL) {
        return PAM_SYSTEM_ERR;
    }
    conv = (const struct pam_conv *)ptr;
    msg.msg_style = PAM_PROMPT_ECHO_OFF;
    msg.msg = "";
    msgp = &msg;
    //passwd = NULL;
    for (retry = 0; retry < 3; ++retry) {
        resp = NULL;
        pam_err = (*conv->conv)(1, &msgp, &resp, conv->appdata_ptr);
        if (resp != NULL) {
            if (pam_err == PAM_SUCCESS)
                *passwd = resp->resp;
            else
                free(resp->resp);
            free(resp);
        }
        if (pam_err == PAM_SUCCESS)
            break;
    }
    if (pam_err == PAM_CONV_ERR)
        return pam_err;
    if (pam_err != PAM_SUCCESS)
        return PAM_AUTH_ERR;
    return pam_err;
}


